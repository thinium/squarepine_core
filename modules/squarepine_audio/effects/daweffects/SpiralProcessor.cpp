namespace djdawprocessor
{

SpiralProcessor::SpiralProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    //The wet dry in this instance should also alter the amount of feedback being used.
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

    delayUnit.setDelaySamples (200 * 48);

    lsf.setFilterType (DigitalFilter::FilterType::LSHELF);
    lsf.setFreq (1000.0f);
    lsf.setQ (0.3f);
    lsf.setAmpdB (-3.0f);
    
    setEffectiveInTimeDomain (true);

    hsf.setFilterType (DigitalFilter::FilterType::HSHELF);
    hsf.setFreq (15000.0f);
    hsf.setQ (0.3f);
    hsf.setAmpdB (-3.0f);

}

SpiralProcessor::~SpiralProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
}

//============================================================================== Audio processing
void SpiralProcessor::prepareToPlay (double Fs, int bufferSize)
{
    BandProcessor::prepareToPlay (Fs, bufferSize);

    delayUnit.setFs ((float) Fs);
    sampleRate = Fs;
    hsf.setFs(Fs);
    lsf.setFs(Fs);
    
    apf.setFeedbackAmount(0.3f);
}
void SpiralProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float wet;
    bool bypass;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        bypass = ! fxOnParam->get();
        float delayMS = timeParam->get();
        float samplesOfDelay = delayMS / 1000.f * static_cast<float> (sampleRate);
        delayUnit.setDelaySamples (samplesOfDelay);
        apf.setDelaySamples(samplesOfDelay/4.f);
    }

    if (bypass || isBypassed())
        return;

    fillMultibandBuffer (buffer);

    float feedbackAmp = 0.3f;
    
    float inputAmp = 0.f;
    for (int c = 0; c < numChannels; ++c)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            feedbackAmp = jmax (0.3f, wetSmooth[c]);
            inputAmp = jmax(1.f - wetSmooth[c],0.2f);
            float x = multibandBuffer.getWritePointer (c)[s];
            float y = (z[c] * feedbackAmp) + inputAmp * x;
            float w = delayUnit.processSample (y, c);
            w = hsf.processSample (w, c);
            w = lsf.processSample (w, c);
            //z[c] = (2.f / static_cast<float> (M_PI)) * std::atan (w * 2.f);
            z[c] = apf.processSample(w,c);
            
            multibandBuffer.getWritePointer (c)[s] = wetSmooth[c] * y;
        }
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer (c), numSamples);
    }
}

const String SpiralProcessor::getName() const { return TRANS ("Spiral"); }
/** @internal */
Identifier SpiralProcessor::getIdentifier() const { return "Spiral" + String (idNumber); }
/** @internal */
bool SpiralProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void SpiralProcessor::parameterValueChanged (int id, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.

    //Subtract the number of new parameters in this processor
    BandProcessor::parameterValueChanged (id, value);
}

}
