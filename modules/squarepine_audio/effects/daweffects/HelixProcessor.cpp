namespace djdawprocessor
{

HelixProcessor::HelixProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    //Set the ratio of sound overlay. Turn fully left to return to the original sound. Turn right from the fully left position to records input sound from its default state.
    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWet", "Dry/Wet", wetDryRange, 0.5f,
                                                                   true,// isAutomatable
                                                                   "Dry/Wet",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       int percentage = roundToInt (value * 100);
                                                                       String txt (percentage);
                                                                       return txt << "%";
                                                                   });

    auto fxon = std::make_unique<NotifiableAudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String {
        if (value > 0)
            return TRANS ("On");
        return TRANS ("Off");
        ;
    });

    NormalisableRange<float> timeRange = { 1.f, 4000.f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("time", "Time", timeRange, 500.f,
                                                                 true,// isAutomatable
                                                                 "Time ",
                                                                 AudioProcessorParameter::genericParameter,
                                                                 [] (float value, int) -> String
                                                                 {
                                                                     String txt (roundToInt (value));
                                                                     return txt;
                                                                     ;
                                                                 });

    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);

    fxOnParam = fxon.get();
    fxOnParam->addListener (this);
    
    timeParam = time.get();
    timeParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (time));
    setupBandParameters (layout);

    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
    
    delayUnit.setDelaySamples (200 * 48);
    
}

HelixProcessor::~HelixProcessor()
{
    wetDryParam->removeListener (this);
    timeParam->removeListener (this);
    fxOnParam->removeListener (this);
}

//============================================================================== Audio processing
void HelixProcessor::prepareToPlay (double Fs, int bufferSize)
{
    sampleRate = static_cast<float> (Fs);
    BandProcessor::prepareToPlay (Fs, bufferSize);
    
    delayUnit.setFs ((float) Fs);
    
    effectBuffer = AudioBuffer<float> (2 , bufferSize);
    
    #if SQUAREPINE_USE_ELASTIQUE

     const auto mode = useElastiquePro
                         ? CElastiqueProV3If::kV3Pro
                         : CElastiqueProV3If::kV3Eff;

     elastique = zplane::createElastiquePtr (bufferSize, 2, Fs, mode);

     if (elastique == nullptr)
     {
         jassertfalse; // Something failed...
     }

    elastique->Reset();
    
    
    auto pitchFactor = (float) std::clamp (1.0, 0.25, 4.0); // 2.0 = up an octave (double frequency)
    auto localRatio = (float) std::clamp (1.0, 0.01, 10.0);
    zplane::isValid (elastique->SetStretchPitchQFactor (localRatio, pitchFactor, useElastiquePro));

    outputBuffer = AudioBuffer<float> (2 , bufferSize);
    
    #endif
}
void HelixProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    float wet;
    float dry;
    bool bypass;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        dry = 1.f - wet;
        bypass = !fxOnParam->get();
        float delayMS = timeParam->get();
        float samplesOfDelay = delayMS/1000.f * sampleRate;
        delayUnit.setDelaySamples (samplesOfDelay);
    }
    
    if (bypass)
        return;
    
    fillMultibandBuffer (buffer);
    
    float pitchFactorTemp = 0.9f * pitchFactorSmooth + 0.1f * pitchFactorTarget;
    if (abs(pitchFactorSmooth - pitchFactorTemp) > 0.001f)
    {
        pitchFactorSmooth = pitchFactorTemp;
        elastique->SetStretchPitchQFactor (1.f, pitchFactorSmooth, useElastiquePro);
    }
    const auto numSamplesToRead = elastique->GetFramesNeeded (static_cast<int>(numSamples));
    
    effectBuffer.setSize (2,numSamplesToRead, false, true, true);
    
    for (int c = 0; c < numChannels ; ++c)
    {
        int index = numSamplesToRead - numSamples;
        for (int n = 0; n < numSamples ; ++n)
        {
            float x = multibandBuffer.getWritePointer(c) [n];
            multibandBuffer.getWritePointer(c) [n] = (1.f-wetSmooth[c]) * x;
            
            float y = wetSmooth[c] * x;

            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            
            effectBuffer.getWritePointer(c) [index] = y;
            
            ++index;
        }
        
    }
    
    auto inChannels = effectBuffer.getArrayOfReadPointers();
    auto outChannels = outputBuffer.getArrayOfWritePointers();
    zplane::isValid (elastique->ProcessData ((float**) inChannels, numSamplesToRead, outChannels));

    const ScopedLock sl (getCallbackLock());
    
    float feedbackAmp = 0.5;
    for (int c = 0; c < numChannels; ++c)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            float x = outputBuffer.getWritePointer (c)[s];
            float y = (z[c] * feedbackAmp) + x;
            z[c]  = delayUnit.processSample (y, c);
            outputBuffer.getWritePointer (c)[s] = y;
        }
    }
    
    for (int c = 0; c < numChannels ; ++c)
        buffer.addFrom (c,0,outputBuffer.getWritePointer(c),numSamples);
}

const String HelixProcessor::getName() const { return TRANS ("Helix"); }
/** @internal */
Identifier HelixProcessor::getIdentifier() const { return "Helix" + String (idNumber); }
/** @internal */
bool HelixProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void HelixProcessor::parameterValueChanged (int paramIndex, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.
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
            //wetDry.setTargetValue (value);
            //
            break;
        }
        case (3):
        {
        
            break;
        }
//        case (5):
//        {
//
//            pitchFactorTarget = value/100.f + 1.f;
//
//            break;
//        }
    }
}

}
