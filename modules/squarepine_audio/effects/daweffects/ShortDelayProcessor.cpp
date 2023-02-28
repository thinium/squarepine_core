namespace djdawprocessor
{


ShortDelayProcessor::ShortDelayProcessor (int idNum): idNumber (idNum)
{
    reset();
    
    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWetDelay", "Dry/Wet", wetDryRange, 1.f,
                                                                   true,// isAutomatable
                                                                   "Dry/Wet",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String {
        int percentage = roundToInt (value * 100);
        String txt (percentage);
        return txt << "%";
    });
    
    
    auto fxon = std::make_unique<AudioParameterBool> ("fxonoff", "FX On",true,
                                                                 "FX On/Off ",
                                                                 [] (bool value, int) -> String {
                                                                     if (value > 0)
                                                                         return TRANS("On");
                                                                     return TRANS("Off");
                                                                     ;
                                                                 });
    NormalisableRange<float> timeRange = { 50.f, 200.0f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("delayTime", "Delay Time", timeRange, 100.f,
                                                                 true,// isAutomatable
                                                                 "Delay Time",
                                                                 AudioProcessorParameter::genericParameter,
                                                                 [] (float value, int) -> String {
        String txt (roundToInt (value));
        return txt << "ms";
        ;
    });
    NormalisableRange<float> colourRange = { -1.f, 1.0f };
    auto colour = std::make_unique<NotifiableAudioParameterFloat> ("colour", "Colour/Tone", colourRange, 0.f,
                                                                   true,// isAutomatable
                                                                   "Colour ",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String {
        String txt (value);
        return txt;
        ;
    });
    
    NormalisableRange<float> feedbackRange = { 0.f, 1.0f };
    auto feedback = std::make_unique<NotifiableAudioParameterFloat> ("feedback", "Feedback", feedbackRange, 0.5f,
                                                                     true,// isAutomatable
                                                                     "Feedback ",
                                                                     AudioProcessorParameter::genericParameter,
                                                                     [] (float value, int) -> String {
        int percentage = roundToInt (value * 100);
        String txt (percentage);
        return txt << "%";
    });
    
    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);
    
    fxOnParam = fxon.get();
    fxOnParam->addListener(this);
    
    colourParam = colour.get();
    colourParam->addListener (this);
    
    feedbackParam = feedback.get();
    feedbackParam->addListener (this);
    
    timeParam = time.get();
    timeParam->addListener(this);
    
    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (colour));
    layout.add (std::move (feedback));
    layout.add (std::move (time));
    appendExtraParams(layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (colourParam);
    
    delayTime.setTargetValue (timeParam->get());
    wetDry.setTargetValue (0.5);
    
}

ShortDelayProcessor::~ShortDelayProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener(this);
    colourParam->removeListener (this);
    feedbackParam->removeListener (this);
    timeParam->removeListener(this);
}

//============================================================================== Audio processing
void ShortDelayProcessor::prepareToPlay (double sampleRate, int)
{
    Fs = static_cast<float> (sampleRate);
    delayUnit.setFs (Fs);
    wetDry.reset (Fs, 0.001f);
    delayTime.reset (Fs, 0.001f);
    delayUnit.setDelaySamples (delayTime.getNextValue()/1000.f * Fs);
}
void ShortDelayProcessor::processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    bool bypass;
    float feedback;
    {
        const ScopedLock sl (getCallbackLock());
        bypass = !fxOnParam->get();
        feedback = feedbackParam->get() * 0.75f; // max feedback gain is 0.75
    }
    
    if (bypass)
        return;
    
    float dry, wet, x, y;
    
    float samplesOfDelay = delayTime.getNextValue()/1000.f * Fs;
    delayUnit.setDelaySamples (samplesOfDelay);
    
    for (int s = 0; s < numSamples; ++s)
    {
        wet = wetDry.getNextValue();
        delayTime.getNextValue(); // continue smoothing
        dry = 1.f - wet;
        for (int c = 0; c < numChannels; ++c)
        {
            x = buffer.getWritePointer (c)[s];
            z[c] = delayUnit.processSample (x + feedback * z[c], c);
            y = (z[c] * wet) + (x * dry);
            buffer.getWritePointer (c)[s] = y;
        }
    }
    
}

const String ShortDelayProcessor::getName() const { return TRANS ("Short Delay"); }
/** @internal */
Identifier ShortDelayProcessor::getIdentifier() const { return "Short Delay" + String (idNumber); }
/** @internal */
bool ShortDelayProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void ShortDelayProcessor::parameterValueChanged (int paramIndex, float value)
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
        {
            wetDry.setTargetValue (value);
            //
            break;
        }
        case (3):
        {
        
            break;
        }
        case (4):
        {
            
            
            break;
        }
        case (5):
        {
            delayTime.setTargetValue (value);
            float samplesOfDelay = delayTime.getNextValue()/1000.f * Fs;
            delayUnit.setDelaySamples (samplesOfDelay);
            break;
        }
    }
    
}

}
