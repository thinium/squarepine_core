namespace djdawprocessor
{

PingPongProcessor::PingPongProcessor (int idNum)
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

    NormalisableRange<float> timeRange = { 1.f, 4000.f };
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
}

PingPongProcessor::~PingPongProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
}

//============================================================================== Audio processing
void PingPongProcessor::prepareToPlay (double sampleRate, int bufferSize)
{
    BandProcessor::prepareToPlay (sampleRate, bufferSize);
    Fs = static_cast<float> (sampleRate);
    delayLeft.setFs (Fs);
    delayRight.setFs (Fs);

    delayLeft2.setFs (Fs);
    delayRight2.setFs (Fs);

    delayTime.reset (Fs, 1.f);
    float samplesOfDelay = timeParam->get() / 1000.f * static_cast<float> (sampleRate);
    delayLeft.setDelaySamples (samplesOfDelay);
    delayRight.setDelaySamples (samplesOfDelay);
    delayLeft2.setDelaySamples (samplesOfDelay);
    delayRight2.setDelaySamples (samplesOfDelay);
}
void PingPongProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
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

    fillMultibandBuffer (buffer);

    for (int n = 0; n < numSamples; ++n)
    {
        float xL = multibandBuffer.getWritePointer (0)[n];
        float xR = multibandBuffer.getWritePointer (1)[n];
        float yL = 0.f;
        float yR = 0.f;
        getStereoDelayedSample (xL, xR, yL, yR);

        wetSmooth = 0.999f * wetSmooth + 0.001f * wet;

        multibandBuffer.getWritePointer (0)[n] = wetSmooth * yL;
        multibandBuffer.getWritePointer (1)[n] = wetSmooth * yR;

        float drySmooth = 1.f - wetSmooth;
        buffer.getWritePointer (0)[n] *= (drySmooth);
        buffer.getWritePointer (1)[n] *= (drySmooth);
    }

    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer (c), numSamples);
}

const String PingPongProcessor::getName() const { return TRANS ("Ping Pong"); }
/** @internal */
Identifier PingPongProcessor::getIdentifier() const { return "Ping Pong" + String (idNumber); }
/** @internal */
bool PingPongProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void PingPongProcessor::parameterValueChanged (int paramIndex, float value)
{
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
            //wetDry.setTargetValue (value);
            //
            break;
        }
        case (3):
        {
            float samplesOfDelay = value / 1000.f * Fs;
            delayTime.setTargetValue (samplesOfDelay);

            if (isSteppedTime)
            {
                // if we are currently using buffer1, then we change the time of buffer2 before crossfade
                if (usingDelayBuffer1)
                {
                    delayLeft2.setDelaySamples (samplesOfDelay);
                    delayRight2.setDelaySamples (samplesOfDelay);
                }
                else
                {
                    delayLeft.setDelaySamples (samplesOfDelay);
                    delayRight.setDelaySamples (samplesOfDelay);
                }

                duringCrossfade = true;// if we are using steppedTime, and a change to time has occurred, then we need to start a crossfade
                crossfadeIndex = 0;
            }

            break;
        }
    }
}

void PingPongProcessor::getStereoDelayedSample (float xL, float xR, float& yL, float& yR)
{
    if (isSteppedTime)
    {
        getDelayFromDoubleBuffer (xL, xR, yL, yR);
    }
    else
    {
        float samplesOfDelay = delayTime.getNextValue();
        delayLeft.setDelaySamples (samplesOfDelay);
        delayRight.setDelaySamples (samplesOfDelay);

        // If we are using continuous time, then we don't do the double-buffer switching
        float xSum = AMPSUMCHAN * (xL + xR);

        yL = delayLeft.processSample (xSum + feedback * z, 0);
        z = delayRight.processSample (yL, 0);
        yR = z;
    }
}

void PingPongProcessor::getDelayFromDoubleBuffer (float xL, float xR, float& yL, float& yR)
{
    if (duringCrossfade)
    {
        getDelayDuringCrossfade (xL, xR, yL, yR);
    }
    else
    {
        float ampA = 0.f, ampB = 1.f;
        if (usingDelayBuffer1)
        {
            ampA = 1.f;
            ampB = 0.f;
        }
        getDelayWithAmp (xL, xR, yL, yR, ampA, ampB);
    }
}

void PingPongProcessor::getDelayDuringCrossfade (float xL, float xR, float& yL, float& yR)
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

    getDelayWithAmp (xL, xR, yL, yR, ampA, ampB);
}

void PingPongProcessor::getDelayWithAmp (float xL, float xR, float& yL, float& yR, float ampA, float ampB)
{
    float xSum = AMPSUMCHAN * (xL + xR);

    float left1 = delayLeft.processSample (ampA * xSum + feedback * z, 0);
    z = delayRight.processSample (left1, 0);

    float left2 = delayLeft2.processSample (ampB * xSum + feedback * zUnit2, 0);
    zUnit2 = delayRight2.processSample (left2, 0);

    yL = left1 + left2;
    yR = z + zUnit2;
}

}
