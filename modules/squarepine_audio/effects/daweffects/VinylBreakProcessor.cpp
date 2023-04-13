namespace djdawprocessor
{

VinylBreakProcessor::VinylBreakProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    /*
     Sets the playback speed of the input sound. Turn fully
     left to return steadily to the original sound. Turn right
     from the fully left position to slow playback steadily,
     resulting in an effect that stops playback.
     */
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

    auto fxon = std::make_unique<NotifiableAudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String {
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
    
    // Multiplier for speed the vinyl break happens
    NormalisableRange<float> speedRange = { 0.f, 1.0f };
    auto speed = std::make_unique<NotifiableAudioParameterFloat> ("speed", "Speed", speedRange, 1.f,
                                                                  true,// isAutomatable
                                                                  "Speed ",
                                                                  AudioProcessorParameter::genericParameter,
                                                                  [] (float value, int) -> String
                                                                  {
                                                                      int percentage = roundToInt (value * 100);
                                                                      String txt (percentage);
                                                                      return txt << "%";
                                                                  });
    
    // Multiplier for speed the vinyl break happens
    NormalisableRange<float> speedRange = { 0.f, 1.0f };
    auto speed = std::make_unique<NotifiableAudioParameterFloat> ("speed", "Speed", speedRange, 1.f,
                                                                  true,// isAutomatable
                                                                  "Speed ",
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

    speedParam = speed.get();
    speedParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (speed));
    layout.add (std::move (time));
    layout.add (std::move (wetdry));
    setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (speedParam);
}

VinylBreakProcessor::~VinylBreakProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
    speedParam->removeListener (this);
}

//============================================================================== Audio processing
void VinylBreakProcessor::prepareToPlay (double sampleRate, int bufferSize)
{
    Fs = static_cast<float> (sampleRate);

    float time = timeParam->get() / 1000.f;
    float speed = jmax (speedParam->get(), 0.3f);
    alpha = std::exp (-std::log (9.f) / (Fs * time * speed));
    pitchFactorTarget = 0.f;
    pitchFactorSmooth = 1.f;

    BandProcessor::prepareToPlay (sampleRate, bufferSize);

    effectBuffer = AudioBuffer<float> (2, bufferSize);

#if SQUAREPINE_USE_ELASTIQUE

    const auto mode = useElastiquePro
                          ? CElastiqueProV3If::kV3Pro
                          : CElastiqueProV3If::kV3Eff;

    elastique = zplane::createElastiquePtr (bufferSize, 2, sampleRate, mode);

    if (elastique == nullptr)
    {
        jassertfalse;// Something failed...
    }

    elastique->Reset();

    auto pitchFactor = (float) std::clamp (1.0, 0.25, 4.0);// 2.0 = up an octave (double frequency)
    auto localRatio = (float) std::clamp (1.0, 0.01, 10.0);
    zplane::isValid (elastique->SetStretchPitchQFactor (localRatio, pitchFactor, useElastiquePro));

    outputBuffer = AudioBuffer<float> (2, bufferSize);

#endif
}
void VinylBreakProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
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
        bypass = ! fxOnParam->get();
    }

    if (bypass)
        return;

    fillMultibandBuffer (buffer);

    for (int n = 0; n < numSamples; ++n)
    {
        pitchFactorSmooth = alpha * pitchFactorSmooth + (1.f - alpha) * pitchFactorTarget;
    }
    elastique->SetStretchPitchQFactor (1.f, pitchFactorSmooth, useElastiquePro);

    const auto numSamplesToRead = elastique->GetFramesNeeded (static_cast<int> (numSamples));

    effectBuffer.setSize (2, numSamplesToRead, false, true, true);

    for (int c = 0; c < numChannels; ++c)
    {
        int index = numSamplesToRead - numSamples;
        for (int n = 0; n < numSamples; ++n)
        {
            float x = multibandBuffer.getWritePointer (c)[n];
            multibandBuffer.getWritePointer (c)[n] = (1.f - wetSmooth[c]) * x;

            float y = wetSmooth[c] * x;

            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;

            effectBuffer.getWritePointer (c)[index] = y;
            buffer.getWritePointer (c)[n] *= (1.f - wetSmooth[c]);

            ++index;
        }
    }

    auto inChannels = effectBuffer.getArrayOfReadPointers();
    auto outChannels = outputBuffer.getArrayOfWritePointers();
    zplane::isValid (elastique->ProcessData ((float**) inChannels, numSamplesToRead, (float**) outChannels));

    const ScopedLock sl (getCallbackLock());

    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, outputBuffer.getWritePointer (c), numSamples);
}

const String VinylBreakProcessor::getName() const { return TRANS ("Vinyl Break"); }
/** @internal */
Identifier VinylBreakProcessor::getIdentifier() const { return "Vinyl Break" + String (idNumber); }
/** @internal */
bool VinylBreakProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void VinylBreakProcessor::parameterValueChanged (int id, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.

    //Subtract the number of new parameters in this processor
    BandProcessor::parameterValueChanged (id, value);

    const ScopedLock sl (getCallbackLock());
    switch (id)
    {
        case (1):
        {
            // fx on/off (handled in processBlock)
            if (value < 0.5f)
            {
                pitchFactorTarget = 1.f;
                pitchFactorSmooth = 1.f;
            }
            else
            {
                pitchFactorTarget = 0.f;
                float time = timeParam->get() / 1000.f;
                float speed = jmax (speedParam->get(), 0.3f);
                alpha = std::exp (-std::log (9.f) / (Fs * time * speed));
            }
            break;
        }
        case (2):
        {
            break;
        }
        case (3):
        {
            float time = value / 1000.f;
            float speed = jmax (speedParam->get(), 0.3f);
            alpha = std::exp (-std::log (9.f) / (Fs * time * speed));

            break;
        }
        case (4):
        {
            // Speed
            if (value < 0.001)
            {
                pitchFactorTarget = 1.f;
                pitchFactorSmooth = 1.f;
            }
            else
            {
                pitchFactorTarget = 0.f;
                float time = timeParam->get() / 1000.f;
                value = jmax (value, 0.3f);
                alpha = std::exp (-std::log (9.f) / (Fs * time * value));
            }
            break;
        }
    }
}

}
