namespace djdawprocessor
{

DubEchoProcessor::DubEchoProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWetDelay", "Dry/Wet", wetDryRange, 1.f,
                                                                   true,// isAutomatable
                                                                   "Dry/Wet",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       int percentage = roundToInt (value * 100);
                                                                       String txt (percentage);
                                                                       return txt << "%";
                                                                   });

    auto fxon = std::make_unique<NotifiableAudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String
                                                                {
                                                                    if (value > 0)
                                                                        return TRANS ("On");
                                                                    return TRANS ("Off");
                                                                    ;
                                                                });

    NormalisableRange<float> timeRange = { 1.f, 4000.0f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("delayTime", "Echo Time", timeRange, 200.f,
                                                                 true,// isAutomatable
                                                                 "Delay Time",
                                                                 AudioProcessorParameter::genericParameter,
                                                                 [] (float value, int) -> String
                                                                 {
                                                                     String txt (roundToInt (value));
                                                                     return txt << "ms";
                                                                     ;
                                                                 });
    NormalisableRange<float> colourRange = { -1.f, 1.0f };
    auto colour = std::make_unique<NotifiableAudioParameterFloat> ("colour", "Colour/Tone", colourRange, 0.f,
                                                                   true,// isAutomatable
                                                                   "Colour ",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       String txt (value);
                                                                       return txt;
                                                                       ;
                                                                   });

    NormalisableRange<float> feedbackRange = { 0.f, 1.0f };
    auto feedback = std::make_unique<NotifiableAudioParameterFloat> ("feedback", "Feedback", feedbackRange, 0.5f,
                                                                     true,// isAutomatable
                                                                     "Feedback ",
                                                                     AudioProcessorParameter::genericParameter,
                                                                     [] (float value, int) -> String
                                                                     {
                                                                         int percentage = roundToInt (value * 100);
                                                                         String txt (percentage);
                                                                         return txt << "%";
                                                                     });

    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);

    fxOnParam = fxon.get();
    fxOnParam->addListener (this);

    echoColourParam = colour.get();
    echoColourParam->addListener (this);

    feedbackParam = feedback.get();
    feedbackParam->addListener (this);

    timeParam = time.get();
    timeParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (colour));
    layout.add (std::move (feedback));
    layout.add (std::move (time));
    appendExtraParams (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (echoColourParam);

    hpf.setFilterType (DigitalFilter::FilterType::HPF);
    hpf.setFreq (400.f);
    lpf.setFilterType (DigitalFilter::FilterType::LPF);
    lpf.setFreq (10000.f);

    primaryDelay = timeParam->get() / 1000.f * static_cast<float> (sampleRate);
    delayBlock.setDelaySamples (primaryDelay);
    
    phase.setFrequency (0.3f);
    
    setEffectiveInTimeDomain (true);
}

DubEchoProcessor::~DubEchoProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    echoColourParam->removeListener (this);
    feedbackParam->removeListener (this);
    timeParam->removeListener (this);
}

//============================================================================== Audio processing
void DubEchoProcessor::prepareToPlay (double Fs, int /* bufferSize */)
{
    sampleRate = Fs;
    delayBlock.setFs (static_cast<float> (sampleRate));
    hpf.setFs (sampleRate);
    lpf.setFs (sampleRate);
}

void DubEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float wet;
    float feedbackGain;
    bool bypass;
    float colour;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        feedbackGain = feedbackParam->get();
        bypass = ! fxOnParam->get();
        colour = echoColourParam->get();
    }

    if (bypass || isBypassed())
        return;

    if (abs(colour) < 0.01f)
        wet = 0.f;
    
    for (int c = 0; c < numChannels; ++c)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            float x = buffer.getWritePointer (c)[n];
            float y = x + wetSmooth[c] * feedbackSample[c];

            auto y_filter = hpf.processSample (y, c);
            y_filter = lpf.processSample (y_filter, c);
            //y_filter = hpf.processSample (y_filter, c);
            
            float lfoSample = phase.getNextSample (c);
            float depth = 10.f;
            float modDelay = static_cast<float> (depth * sin (lfoSample));
            delayBlock.setDelaySamples (primaryDelay + modDelay);
            feedbackSample[c] = gainSmooth[c] * delayBlock.processSample (y_filter, c);

            gainSmooth[c] = 0.999f * gainSmooth[c] + 0.001f * feedbackGain;
            wetSmooth[c] = 0.9999f * wetSmooth[c] + 0.0001f * wet;
            buffer.getWritePointer (c)[n] = y;
        }
    }
}

const String DubEchoProcessor::getName() const { return TRANS ("Dub Echo"); }
/** @internal */
Identifier DubEchoProcessor::getIdentifier() const { return "Dub Echo" + String (idNumber); }
/** @internal */
bool DubEchoProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void DubEchoProcessor::parameterValueChanged (int paramIndex, float value)
{
    const ScopedLock sl (getCallbackLock());
    switch (paramIndex)
    {
        case (1):
        {
            // fx on/off (handled in processBlock)
            break;
        }
        case (2):
        {// Wet/dry (handled in processBlock)
            break;
        }
        case (3):
        {//Colour
            if (value > 0.f)
            {
                float freqHz = 2.f * std::powf (10.f, value + 2.f);// 200 - 2000
                hpf.setFreq (freqHz);
                lpf.setFreq (11000.f);
            }
            else
            {
                float normValue = 1.f + value;
                float freqHz = std::powf (10.f, normValue + 3.f) + 1000.f;// 11000 -> 2000
                lpf.setFreq (freqHz);
                hpf.setFreq (200.f);
            }
            break;
        }
        case (4):
        {//feedback (handled in processBlock)
            break;
        }
        case (5):
        {//Time
            primaryDelay = value / 1000.f * static_cast<float> (sampleRate);
            
            break;
        }
    }
}

}
