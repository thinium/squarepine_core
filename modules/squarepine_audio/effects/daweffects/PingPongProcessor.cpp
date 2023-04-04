namespace djdawprocessor
{

PingPongProcessor::PingPongProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

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
                                                                     return txt << "ms";
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
    
}

PingPongProcessor::~PingPongProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
    //feedbackParam->removeListener (this);
}

//============================================================================== Audio processing
void PingPongProcessor::prepareToPlay (double sampleRate, int bufferSize)
{
    BandProcessor::prepareToPlay (sampleRate, bufferSize);
    Fs = static_cast<float> (sampleRate);
    delayLeft.setFs (Fs);
    delayRight.setFs (Fs);
    
    delayTime.reset (Fs, 0.5f);
    float samplesOfDelay = timeParam->get()/1000.f * static_cast<float> (sampleRate);
    delayLeft.setDelaySamples (samplesOfDelay);
    delayRight.setDelaySamples (samplesOfDelay);
}
void PingPongProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    float wet;
    bool bypass;
    float feedback = 0.6f;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        bypass = !fxOnParam->get();
    }
    
    if (bypass)
        return;
    
    fillMultibandBuffer (buffer);
    
    for (int n = 0; n < numSamples ; ++n)
    {
        float samplesOfDelay = delayTime.getNextValue();
        delayLeft.setDelaySamples (samplesOfDelay);
        delayRight.setDelaySamples (samplesOfDelay);
        
        float xL = multibandBuffer.getWritePointer(0) [n];
        float xR = multibandBuffer.getWritePointer(1) [n];

        float xSum = 0.7071f * (xL + xR);
        
        float dL = delayLeft.processSample (xSum + feedback * z,0);
        z = delayRight.processSample (dL,0);

        wetSmooth = 0.999f * wetSmooth + 0.001f * wet;
        
        multibandBuffer.getWritePointer(0) [n] = wetSmooth * dL;
        multibandBuffer.getWritePointer(1) [n] = wetSmooth * z;
        
        float drySmooth = 1.f - wetSmooth;
        buffer.getWritePointer (0)[n] *= (drySmooth);
        buffer.getWritePointer (1)[n] *= (drySmooth);

    }
    
    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer(c), numSamples);
}

const String PingPongProcessor::getName() const { return TRANS ("Ping Pong"); }
/** @internal */
Identifier PingPongProcessor::getIdentifier() const { return "Ping Pong" + String (idNumber); }
/** @internal */
bool PingPongProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void PingPongProcessor::parameterValueChanged (int paramIndex, float value)
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
            float samplesOfDelay = value/1000.f * Fs;
            delayTime.setTargetValue (samplesOfDelay);
            
            break;
        }
    }
}

}
