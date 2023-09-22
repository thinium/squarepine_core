
namespace djdawprocessor
{

TransEffectProcessor::TransEffectProcessor (int idNum)
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

    NormalisableRange<float> timeRange = { 10.f, 4000.f };
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
    setEffectiveInTimeDomain (true);

    phase.setFrequency (1.f / (timeParam->get() / 1000.f));
}

TransEffectProcessor::~TransEffectProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
}

//============================================================================== Audio processing
void TransEffectProcessor::prepareToPlay (double Fs, int bufferSize)
{
    BandProcessor::prepareToPlay (Fs, bufferSize);

    const ScopedLock sl (getCallbackLock());
    phase.prepare (Fs, bufferSize);
}
void TransEffectProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float wet;
    bool bypass;
    float periodOfCycle = timeParam->get() / 1000.f;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        phase.setFrequency (1.f / periodOfCycle);
        bypass = ! fxOnParam->get();
    }

    if (bypass || isBypassed())
        return;

    fillMultibandBuffer (buffer);

    double lfoSample;
    //
    double numCyclesSinceStart = effectTimeRelativeToProjectDownBeat / periodOfCycle;
    double fractionOfCycle = numCyclesSinceStart - std::floor(numCyclesSinceStart);
    float phaseInRadians = static_cast<float> (fractionOfCycle * 2.0 * M_PI);
    
    for (int c = 0; c < numChannels; ++c)
    {
        phase.setCurrentAngle(phaseInRadians,c);
        for (int n = 0; n < numSamples; ++n)
        {
            lfoSample = phase.getNextSample (c);
            float x = multibandBuffer.getWritePointer (c)[n];

            float amp = 1.f;
            if (lfoSample > M_PI)
                amp = 0.f;

            ampSmooth[c] = 0.95f * ampSmooth[c] + 0.05f * amp;

            float y = x * ampSmooth[c];

            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            multibandBuffer.getWritePointer (c)[n] = wetSmooth[c] * y;
            buffer.getWritePointer (c)[n] *= (1.f - wetSmooth[c]);
        }
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer (c), numSamples);
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
    BandProcessor::parameterValueChanged (id, value);
}

}
