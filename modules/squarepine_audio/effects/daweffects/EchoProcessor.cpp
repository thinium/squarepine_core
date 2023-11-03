
namespace djdawprocessor
{

EchoProcessor::EchoProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWet", "Dry/Wet", wetDryRange, 0.25f,
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

    float initialDelayTime = 200.f * 48.f;
    delayUnit.setDelaySamples (initialDelayTime);
    delayUnit2.setDelaySamples (initialDelayTime);
    wetDry.setTargetValue (0.25);
    delayTime.setTargetValue (initialDelayTime);

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

    delayUnit.setFs (static_cast<float> (Fs));
    delayUnit2.setFs (static_cast<float> (Fs));
    wetDry.reset (Fs, 0.5f);
    delayTime.reset (Fs, 1.f);
    setRateAndBufferSizeDetails (Fs, bufferSize);

    sampleRate = Fs;
}
void EchoProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    //TODO
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

    float wet, x, y;
    wet = wetDry.getNextValue();

    for (int s = 0; s < numSamples; ++s)
    {
        wet = wetDry.getNextValue();
        for (int c = 0; c < numChannels; ++c)
        {
            x = multibandBuffer.getWritePointer (c)[s];
            y = getDelayedSample (x, c);
            multibandBuffer.getWritePointer (c)[s] = wet * y;
            buffer.getWritePointer (c)[s] *= (1.f - wet);
        }
    }

    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer (c), numSamples);
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
            float samplesOfDelay = value / 1000.f * static_cast<float> (sampleRate);
            delayTime.setTargetValue (samplesOfDelay);

            if (isSteppedTime)
            {
                // if we are currently using buffer1, then we change the time of buffer2 before crossfade
                if (usingDelayBuffer1)
                    delayUnit2.setDelaySamples (samplesOfDelay);
                else
                    delayUnit.setDelaySamples (samplesOfDelay);

                duringCrossfade = true;// if we are using steppedTime, and a change to time has occurred, then we need to start a crossfade
                crossfadeIndex = 0;
            }

            break;
        }
    }
}

float EchoProcessor::getDelayedSample (float x, int c)
{
    if (isSteppedTime)
    {
        return getDelayFromDoubleBuffer (x, c);
    }
    else
    {
        // If we are using continuous time, then we don't do the double-buffer switching
        delayUnit.setDelaySamples (delayTime.getNextValue());
        float y = (z[c] * FEEDBACKAMP) + x;
        z[c] = delayUnit.processSample (y, c);
        return y;
    }
}

float EchoProcessor::getDelayFromDoubleBuffer (float x, int c)
{
    if (duringCrossfade)
    {
        return getDelayDuringCrossfade (x, c);
    }
    else if (usingDelayBuffer1)
    {
        float y = (z[c] * FEEDBACKAMP) + x;
        z[c] = delayUnit.processSample (y, c);
        float y2 = (zUnit2[c] * FEEDBACKAMP) + 0.f;
        zUnit2[c] = delayUnit2.processSample (y2, c);

        return y + y2;
    }
    else
    {
        // case for just using the 2nd delay buffer
        float y2 = (zUnit2[c] * FEEDBACKAMP) + x;
        zUnit2[c] = delayUnit2.processSample (y2, c);
        float y = (z[c] * FEEDBACKAMP) + 0.f;
        z[c] = delayUnit.processSample (y, c);
        return y + y2;
    }
}

float EchoProcessor::getDelayDuringCrossfade (float x, int c)
{
    float amp = static_cast<float> (crossfadeIndex) / static_cast<float> (LENGTHOFCROSSFADE);
    float ampA;
    float ampB;
    if (crossFadeFrom1to2)
    {
        ampA = 1.f - amp;
        ampB = amp;
    }
    else
    {
        // crossfade from delay buffer 2 to delay buffer 1
        ampA = amp;
        ampB = 1.f - amp;
    }
    crossfadeIndex++;
    if (crossfadeIndex == LENGTHOFCROSSFADE)
    {
        crossfadeIndex = 0;
        duringCrossfade = false;// we've reached the end of this crossfade for this time change
        crossFadeFrom1to2 = ! crossFadeFrom1to2;// next time we do a crossfade, it should be from the opposite buffers
        usingDelayBuffer1 = ! usingDelayBuffer1;
    }

    float y = (z[c] * FEEDBACKAMP) + ampA * x;
    z[c] = delayUnit.processSample (y, c);

    float y2 = (zUnit2[c] * FEEDBACKAMP) + ampB * x;
    zUnit2[c] = delayUnit2.processSample (y2, c);

    return y + y2;
}

}
