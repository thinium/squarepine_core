namespace djdawprocessor
{

float ModulatedDelay::processSample (float x, int channel)
{
    if (delay < 1.f)
    {
        return x;
    }
    else
    {
        // Delay Buffer
        // "delay" can be fraction
        int d1 = (int) floor (delay);
        int d2 = d1 + 1;
        float g2 = delay - (float) d1;
        float g1 = 1.0f - g2;

        int indexD1 = index[channel] - d1;
        if (indexD1 < 0)
        {
            indexD1 += MAX_BUFFER_SIZE;
        }

        int indexD2 = index[channel] - d2;
        if (indexD2 < 0)
        {
            indexD2 += MAX_BUFFER_SIZE;
        }

        float y = g1 * delayBuffer[indexD1][channel] + g2 * delayBuffer[indexD2][channel];

        delayBuffer[index[channel]][channel] = x;

        if (index[channel] < MAX_BUFFER_SIZE - 1)
        {
            index[channel]++;
        }
        else
        {
            index[channel] = 0;
        }

        return y;
    }
}

void ModulatedDelay::setFs (float _Fs)
{
    this->Fs = _Fs;
}

void ModulatedDelay::setDelaySamples (float _delay)
{
    if (delay >= 1.f)
    {
        delay = _delay;
    }
    else
    {
        delay = 0.f;
    }
}

void ModulatedDelay::clearDelay()
{
    for (int i = 0; i < MAX_BUFFER_SIZE; ++i)
    {
        for (int c = 0; c < 2; ++c)
        {
            delayBuffer[i][c] = 0;
        }
    }
}

float FractionalDelay::processSample (float x, int channel)
{
    smoothDelay[channel] = 0.999f * smoothDelay[channel] + 0.001f * delay;

    if (smoothDelay[channel] < 1.f)
    {
        return x;
    }
    else
    {
        // Delay Buffer
        // "delay" can be fraction
        int d1 = (int) floor (smoothDelay[channel]);
        int d2 = d1 + 1;
        float g2 = smoothDelay[channel] - (float) d1;
        float g1 = 1.0f - g2;

        int indexD1 = index[channel] - d1;
        if (indexD1 < 0)
        {
            indexD1 += MAX_BUFFER_SIZE;
        }

        int indexD2 = index[channel] - d2;
        if (indexD2 < 0)
        {
            indexD2 += MAX_BUFFER_SIZE;
        }

        float y = g1 * delayBuffer[indexD1][channel] + g2 * delayBuffer[indexD2][channel];

        delayBuffer[index[channel]][channel] = x;

        if (index[channel] < MAX_BUFFER_SIZE - 1)
        {
            index[channel]++;
        }
        else
        {
            index[channel] = 0;
        }

        return y;
    }
}

void FractionalDelay::setFs (float _Fs)
{
    this->Fs = _Fs;
}

void FractionalDelay::setDelaySamples (float _delay)
{
    if (_delay >= 1.f)
    {
        delay = _delay;
    }
    else
    {
        delay = 0.f;
    }
}

void FractionalDelay::clearDelay()
{
    for (int i = 0; i < MAX_BUFFER_SIZE; ++i)
    {
        for (int c = 0; c < 2; ++c)
        {
            delayBuffer[i][c] = 0;
        }
    }
}

float AllPassDelay::processSample (float x, int channel)
{
    float y = -feedbackAmount * x + feedbackSample[channel];
    feedbackSample[channel] = delayBlock.processSample(x + feedbackAmount * feedbackSample[channel], channel);
    return y;
}

void AllPassDelay::setFs (float _Fs)
{
    this->Fs = _Fs;
}

void AllPassDelay::setDelaySamples (float _delay)
{
    delayBlock.setDelaySamples(_delay);
}

void AllPassDelay::clearDelay()
{
    delayBlock.clearDelay();
}

DelayProcessor::DelayProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWetDelay", "Dry/Wet", wetDryRange, 0.5f,
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

    NormalisableRange<float> timeRange = { 1.f, 4000.0f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("delayTime", "Delay Time", timeRange, 200.f,
                                                                 true,// isAutomatable
                                                                 "Delay Time",
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

    setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
    setEffectiveInTimeDomain (true);
}

DelayProcessor::~DelayProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
}

//============================================================================== Audio processing
void DelayProcessor::prepareToPlay (double Fs, int bufferSize)
{
    const ScopedLock lock (getCallbackLock());
    BandProcessor::prepareToPlay (Fs, bufferSize);

    delayUnit.setFs ((float) Fs);
    wetDry.reset (Fs, 0.5f);
    delayTime.reset (Fs, 0.5f);
    setRateAndBufferSizeDetails (Fs, bufferSize);

    sampleRate = static_cast<float> (Fs);
}
void DelayProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    bool bypass;
    {
        const ScopedLock lock (getCallbackLock());
        bypass = ! fxOnParam->get();
    }

    if (bypass || isBypassed())
        return;

    fillMultibandBuffer (buffer);

    float dry, wet, x, y;
    for (int s = 0; s < numSamples; ++s)
    {
        delayUnit.setDelaySamples (delayTime.getNextValue());
        wet = wetDry.getNextValue();
        dry = 1.f - wet;
        for (int c = 0; c < numChannels; ++c)
        {
            x = multibandBuffer.getWritePointer (c)[s];
            y = delayUnit.processSample (x, c);
            multibandBuffer.getWritePointer (c)[s] = wet * y;
            buffer.getWritePointer (c)[s] *= dry;
        }
    }

    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer (c), numSamples);
}
//============================================================================== House keeping
const String DelayProcessor::getName() const { return TRANS ("Delay"); }
/** @internal */
Identifier DelayProcessor::getIdentifier() const { return "Delay" + String (idNumber); }
/** @internal */
bool DelayProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void DelayProcessor::parameterValueChanged (int paramIndex, float value)
{
    const ScopedLock sl (getCallbackLock());

    BandProcessor::parameterValueChanged (paramIndex, value);

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
            float samplesOfDelay = (value / 1000.f) * sampleRate;
            delayTime.setTargetValue (samplesOfDelay);
            break;
        }
    }
}
void DelayProcessor::releaseResources()
{
    delayUnit.clearDelay();
}

}
