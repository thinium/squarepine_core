namespace djdawprocessor
{

ShimmerProcessor::ShimmerProcessor (int idNum)
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

    NormalisableRange<float> reverbAmountRange = { 0.f, 1 };
    auto reverbAmount = std::make_unique<NotifiableAudioParameterFloat> ("amount", "Reverb Filter Amount ", reverbAmountRange, 0.5,
                                                                         true,// isAutomatable
                                                                         "Reverb Filter Amount ",
                                                                         AudioProcessorParameter::genericParameter,
                                                                         [] (float value, int) -> String
                                                                         {
                                                                             int percentage = roundToInt (value * 100);
                                                                             String txt (percentage);
                                                                             return txt << "%";
                                                                         });

    NormalisableRange<float> timeRange = { 0.f, 1.f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("time", "Time", timeRange, 0.9f,
                                                                 true,// isAutomatable
                                                                 "Time ",
                                                                 AudioProcessorParameter::genericParameter,
                                                                 [] (float value, int) -> String
                                                                 {
                                                                     String txt (round (value * 100.f) / 100.f);
                                                                     return txt;
                                                                     ;
                                                                 });

    NormalisableRange<float> otherRange = { 0.f, 1.0f };
    auto other = std::make_unique<NotifiableAudioParameterFloat> ("x Pad", "Cutoff", otherRange, 3,
                                                                  false,// isAutomatable
                                                                  "X Pad Division ",
                                                                  AudioProcessorParameter::genericParameter,
                                                                  [] (float value, int) -> String
                                                                  {
                                                                      String txt (roundToInt (value));
                                                                      return txt;
                                                                  });

    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);

    fxOnParam = fxon.get();
    fxOnParam->addListener (this);

    reverbAmountParam = reverbAmount.get();
    reverbAmountParam->addListener (this);

    timeParam = time.get();
    timeParam->addListener (this);

    xPadParam = other.get();
    xPadParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (reverbAmount));
    layout.add (std::move (time));
    layout.add (std::move (other));
    setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
}

ShimmerProcessor::~ShimmerProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    reverbAmountParam->removeListener (this);
    timeParam->removeListener (this);
    xPadParam->removeListener (this);
}

//============================================================================== Audio processing
void ShimmerProcessor::prepareToPlay (double Fs, int bufferSize)
{
    BandProcessor::prepareToPlay (Fs, bufferSize);

    pitchShifter.setFs (static_cast<float> (Fs));
    pitchShifter.setPitch (12.f);

    effectBuffer = AudioBuffer<float> (2, bufferSize);

#if SQUAREPINE_USE_ELASTIQUE

    const auto mode = useElastiquePro
                          ? CElastiqueProV3If::kV3Pro
                          : CElastiqueProV3If::kV3Eff;

    elastique = zplane::createElastiquePtr (bufferSize, 2, Fs, mode);

    if (elastique == nullptr)
    {
        jassertfalse;// Something failed...
    }

    //updateRatio();
    elastique->Reset();

    // "Reset()" also clears out the Stretch factor, so it needs to be reset

    auto pitchFactor = (float) std::clamp (2.0, 0.25, 4.0);// 2.0 = up an octave (double frequency)
    auto localRatio = (float) std::clamp (1.0, 0.01, 10.0);
    zplane::isValid (elastique->SetStretchPitchQFactor (localRatio, pitchFactor, useElastiquePro));

    outputBuffer = AudioBuffer<float> (2, bufferSize);

#endif
}
void ShimmerProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float wet;
    bool bypass;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        bypass = ! fxOnParam->get();
    }

    if (bypass || isBypassed())
        return;

    const auto numSamplesToRead = elastique->GetFramesNeeded (static_cast<int> (numSamples));

    effectBuffer.setSize (2, numSamplesToRead, false, true, true);

    fillMultibandBuffer (buffer);

    for (int c = 0; c < numChannels; ++c)
    {
        int index = numSamplesToRead - numSamples;
        for (int n = 0; n < numSamples; ++n)
        {
            float x = multibandBuffer.getWritePointer (c)[n];

            float y = wetSmooth[c] * x;

            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;

            effectBuffer.getWritePointer (c)[index] = y;
            ++index;
        }
    }

    auto inChannels = effectBuffer.getArrayOfReadPointers();
    auto outChannels = outputBuffer.getArrayOfWritePointers();
    zplane::isValid (elastique->ProcessData ((float**) inChannels, numSamplesToRead, (float**) outChannels));

    auto chans = outputBuffer.getArrayOfWritePointers();

    const ScopedLock sl (getCallbackLock());

    switch (numChannels)
    {
        case 1:
            reverb.processMono (chans[0], numSamples);
            break;

        case 2:
            reverb.processStereo (chans[0], chans[1], numSamples);
            break;

        default:
            break;
    }

    outputBuffer.applyGain (wet);
    buffer.applyGain (1.f - wet);

    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, outputBuffer.getWritePointer (c), numSamples);
}

const String ShimmerProcessor::getName() const { return TRANS ("Shimmer"); }
/** @internal */
Identifier ShimmerProcessor::getIdentifier() const { return "Shimmer" + String (idNumber); }
/** @internal */
bool ShimmerProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void ShimmerProcessor::parameterValueChanged (int id, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.
    BandProcessor::parameterValueChanged (id, value);

    Reverb::Parameters localParams;

    localParams.roomSize = timeParam->get();
    localParams.damping = 1.f - reverbAmountParam->get();
    localParams.wetLevel = 1.f;
    localParams.dryLevel = 0.f;
    localParams.width = 1;
    localParams.freezeMode = 0;

    {
        const ScopedLock sl (getCallbackLock());
        reverb.setParameters (localParams);
    }
}

}
