namespace djdawprocessor
{

PhaserProcessor::PhaserProcessor (int idNum)
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

    auto fxon = std::make_unique<AudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String
                                                      {
                                                          if (value > 0)
                                                              return TRANS ("On");
                                                          return TRANS ("Off");
                                                          ;
                                                      });

    StringArray options { "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16" };
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
    auto other = std::make_unique<NotifiableAudioParameterFloat> ("x Pad", "Modulation", otherRange, 3,
                                                                  true,// isAutomatable
                                                                  "Modulation",
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
    setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
    
    phase.setFrequency (1.f/(timeParam->get()/1000.f));
    
    apf.setFilterType (DigitalFilter::FilterType::APF);
    apf.setQ (1.f);
}

PhaserProcessor::~PhaserProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    beatParam->removeListener (this);
    timeParam->removeListener (this);
    xPadParam->removeListener (this);
}

//============================================================================== Audio processing
void PhaserProcessor::prepareToPlay (double Fs, int bufferSize)
{
    BandProcessor::prepareToPlay (Fs, bufferSize);
    const ScopedLock sl (getCallbackLock());
    apf.setFs (Fs);
    phase.prepare (Fs,bufferSize);
}
void PhaserProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
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
            
            float normLFO = 0.5f * sin(lfoSample) + 0.5f;
            float value = static_cast<float> (depthSmooth[c] * normLFO * 0.5 + 0.5);
            float freqHz = 4.f * std::powf(10.f, value + 2.f); // 400 - 4000
            apf.setFreq (freqHz);
            
            float wetSample = apf.processSample (x, c);
            
            float y = x + wetSmooth[c] * wetSample;
            
            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            depthSmooth[c] = 0.999f * depthSmooth[c] + 0.001f * depth;
            buffer.getWritePointer(c) [n] = y;
        }
    }
}

const String PhaserProcessor::getName() const { return TRANS ("Phaser"); }
/** @internal */
Identifier PhaserProcessor::getIdentifier() const { return "Phaser" + String (idNumber); }
/** @internal */
bool PhaserProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void PhaserProcessor::parameterValueChanged (int paramIndex, float /*value*/)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.

    //Subtract the number of new parameters in this processor
    BandProcessor::parameterValueChanged (id, value);
    
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
        case (4):
        {

            break; // time
        }
        case (5):
        {
            //depth = 20.0 * value;
            break; // Modulation
        }
    }
}

}
