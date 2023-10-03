
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

    auto fxon = std::make_unique<NotifiableAudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String
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

    apf1.setFilterType (DigitalFilter::FilterType::APF);
    apf1.setQ (1.f);
    apf2.setFilterType (DigitalFilter::FilterType::APF);
    apf2.setQ (1.f);
    apf3.setFilterType (DigitalFilter::FilterType::APF);
    apf3.setQ (1.f);
}

PhaserProcessor::~PhaserProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
    xPadParam->removeListener (this);
}

//============================================================================== Audio processing
void PhaserProcessor::prepareToPlay (double Fs, int bufferSize)
{
    BandProcessor::prepareToPlay (Fs, bufferSize);
    const ScopedLock sl (getCallbackLock());
    apf1.setFs (Fs);
    apf2.setFs (Fs);
    apf3.setFs (Fs);
    phase.prepare (Fs, bufferSize);
    phaseWarble.prepare (Fs, bufferSize);
}
void PhaserProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

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

    float lfoSample;
    float warbleSample;
    //
    double numCyclesSinceStart = effectTimeRelativeToProjectDownBeat / periodOfCycle;
    double fractionOfCycle = numCyclesSinceStart - std::floor(numCyclesSinceStart);
    float phaseInRadians = static_cast<float> (fractionOfCycle * 2.0 * M_PI);
    
    // effectPhaseRelativeToProjectDownBeat needs to be set once per buffer
    // based on the transport in Track::process
    
    for (int c = 0; c < numChannels; ++c)
    {
        phase.setCurrentAngle(effectPhaseRelativeToProjectDownBeat,c);
        for (int n = 0; n < numSamples; ++n)
        {
            lfoSample = phase.getNextSample (c);
            warbleSample = phaseWarble.getNextSample (c);

            float x = multibandBuffer.getWritePointer (c)[n];

            if (count < UPDATEFILTERS)
                count++;// we want to avoid re-calulating filters every sample
            else
            {
                float normLFO = 0.5f * sin (lfoSample) + 0.5f;
                float warbleLFO = .05f * warbleSmooth[c] * sin (warbleSample);
                float value = static_cast<float> (normLFO * 0.5 + 0.5 + warbleLFO);
                float freqHz = 4.f * std::powf (10.f, value + 2.f);// 400 - 4000

                apf1.setFreq (freqHz);
                apf2.setFreq (freqHz);
                apf3.setFreq (freqHz);
                count = 0;
            }

            float wetSample = apf1.processSample (x, c);
            wetSample = apf2.processSample (wetSample, c);
            wetSample = apf3.processSample (wetSample, c);
            float y = wetSmooth[c] * wetSample;

            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            warbleSmooth[c] = 0.999f * warbleSmooth[c] + 0.001f * warble;
            multibandBuffer.getWritePointer (c)[n] = y;
        }
    }

    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer (c), numSamples);
}

const String PhaserProcessor::getName() const { return TRANS ("Phaser"); }
/** @internal */
Identifier PhaserProcessor::getIdentifier() const { return "Phaser" + String (idNumber); }
/** @internal */
bool PhaserProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void PhaserProcessor::parameterValueChanged (int paramIndex, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.
    
    BandProcessor::parameterValueChanged (paramIndex, value);

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
