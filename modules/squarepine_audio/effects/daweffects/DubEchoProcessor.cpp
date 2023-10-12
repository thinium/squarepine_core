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
    hpf2.setFilterType (DigitalFilter::FilterType::HPF);
    hpf2.setFreq (400.f);
    lpf2.setFilterType (DigitalFilter::FilterType::LPF);
    lpf2.setFreq (10000.f);

    float initialDelayTime = timeParam->get() / 1000.f * static_cast<float> (sampleRate);
    delayTime.setTargetValue (initialDelayTime);
    delayBlock.setDelaySamples (initialDelayTime);
    delayUnit2.setDelaySamples (initialDelayTime);
    unit1SteppedDelayTime = initialDelayTime;
    unit2SteppedDelayTime = initialDelayTime;
    
    phase.setFrequency (lfoFreq);
    
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
    delayUnit2.setFs (static_cast<float> (sampleRate));
    delayTime.reset (Fs, 1.f);
    hpf.setFs (sampleRate);
    lpf.setFs (sampleRate);
}

void DubEchoProcessor::processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    bool bypass;
    float colour;
    {
        const ScopedLock sl (getCallbackLock());
        wetTarget = wetDryParam->get();
        feedbackTarget = feedbackParam->get();
        bypass = ! fxOnParam->get();
        colour = echoColourParam->get();
    }

    if (bypass || isBypassed())
        return;

    if (abs(colour) < 0.01f)
        wetTarget = 0.f;
    
    for (int c = 0; c < numChannels; ++c)
    {
        //phase.setCurrentAngle(effectPhaseRelativeToProjectDownBeat,c);
        for (int n = 0; n < numSamples; ++n)
        {
            float x = buffer.getWritePointer (c)[n];
            float y = getDelayedSample(x,c);

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
                hpf2.setFreq (freqHz);
                lpf2.setFreq (11000.f);
            }
            else
            {
                float normValue = 1.f + value;
                float freqHz = std::powf (10.f, normValue + 3.f) + 1000.f;// 11000 -> 2000
                lpf.setFreq (freqHz);
                hpf.setFreq (200.f);
                lpf2.setFreq (freqHz);
                hpf2.setFreq (200.f);
            }
            break;
        }
        case (4):
        {//feedback (handled in processBlock)
            break;
        }
        case (5):
        {//Time
            // primaryDelay
            float samplesOfDelay = value / 1000.f * static_cast<float> (sampleRate);
            
            delayTime.setTargetValue (samplesOfDelay);
            
            if (isSteppedTime)
            {
                // if we are currently using buffer1, then we change the time of buffer2 before crossfade
                if (usingDelayBuffer1)
                    unit2SteppedDelayTime = samplesOfDelay;
                else
                    unit1SteppedDelayTime = samplesOfDelay;
                
                duringCrossfade = true; // if we are using steppedTime, and a change to time has occurred, then we need to start a crossfade
                crossfadeIndex = 0;
            }
            
            break;
        }
    }
}


float DubEchoProcessor::getDelayedSample (float x, int c)
{
    if (isSteppedTime)
    {
        return getDelayFromDoubleBuffer (x, c);
    }
    else
    {
        // If we are using continuous time, then we don't do the double-buffer switching
        float y = x + wetSmooth[c] * feedbackSample[c];

        auto y_filter = hpf.processSample (y, c);
        y_filter = lpf.processSample (y_filter, c);
        
        float lfoSample = phase.getNextSample (c);
        float modDelay = static_cast<float> (DEPTH * sin (lfoSample));
        float sampleDelayTime = delayTime.getNextValue();
        delayBlock.setDelaySamples (sampleDelayTime + modDelay);
        feedbackSample[c] = gainSmooth[c] * delayBlock.processSample (y_filter, c);
        gainSmooth[c] = 0.999f * gainSmooth[c] + 0.001f * feedbackTarget;
        wetSmooth[c] = 0.9999f * wetSmooth[c] + 0.0001f * wetTarget;
        return y;
    }
}

float DubEchoProcessor::getDelayFromDoubleBuffer (float x, int c)
{
    if (duringCrossfade)
    {
        return getDelayDuringCrossfade (x, c);
    }
    else
    {
        
        float ampA = 0.f, ampB = 1.f;
        if (usingDelayBuffer1)
        {
            ampA = 1.f;
            ampB = 0.f;
        }
                
        return getDelayWithAmp (x, c, ampA, ampB);
    }
}

float DubEchoProcessor::getDelayDuringCrossfade (float x, int c)
{
    
    float amp = static_cast<float> (crossfadeIndex) / static_cast<float> (LENGTHOFCROSSFADE);
    float ampA;
    float ampB;
    if (crossFadeFrom1to2)
    {
        ampA = 1.f - amp;
        ampB = amp;
    }
    else
    {
        // crossfade from delay buffer 2 to delay buffer 1
        ampA = amp;
        ampB = 1.f - amp;
    }
    crossfadeIndex++;
    if (crossfadeIndex == LENGTHOFCROSSFADE)
    {
        crossfadeIndex = 0;
        duringCrossfade = false; // we've reached the end of this crossfade for this time change
        crossFadeFrom1to2 = !crossFadeFrom1to2; // next time we do a crossfade, it should be from the opposite buffers
        usingDelayBuffer1 = !usingDelayBuffer1;
    }
    
    return getDelayWithAmp (x, c, ampA, ampB);
    
}


float DubEchoProcessor::getDelayWithAmp (float x, int c, float ampA, float ampB)
{
    float lfoSample = phase.getNextSample (c);
    float modDelay = static_cast<float> (DEPTH * sin (lfoSample));
    delayBlock.setDelaySamples (unit1SteppedDelayTime + modDelay);
    delayUnit2.setDelaySamples (unit2SteppedDelayTime + modDelay);

    float y = ampA * x + wetSmooth[c] * feedbackSample[c];
    auto y_filter = hpf.processSample (y, c);
    y_filter = lpf.processSample (y_filter, c);
    feedbackSample[c] = gainSmooth[c] * delayBlock.processSample (y_filter, c);

    float y2 = ampB * x + wetSmooth[c] * feedbackUnit2[c];
    y_filter = hpf2.processSample (y2, c);
    y_filter = lpf2.processSample (y_filter, c);
    feedbackUnit2[c] = gainSmooth[c] * delayUnit2.processSample (y_filter, c);

    gainSmooth[c] = 0.999f * gainSmooth[c] + 0.001f * feedbackTarget;
    wetSmooth[c] = 0.9999f * wetSmooth[c] + 0.0001f * wetTarget;

    return y + y2;
}

}
