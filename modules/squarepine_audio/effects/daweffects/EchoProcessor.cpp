
namespace djdawprocessor
{

EchoProcessor::EchoProcessor (int idNum): InsertProcessor (true),
idNumber (idNum)
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

    
    delayUnit.setDelaySamples (200 * 48);
    wetDry.setTargetValue (0.5);
    delayTime.setTargetValue (200 * 48);
    
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

    //layout.add (std::move (feedback));

    setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
    
}

EchoProcessor::~EchoProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
}

//============================================================================== Audio processing
void EchoProcessor::prepareToPlay (double Fs, int bufferSize)
{
    BandProcessor::prepareToPlay (Fs, bufferSize);
    
    const ScopedLock lock (getCallbackLock());

    delayUnit.setFs ((float) Fs);
    wetDry.reset (Fs, 0.5f);
    delayTime.reset (Fs, 0.5f);
    setRateAndBufferSizeDetails (Fs, bufferSize);
    
    sampleRate = Fs;
}
void EchoProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    //TODO
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    bool bypass;
    float feedbackAmp = 0.6f;
    {
        const ScopedLock lock (getCallbackLock());
        bypass = !fxOnParam->get();
        //feedbackAmp = feedbackParam->get(); // appears to be constant from hardware demo
    }
    
    if (bypass)
        return;
    
    fillMultibandBuffer (buffer);
    
    float wet, x, y;
    wet = wetDry.getNextValue();
    
    for (int s = 0; s < numSamples; ++s)
    {
        wet = wetDry.getNextValue();
        delayUnit.setDelaySamples (delayTime.getNextValue());
        for (int c = 0; c < numChannels; ++c)
        {
            x = multibandBuffer.getWritePointer (c)[s];
            y = (z[c] * feedbackAmp) + x;
            z[c] = delayUnit.processSample (y, c);
            multibandBuffer.getWritePointer (c)[s] = wet * y;
            buffer.getWritePointer (c)[s] *= (1.f - wet);
        }
    }
    
    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer(c), numSamples);

}

const String EchoProcessor::getName() const { return TRANS ("Echo"); }
/** @internal */
Identifier EchoProcessor::getIdentifier() const { return "Echo" + String (idNumber); }
/** @internal */
bool EchoProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void EchoProcessor::parameterValueChanged (int paramIndex, float value)
{
    BandProcessor::parameterValueChanged (paramIndex, value);
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
            wetDry.setTargetValue (value);
            //
            break;
        }
        case (3):
        {
            float samplesOfDelay = value/1000.f * static_cast<float> (sampleRate);
            delayTime.setTargetValue (samplesOfDelay);
            break;
        }
    }
    
}

}
