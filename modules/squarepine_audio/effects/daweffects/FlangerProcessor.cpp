namespace djdawprocessor
{
FlangerProcessor::FlangerProcessor (int idNum)
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

    NormalisableRange<float> fxOnRange = { 0.f, 1.0f };

    auto fxon = std::make_unique<NotifiableAudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", true, [] (bool value, int) -> String
                                                                {
                                                                    if (value > 0)
                                                                        return TRANS ("On");
                                                                    return TRANS ("Off");
                                                                    ;
                                                                });

    NormalisableRange<float> timeRange = { 10.f, 32000.f };
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

    NormalisableRange<float> otherRange = { 0.f, 1.0f };
    auto other = std::make_unique<NotifiableAudioParameterFloat> ("x Pad", "Modulation", otherRange, 0.f,
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

    timeParam = time.get();
    timeParam->addListener (this);

    xPadParam = other.get();
    xPadParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (time));
    layout.add (std::move (other));
    setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
    setEffectiveInTimeDomain (true);

    phase.setFrequency (1.f / (timeParam->get() / 1000.f));
    phaseWarble.setFrequency (2.f);
}

FlangerProcessor::~FlangerProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
    xPadParam->removeListener (this);
}

//============================================================================== Audio processing
void FlangerProcessor::prepareToPlay (double Fs, int bufferSize)
{
    BandProcessor::prepareToPlay (Fs, bufferSize);

    const ScopedLock sl (getCallbackLock());
    phase.prepare (Fs, bufferSize);
    phaseWarble.prepare (Fs, bufferSize);
    delayBlock.setFs (static_cast<float> (Fs));
}
void FlangerProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float depth = 10.f;
    float wet;
    bool bypass;
    float warble;
    float periodOfCycle = timeParam->get() / 1000.f;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        phase.setFrequency (1.f / periodOfCycle);
        bypass = ! fxOnParam->get();
        warble = xPadParam->get();
    }

    if (bypass || isBypassed())
        return;

    fillMultibandBuffer (buffer);

    double lfoSample;
    double warbleSample;
    double numCyclesSinceStart = effectTimeRelativeToProjectDownBeat / periodOfCycle;
    double fractionOfCycle = numCyclesSinceStart - std::floor(numCyclesSinceStart);
    float phaseInRadians = static_cast<float> (fractionOfCycle * 2.0 * M_PI);
    
    for (int c = 0; c < numChannels; ++c)
    {
        phase.setCurrentAngle(phaseInRadians,c);
        for (int n = 0; n < numSamples; ++n)
        {
            lfoSample = phase.getNextSample (c);
            warbleSample = phaseWarble.getNextSample (c);
            float x = multibandBuffer.getWritePointer (c)[n];

            float offset = depth + 5.f;
            float warbleLFO = static_cast<float> (2.f * warbleSmooth[c] * sin (warbleSample));
            float delayTime = static_cast<float> (depth * sin (lfoSample) + offset + warbleLFO);

            delayBlock.setDelaySamples (delayTime);

            float wetSample = delayBlock.processSample (x, c);

            float y = wetSmooth[c] * wetSample;

            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            warbleSmooth[c] = 0.999f * warbleSmooth[c] + 0.001f * warble;
            multibandBuffer.getWritePointer (c)[n] = y;
        }
    }
    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer (c), numSamples);
}

const String FlangerProcessor::getName() const { return TRANS ("Flanger"); }
/** @internal */
Identifier FlangerProcessor::getIdentifier() const { return "Flanger" + String (idNumber); }
/** @internal */
bool FlangerProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void FlangerProcessor::parameterValueChanged (int paramIndex, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.
    BandProcessor::parameterValueChanged (paramIndex, value);

    //Subtract the number of new parameters in this processor
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
            break;// time
        }
        case (4):
        {
            break;// Modulation
        }
    }
}

}
