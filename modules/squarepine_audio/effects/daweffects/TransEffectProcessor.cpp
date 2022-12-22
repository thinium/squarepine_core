namespace djdawprocessor
{

TransEffectProcessor::TransEffectProcessor (int idNum): idNumber (idNum)
{
    reset();
    
    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWet", "Dry/Wet", wetDryRange, 0.5f,
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
    
    StringArray options{"1/16","1/8","1/4","1/2","1","2","4","8","16"};
    auto beat = std::make_unique<AudioParameterChoice> ("beat", "Beat Division", options, 3);

    NormalisableRange<float> timeRange = { 10.f, 32000.f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("time", "Time", timeRange, 10.f,
                                                                 true,// isAutomatable
                                                                 "Time ",
                                                                 AudioProcessorParameter::genericParameter,
                                                                 [] (float value, int) -> String {
                                                                     String txt (roundToInt (value));
                                                                     return txt << "ms";
                                                                     ;
                                                                 });

    NormalisableRange<float> otherRange = { 0.f, 1.0f };
    auto other = std::make_unique<NotifiableAudioParameterFloat> ("x Pad", "X Pad Division", otherRange,1.f,
                                                                  true,// isAutomatable
                                                                  "X Pad Division ",
                                                                  AudioProcessorParameter::genericParameter,
                                                                  [] (float value, int) -> String {
        int percentage = roundToInt (value * 100);
        String txt (percentage);
        return txt << "%";
    });
    
    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);
    
    fxOnParam = fxon.get();
    fxOnParam->addListener (this);
    
    beatParam = beat.get();
    beatParam->addListener (this);

    timeParam = time.get();
    timeParam->addListener (this);

    xPadParam = other.get();
    xPadParam->addListener (this);
    
    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (beat));
    layout.add (std::move (time));
    layout.add (std::move (other));
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));
    
    setPrimaryParameter (wetDryParam);
    
    phase.setFrequency (1.f/(timeParam->get()/1000.f));
}

TransEffectProcessor::~TransEffectProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    beatParam->removeListener (this);
    timeParam->removeListener (this);
    xPadParam->removeListener (this);
}

//============================================================================== Audio processing
void TransEffectProcessor::prepareToPlay (double Fs, int bufferSize)
{
    const ScopedLock sl (getCallbackLock());
    phase.prepare (Fs,bufferSize);
}
void TransEffectProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    float wet;
    bool bypass;
    float depth;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        phase.setFrequency (1.f/(timeParam->get()/1000.f));
        bypass = !fxOnParam->get();
        depth = xPadParam->get();
    }
    
    if (bypass)
        return;
    
    double lfoSample;
    //
    for (int c = 0; c < numChannels ; ++c)
    {
        for (int n = 0; n < numSamples ; ++n)
        {
            lfoSample = phase.getNextSample(c);
            float x = buffer.getWritePointer(c) [n];
            
            float amp = 1.f;
            if (lfoSample > M_PI)
                amp = 0.f;
            
            ampSmooth[c] = 0.95f * ampSmooth[c] + 0.05f * amp;
            
            float offset = 1.f - depthSmooth[c];
            float combinedAmp = static_cast<float> (depthSmooth[c] * ampSmooth[c] + offset);
            
            float wetSample = x * combinedAmp;
            
            float y = (1.f - wetSmooth[c]) * x + wetSmooth[c] * wetSample;
            
            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            depthSmooth[c] = 0.999f * depthSmooth[c] + 0.001f * depth;
            buffer.getWritePointer(c) [n] = y;
        }
    }
}

const String TransEffectProcessor::getName() const { return TRANS ("Trans"); }
/** @internal */
Identifier TransEffectProcessor::getIdentifier() const { return "Trans" + String (idNumber); }
/** @internal */
bool TransEffectProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void TransEffectProcessor::parameterValueChanged (int id, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.
    
    //Subtract the number of new parameters in this processor
    //BandProcessor::parameterValueChanged (id, value);
}

}
