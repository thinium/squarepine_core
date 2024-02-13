//
//  LimiterProcessor.cpp
//
namespace djdawprocessor
{

LimiterProcessor::LimiterProcessor()
{
    setKnee (1.5f);
    setInputGain (6.f);
    setTruePeakOn (true);
    setOversampling (true);
    setOverSamplingLevel (2);
    setEnhanceAmount (0);
    setThreshold (-3.0f);
}

void LimiterProcessor::processBuffer (AudioBuffer<float>& buffer)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    bypassDelay (buffer, bypassBuffer, numChannels, numSamples);

    // Input Gain
    //if (inputGain < 1)
    //    int test = 1;
    
    applyInputSmoothGain (buffer, inputGain, inputGainSmooth);
    //applyInputSmoothGain (lookaheadBuffer, inputGain, lookaheadGainSmooth); // scale

    inputMeterValue = getPeakMeterValue (buffer, true);// should this be before input gain?

    if (bypassed)
    {
        for (int c = 0; c < numChannels; ++c)
            buffer.copyFrom (c, 0, bypassBuffer.getWritePointer (c), numSamples);

        return;
    }

    combinedLinearGR = 1.0f;// Reset for each buffer, so it can be referenced for meters
    autoCompGR = 1.0f;// updated in processAutoComp function
    limiterGR = 1.0f;

    if (autoCompIsOn)
    {
        autoCompDelay (buffer, autoCompBuffer, numChannels, numSamples);
        processAutoComp (buffer, autoCompBuffer, numChannels, numSamples);
    }
        
    lookaheadDelay (buffer, lookaheadBuffer, numChannels, numSamples);

    truePeakAnalysis.fillTruePeakFrameBuffer (buffer, truePeakFrameBuffer, numChannels, numSamples, true);

    if (numChannels == 2)
    {
        if (overSamplingOn)
        {
            float* channelData = nullptr;
            float* osChannelData = nullptr;
            for (int c = 0; c < numChannels; ++c)
            {
                channelData = lookaheadBuffer.getWritePointer (c);
                osChannelData = upbuffer.getWritePointer (c);
                upsampling.process (channelData, osChannelData, c, numSamples);
            }

            float xL;
            float xR;
            float yL;
            float yR;
            for (int i = 0; i < numSamples; ++i)
            {
                float detectSample = truePeakFrameBuffer.getWritePointer (0)[i];
                for (int j = 0; j < OSFactor; ++j)
                {
                    int index = OSFactor * i + j;
                    xL = upbuffer.getWritePointer (0)[index];
                    xR = upbuffer.getWritePointer (1)[index];

                    processStereoSample (xL, xR, detectSample, yL, yR);

                    upbuffer.getWritePointer (0)[index] = yL;
                    upbuffer.getWritePointer (1)[index] = yR;
                }
            }

            for (int c = 0; c < numChannels; ++c)
            {
                channelData = lookaheadBuffer.getWritePointer (c);
                osChannelData = upbuffer.getWritePointer (c);
                downsampling.process (osChannelData, channelData, c, buffer.getNumSamples());
            }
        }
        else
        {
            float xL;
            float xR;
            float yL;
            float yR;
            for (int i = 0; i < numSamples; ++i)
            {
                float detectSample = truePeakFrameBuffer.getWritePointer (0)[i];
                xL = lookaheadBuffer.getWritePointer (0)[i];
                xR = lookaheadBuffer.getWritePointer (1)[i];

                processStereoSample (xL, xR, detectSample, yL, yR);

                lookaheadBuffer.getWritePointer (0)[i] = yL;
                lookaheadBuffer.getWritePointer (1)[i] = yR;
            }
        }
    }
    else
    {
        float x;
        float y;
        int c = 0;
        for (int i = 0; i < numSamples; ++i)
        {
            x = lookaheadBuffer.getWritePointer (c)[i];
            float detectSample = truePeakFrameBuffer.getWritePointer (0)[i];
            y = processSample (x, detectSample, c);
            lookaheadBuffer.getWritePointer (c)[i] = y;
        }
    }

    if (enhanceIsOn)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                float sample = lookaheadBuffer.getWritePointer (c)[n];
                lookaheadBuffer.getWritePointer (c)[n] = enhanceProcess (sample);
            }
        }
    }

    // Constant Gain Monitoring (on/off) is handled internally
    applyOutputSmoothGain (lookaheadBuffer, inputGain, inputInvSmooth);
    

    for (int c = 0; c < numChannels; ++c)
        buffer.copyFrom (c, 0, lookaheadBuffer.getWritePointer (c), numSamples);

    if (truePeakIsOn)
    {
        truePeakPostAnalysis.fillTruePeakFrameBuffer (buffer, truePeakPostBuffer, numChannels, numSamples, true);

        float truePeakPostMax = truePeakPostBuffer.getMagnitude (0, numSamples);
        float truePeakTargetGain = jmin (ceilingLinearThresh / truePeakPostMax, 1.f);
        applyTruePeakGain (buffer, truePeakTargetGain, truePeakGain);
    }

    outputMeterValue = getPeakMeterValue (buffer, false);// should this be before input gain?

    combinedLinearGR = autoCompGR * limiterGR;// store one value per buffer for the GR meter
}

void LimiterProcessor::processStereoSample (float xL, float xR, float detectSample, float& yL, float& yR)
{
    float xUni = std::abs (detectSample);// Convert bi-polar signal to uni-polar on decibel scale
    float x_dB = 20.0f * std::log10 (xUni);
    if (x_dB < -144.f)
    {
        x_dB = -144.f;
    }// Prevent -Inf values

    float gainSC;// This variable calculates the "desired" output level based on static characteristics
    if (x_dB > thresh + knee / 2.0f)
    {
        gainSC = thresh + knee / 2.0f;// + (x_dB - thresh) / ratio;
    }
    else if (x_dB > (thresh - knee / 2.0f))
    {
        gainSC = x_dB + ((1.f / ratio - 1.f) * std::pow (x_dB - thresh + knee / 2.f, 2.f)) / (2.f * knee);
    }
    else
    {
        gainSC = x_dB;
    }

    float gainChange_dB = gainSC - x_dB;

    // First-order smoothing filter to control attack and release response time
    float gainSmooth;
    if (gainChange_dB < gainSmoothPrev[0])
    {// Attack mode
        gainSmooth = (1.f - alphaA) * gainChange_dB + alphaA * gainSmoothPrev[0];
    }
    else
    {// Release mode
        gainSmooth = (1.f - alphaR) * gainChange_dB + alphaR * gainSmoothPrev[0];
    }

    gainSmoothPrev[0] = gainSmooth;// Save for the next loop (i.e. delay sample in smoothing filter)

    linA = std::pow (10.f, gainSmooth / 20.f);// Determine linear amplitude

    yL = linA * xL;// Apply to input signal for left channel
    yR = linA * xR;// Apply to input signal for right channel

    if (linA < limiterGR)
        limiterGR = linA;// Keep one value per buffer that represents the maximum amout of GR

    outputGainSmooth = 0.999f * outputGainSmooth + 0.001f * outputGain;
    yL *= outputGainSmooth;
    yR *= outputGainSmooth;

    auto leftReduction = applyCeilingReduction (yL, true);
    auto rightReduction = applyCeilingReduction (yR, false);

    auto extraReduction = jmax (leftReduction, rightReduction);
    gainReduction = linA + extraReduction;
}
float LimiterProcessor::applyCeilingReduction (float& value, bool isLeft)
{
    if (value > ceiling)
    {
        float additionalDelta = 20 * std::log10 (value / ceiling);
        if (isLeft)
        {
            additionalGainReductionL = std::powf (10.0f, (-additionalDelta) / 20.0f);
            additionalGainReductionSmoothL = 0.999f * additionalGainReductionSmoothL + 0.001f * additionalGainReductionL;
            value *= additionalGainReductionSmoothL;
            return additionalGainReductionSmoothL;
        }
        else
        {
            additionalGainReductionR = std::powf (10.0f, (-additionalDelta) / 20.0f);
            additionalGainReductionSmoothR = 0.999f * additionalGainReductionSmoothR + 0.001f * additionalGainReductionR;
            value *= additionalGainReductionSmoothR;
            return additionalGainReductionSmoothR;
        }
    }
    return 0.f;
}

float LimiterProcessor::processSample (float x, float detectSample, int channel)
{
    // Output variable
    float y = 0;

    float xUni = std::abs (detectSample);// Convert bi-polar signal to uni-polar on decibel scale
    float x_dB = 20.0f * std::log10 (xUni);
    if (x_dB < -144.f)
    {
        x_dB = -144.f;
    }// Prevent -Inf values

    float gainSC;// This variable calculates the "desired" output level based on static characteristics
    if (x_dB > thresh + knee / 2.0f)
    {
        gainSC = thresh + knee / 2.0f;// + (x_dB - thresh) / ratio;
    }
    else if (x_dB > (thresh - knee / 2.0f))
    {
        gainSC = x_dB + ((1.f / ratio - 1.f) * std::pow (x_dB - thresh + knee / 2.f, 2.f)) / (2.f * knee);
    }
    else
    {
        gainSC = x_dB;
    }

    float gainChange_dB = gainSC - x_dB;

    // First-order smoothing filter to control attack and release response time
    float gainSmooth;
    if (gainChange_dB < gainSmoothPrev[channel])
    {// Attack mode
        gainSmooth = (1.f - alphaA) * gainChange_dB + alphaA * gainSmoothPrev[channel];
    }
    else
    {// Release mode
        gainSmooth = (1.f - alphaR) * gainChange_dB + alphaR * gainSmoothPrev[channel];
    }

    gainSmoothPrev[channel] = gainSmooth;// Save for the next loop (i.e. delay sample in smoothing filter)

    linA = std::pow (10.f, gainSmooth / 20.f);// Determine linear amplitude

    y = linA * x;// Apply to input signal

    if (linA < limiterGR)
        limiterGR = linA;// Keep one value per buffer that represents the maximum amout of GR

    //if (enhanceIsOn)
    //    y = enhanceProcess (y);

    return y * outputGain;
}

void LimiterProcessor::lookaheadDelay (AudioBuffer<float>& buffer, AudioBuffer<float>& delayedBuffer, const int numChannels, const int numSamples)
{
    float x;
    for (int c = 0; c < numChannels; ++c)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            x = buffer.getWritePointer (c)[n];
            lookahead[indexLAWrite[c]][c] = x;
            delayedBuffer.getWritePointer (c)[n] = lookahead[indexLARead[c]][c];

            indexLAWrite[c]++;
            if (indexLAWrite[c] >= LASIZE)
                indexLAWrite[c] = 0;

            indexLARead[c]++;
            if (indexLARead[c] >= LASIZE)
                indexLARead[c] = 0;
        }
    }
}

void LimiterProcessor::autoCompDelay (AudioBuffer<float>& buffer, AudioBuffer<float>& delayedBuffer, const int numChannels, const int numSamples)
{
    float x;
    for (int c = 0; c < numChannels; ++c)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            x = buffer.getWritePointer (c)[n];
            autoCompArray[indexACWrite[c]][c] = x;
            delayedBuffer.getWritePointer (c)[n] = autoCompArray[indexACRead[c]][c];

            indexACWrite[c]++;
            if (indexACWrite[c] >= LASIZE)
                indexACWrite[c] = 0;

            indexACRead[c]++;
            if (indexACRead[c] >= LASIZE)
                indexACRead[c] = 0;
        }
    }
}

void LimiterProcessor::bypassDelay (AudioBuffer<float>& buffer, AudioBuffer<float>& delayedBuffer, const int numChannels, const int numSamples)
{
    float x;
    for (int c = 0; c < numChannels; ++c)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            x = buffer.getWritePointer (c)[n];
            bypassArray[indexBYWrite[c]][c] = x;
            delayedBuffer.getWritePointer (c)[n] = bypassArray[indexBYRead[c]][c];

            indexBYWrite[c]++;
            if (indexBYWrite[c] >= LASIZE)
                indexBYWrite[c] = 0;

            indexBYRead[c]++;
            if (indexBYRead[c] >= LASIZE)
                indexBYRead[c] = 0;
        }
    }
}

float LimiterProcessor::outputGainDelay (float x)
{

    outputGainArray[indexOGWrite] = x;
    float delayedValue = outputGainArray[indexOGRead];

    indexOGWrite++;
    if (indexOGWrite >= LASIZE)
        indexOGWrite = 0;

    indexOGRead++;
    if (indexOGRead >= LASIZE)
        indexOGRead = 0;

    return delayedValue;
    
}

void LimiterProcessor::setOversampling (bool isOn)
{
    if (overSamplingOn != isOn)
    {
        overSamplingOn = isOn;
        reset();
        if (isOn)
        {
            upsampling.prepare (OSFactor, OSQuality);
            downsampling.prepare (OSFactor, OSQuality);
        }
        setAttack (attack);
        setRelease (release);
    }
}

void LimiterProcessor::setOverSamplingLevel (int level)
{
    // Level (from ComboBox dropdown) = 0 (none), 1 (2x), 2 (4x), 3 (8x)
    // OSFactor (internal parameter for oversampling) = 1 (none), 2 (2x), 4 (4x), 8 (8x)
    int newOSFactor = static_cast<int> (pow (2.f, static_cast<float> (level)));
    if (OSFactor != newOSFactor)
    {
        OSFactor = newOSFactor;
        reset();
        //if (overSamplingOn)
        //{
        upsampling.prepare (OSFactor, OSQuality);
        downsampling.prepare (OSFactor, OSQuality);
        //}
        setAttack (attack);
        setRelease (release);
    }
}

void LimiterProcessor::setOfflineOS (bool isOn)
{
    offlineOSOn = isOn;
}

void LimiterProcessor::prepare (float sampleRate, int bufferSize)
{
    Fs = sampleRate;
    samplesPerBuffer = bufferSize;

    // Variables to set if Fs changes
    setAttack (attack);
    setRelease (release);
    truePeakFrameBuffer = AudioBuffer<float> (1, bufferSize);
    truePeakPostBuffer = AudioBuffer<float> (1, bufferSize);

    lookaheadBuffer = AudioBuffer<float> (2, bufferSize);// (numChannels, numSamples)
    bypassBuffer = AudioBuffer<float> (2, bufferSize);// (numChannels, numSamples)
    autoCompBuffer = AudioBuffer<float> (2, bufferSize);// (numChannels, numSamples)

    const int MAX_OS_LEVEL = 8;
    upbuffer = AudioBuffer<float> (2, bufferSize * MAX_OS_LEVEL);// (numChannels, numSamples)
    upsampling.prepare (OSFactor, OSQuality);
    downsampling.prepare (OSFactor, OSQuality);

    int lookAheadSamples = 8;
    indexLAWrite[0] = lookAheadSamples;
    indexLAWrite[1] = lookAheadSamples;
    indexLARead[0] = 0;
    indexLARead[1] = 0;

    int tempIndex = static_cast<int> (round (Fs * .1f));// Write should be ahead of Read when indexing buffer
    indexACWrite[0] = tempIndex;
    indexACWrite[1] = tempIndex;
    indexACRead[0] = 0;
    indexACRead[1] = 0;
    
    indexBYWrite[0] = lookAheadSamples;
    indexBYWrite[1] = lookAheadSamples;
    indexBYRead[0] = 0;
    indexBYRead[1] = 0;

    indexOGWrite = lookAheadSamples;
    indexOGRead = 0;
    
    // Meter rise and fall values similar to many DAWs (logic, ableton, pro tools)
    meterAttack = std::exp (-log (9.f) / (Fs * 0.01f));
    meterRelease = std::exp (-log (9.f) / (Fs * 0.35f));
}

void LimiterProcessor::setThreshold (float threshold)
{
    // used for true peak hard limit, does not factor in knee, -.3 to ensure true peak stays below limit
    ceilingLinearThresh = pow (10.f, (threshold - 0.3f) / 20.f);
    thresh = jlimit (-64.0f, 0.0f, (threshold - 0.3f) - knee / 2.f);// accounts for half of knee above thresh for limit level
    linThresh = pow (10.f, thresh / 20.f);
}

float LimiterProcessor::getThreshold()
{
    return thresh;
}

void LimiterProcessor::setRatio (float rat)
{
    ratio = jlimit (1.0f, 100.0f, rat);
}

float LimiterProcessor::getRatio()
{
    return ratio;
}

void LimiterProcessor::setKnee (float kne)
{
    knee = jlimit (0.0f, 10.0f, kne);
}

float LimiterProcessor::getKnee()
{
    return knee;
}

void LimiterProcessor::setAttack (float att)
{
    //attack = jlimit (0.001f, 0.5f, att);
    attack = att * 0.1f; // factor in lookahead
    // Convert time in seconds to a coefficient value for the smoothing filter
    if (overSamplingOn)
        alphaA = expf (-logf (9.0f) / (Fs * static_cast<float> (OSFactor) * attack));
    else
        alphaA = expf (-logf (9.0f) / (Fs * attack));
}

float LimiterProcessor::getAttack()
{
    return attack;
}

void LimiterProcessor::setRelease (float rel)
{
    release = jlimit (0.01f, 1.0f, rel);
    if (overSamplingOn)
        alphaR = expf (-logf (9.0f) / (Fs * static_cast<float> (OSFactor) * release));
    else
        alphaR = expf (-logf (9.0f) / (Fs * release));
}

float LimiterProcessor::getRelease()
{
    return release;
}

void LimiterProcessor::setInputGain (float inputGain_dB)
{
    inputGain = std::pow (10.f, inputGain_dB / 20.f);
}

float LimiterProcessor::getInputGain()
{
    return 20.f * log10 (inputGain);
}

void LimiterProcessor::setOutputGain (float outputGain_dB)
{
    outputGain = std::pow (10.f, outputGain_dB / 20.f);
}

float LimiterProcessor::getOutputGain()
{
    return 20.f * log10 (outputGain);
}

float LimiterProcessor::getGainReduction (bool linear)
{
    if (linear)
        return combinedLinearGR;
    // This method can be find the amount of gain reduction at a given time
    // if the interface has a meter or display.
    return 20.f * log10 (combinedLinearGR);
}

float LimiterProcessor::enhanceProcess (float x)
{
    float y;
    x = (0.9f / linThresh) * x;
    
    if (x > 1) {
        y = 1;
    }
    else if (x < -1) {
        y = -1;
    }
    else { y = (3.f/2.f) * (x - (1.f/3.f) * std::powf(x,3.f)); }
    
    y *= linThresh;
    
    return y;
}

void LimiterProcessor::applyInputSmoothGain (AudioBuffer<float>& buffer, float targetGain, float& smoothGain)
{
    //const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int n = 0; n < numSamples; ++n)
    {
        smoothGain = 0.999f * smoothGain + 0.001f * targetGain;
        buffer.getWritePointer (0)[n] *= smoothGain;
        buffer.getWritePointer (1)[n] *= smoothGain;
    }
}

void LimiterProcessor::applyOutputSmoothGain (AudioBuffer<float>& buffer, float targetGain, float& smoothGain)
{
    // This makes sure that the output smoothed gain will
    // give a complementary compensation for the input gain, regardless of its settings when CGM is turned on
    const int numSamples = buffer.getNumSamples();

    for (int n = 0; n < numSamples; ++n)
    {
        smoothGain = 0.999f * smoothGain + 0.001f * targetGain; // always smooth gain so it exactly follows the input gain
        float delayedGain = outputGainDelay(smoothGain); // need to delay to factor in lookahead latency
        if (constantGainMonitoring)
        {
            // only apply the gain if constant gain monitoring is on
            float outputSmoothGain = 1.f/delayedGain; // output applies the inverse gain
            buffer.getWritePointer (0)[n] *= outputSmoothGain;
            buffer.getWritePointer (1)[n] *= outputSmoothGain;
        }
    }
}

void LimiterProcessor::applyTruePeakGain (AudioBuffer<float>& buffer, float targetGain, float& smoothGain)
{
    //const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    for (int n = 0; n < numSamples; ++n)
    {
        smoothGain = 0.9f * smoothGain + 0.1f * targetGain;
        buffer.getWritePointer (0)[n] *= smoothGain;
        buffer.getWritePointer (1)[n] *= smoothGain;
    }
}

float LimiterProcessor::getPeakMeterValue (AudioBuffer<float>& buffer, bool isInput)
{
    //const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float peakValue = 0.f;
    float smoothedValue;
    if (isInput)
        smoothedValue = std::pow (10.f, inputMeterValue.load() / 20.f);// smooth using linear value
    else
        smoothedValue = std::pow (10.f, outputMeterValue.load() / 20.f);

    float g = 0.9f;
    for (int n = 0; n < numSamples; ++n)
    {
        peakValue = jmax (abs (buffer.getWritePointer (0)[n]), abs (buffer.getWritePointer (1)[n]));
        if (peakValue > smoothedValue)
            g = meterAttack;// Meter Rising
        else
            g = meterRelease;// Meter Falling

        smoothedValue = (1.f - g) * peakValue + g * smoothedValue;
    }

    smoothedValue = jmax (smoothedValue, pow (10.f, METERFLOORVALUE / 20.f));// make sure we have a value that won't be -infinity dB

    return 20.f * log10 (smoothedValue);// convert back to dB
}

void LimiterProcessor::processAutoComp (AudioBuffer<float>& buffer, AudioBuffer<float>& delayedBuffer, const int numChannels, const int numSamples)
{
    if (numChannels != 2)
        return;

    // This block of processing is based on the lookahead compression of the Oxford Limiter.
    // When I took some measurements of the plug-in with the AutoComp setting "on", I noticed
    // that there was some very subtle gain reduction that was introduced prior to the point
    // hard limiting occurred (for a step input signal). It was difficult/impossible to tell
    // exactly what the parameters (thresh,ratio,etc) are of this extra stage of compression.
    // So I experimented with very subtle settings to get something that worked reasonably well.

    // "acThresh" and "acRatio" were chosen so that the auto-comp applies a gradual amount of
    // gain reduction prior to limiting.
    float acThresh = thresh - knee;
    float acRatio = 2.0f;// 2:1 ratio
    // the lookahead buffer  is  delayed  by .1 sec, so I chose that for the attack time
    float acAlphaA = expf (-logf (9.0f) / (Fs * .1f));
    float acAlphaR = expf (-logf (9.0f) / (Fs * .3f));// slightly longer release for smoothness

    // For the AutoComp, the original "buffer" is used for the sidechain analysis, and the result
    // is applied to the delayedBuffer so that the compressor gradually decreases amplitude prior
    // to the peaks occurring

    for (int n = 0; n < numSamples; ++n)
    {
        float xL = buffer.getWritePointer (0)[n];
        float xR = buffer.getWritePointer (1)[n];
        float detectSample = std::sqrtf (0.5f * (std::powf (xL, 2.0f) + std::powf (xR, 2.0f)));

        float xUni = std::abs (detectSample);// Convert bi-polar signal to uni-polar on decibel scale
        float x_dB = 20.0f * std::log10 (xUni);
        if (x_dB < -144.f)
        {
            x_dB = -144.f;
        }// Prevent -Inf values

        float gainSC;// This variable calculates the "desired" output level based on static characteristics

        if (x_dB > acThresh)
        {
            gainSC = acThresh + (x_dB - acThresh) / acRatio;
        }
        else
        {
            gainSC = x_dB;
        }

        float gainChange_dB = gainSC - x_dB;

        // First-order smoothing filter to control attack and release response time
        float gainSmooth;
        if (gainChange_dB < acGainSmoothPrev)
        {// Attack mode
            gainSmooth = (1.f - acAlphaA) * gainChange_dB + acAlphaA * acGainSmoothPrev;
        }
        else
        {// Release mode
            gainSmooth = (1.f - acAlphaR) * gainChange_dB + acAlphaR * acGainSmoothPrev;
        }

        acGainSmoothPrev = gainSmooth;// Save for the next loop (i.e. delay sample in smoothing filter)

        float linGain = std::pow (10.f, gainSmooth / 20.f);// Determine linear amplitude

        if (linGain < autoCompGR)
            autoCompGR = linGain;// Keep one value per buffer that represents the maximum amount of GR

        delayedBuffer.getWritePointer (0)[n] *= linGain;// Apply to input signal for left channel
        delayedBuffer.getWritePointer (1)[n] *= linGain;// Apply to input signal for right channel
        
        // Need to copy back to buffer in order that the limiter is responding to the compressed signal
        buffer.getWritePointer(0)[n] = delayedBuffer.getWritePointer (0)[n];
        buffer.getWritePointer(1)[n] = delayedBuffer.getWritePointer (1)[n];
    }
}

void LimiterProcessor::reset()
{
    upbuffer.clear();
    lookaheadBuffer.clear();
    bypassBuffer.clear();
    truePeakFrameBuffer.clear();
    truePeakPostBuffer.clear();
    truePeakAnalysis.reset();
    truePeakPostAnalysis.reset();

    for (int i = 0; i < LASIZE; ++i)
    {
        lookahead[i][0] = 0.f;
        lookahead[i][1] = 0.f;
        bypassArray[i][0] = 0.f;
        bypassArray[i][0] = 0.f;
    }
}

}
