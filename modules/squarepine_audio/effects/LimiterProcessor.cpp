//
//  LimiterProcessor.cpp
//

//#include "LimiterProcessor.h"

LimiterProcessor::LimiterProcessor()
{
    setThreshold(-0.1f);
    setKnee(1.5f);
    setInputGain(0.f);
    setOutputGain(-0.1f);
}



void LimiterProcessor::processBuffer (AudioBuffer<float>& buffer)
{
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    bypassDelay (buffer, bypassBuffer, numChannels, numSamples);
    
    // Input Gain
    //buffer.applyGain (0, numSamples, inputGain); // not smoothed
    applySmoothGain (buffer, inputGain, inputGainSmooth);
    
    lookaheadDelay (buffer, lookaheadBuffer, numChannels, numSamples);
    
    fillTruePeakFrameBuffer (lookaheadBuffer, numChannels, numSamples);
    
    if (bypassed)
    {
        for (int c = 0; c < numChannels ; ++c)
            buffer.copyFrom (c, 0, bypassBuffer.getWritePointer(c), numSamples);
        
        return;
    }
    
    if (autoCompIsOn)
        processAutoComp (buffer, lookaheadBuffer, numChannels, numSamples);
    
    if (numChannels == 2)
    {
        if (overSamplingOn)
        {
            float * channelData = nullptr;
            float * osChannelData = nullptr;
            for (int c = 0; c < numChannels ; ++c){
                channelData = lookaheadBuffer.getWritePointer(c);
                osChannelData = upbuffer.getWritePointer(c);
                upsampling.process(channelData, osChannelData, c, numSamples);
            }
            
            float xL;
            float xR;
            float yL;
            float yR;
            for (int i = 0 ; i < numSamples ; ++i)
            {
                float detectSample = truePeakFrameBuffer.getWritePointer (0)[i];
                for (int j = 0; j < OSFactor ; ++j)
                {
                    int index = OSFactor * i + j;
                    xL = upbuffer.getWritePointer (0)[index];
                    xR = upbuffer.getWritePointer (1)[index];
                    
                    processStereoSample (xL, xR, detectSample, yL, yR);
                    
                    upbuffer.getWritePointer (0)[index] = yL;
                    upbuffer.getWritePointer (1)[index] = yR;
                }
            }
            
            for (int c = 0; c < numChannels ; ++c){
                channelData = lookaheadBuffer.getWritePointer(c);
                osChannelData = upbuffer.getWritePointer(c);
                downsampling.process(osChannelData, channelData, c, buffer.getNumSamples());
            }
        }
        else
        {
            float xL;
            float xR;
            float yL;
            float yR;
            for (int i = 0 ; i < numSamples ; ++i)
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
        for (int i = 0 ; i < numSamples ; ++i)
        {
            x = lookaheadBuffer.getWritePointer (c)[i];
            float detectSample = truePeakFrameBuffer.getWritePointer (0)[i];
            y = processSample (x, detectSample, c);
            lookaheadBuffer.getWritePointer (c)[i] = y;
        }
    }
    
    // Constant Gain Monitoring
    if (constantGainMonitoring)
    {
        //lookaheadBuffer.applyGain (0, numSamples, 1.f/inputGain);
        applySmoothGain (lookaheadBuffer, 1.f/inputGain, inputInvSmooth);
    }
    
    for (int c = 0; c < numChannels ; ++c)
        buffer.copyFrom (c,0,lookaheadBuffer.getWritePointer(c),numSamples);
}

void LimiterProcessor::processStereoSample (float xL, float xR, float detectSample, float & yL, float & yR)
{
    
    float xUni = std::abs (detectSample); // Convert bi-polar signal to uni-polar on decibel scale
    float x_dB = 20.0f * std::log10 (xUni);
    if (x_dB < -144.f)
    {
        x_dB = -144.f;
    } // Prevent -Inf values

    float gainSC; // This variable calculates the "desired" output level based on static characteristics
    if (x_dB > thresh + knee / 2.0f)
    {
        gainSC = thresh + knee / 2.0f; // + (x_dB - thresh) / ratio;
    }
    else if (x_dB > (thresh - knee / 2.0f))
    {
        gainSC = x_dB + ((1.f / ratio - 1.f) * std::pow(x_dB - thresh + knee / 2.f, 2.f)) / (2.f * knee);
    }
    else
    {
        gainSC = x_dB;
    }

    float gainChange_dB = gainSC - x_dB;

    // First-order smoothing filter to control attack and release response time
    float gainSmooth;
    if (gainChange_dB < gainSmoothPrev[0])
    { // Attack mode
        gainSmooth = (1.f - alphaA) * gainChange_dB + alphaA * gainSmoothPrev[0];
    }
    else
    { // Release mode
        gainSmooth = (1.f - alphaR) * gainChange_dB + alphaR * gainSmoothPrev[0];
    }

    gainSmoothPrev[0] = gainSmooth; // Save for the next loop (i.e. delay sample in smoothing filter)

    linA = std::pow(10.f, gainSmooth / 20.f); // Determine linear amplitude

    yL = linA * xL; // Apply to input signal for left channel
    yR = linA * xR; // Apply to input signal for right channel
    
    if (enhanceIsOn)
    {
        yL = enhanceProcess (yL);
        yR = enhanceProcess (yR);
    }
    
    outputGainSmooth = 0.999f*outputGainSmooth + 0.001f*outputGain;
    yL *= outputGainSmooth;
    yR *= outputGainSmooth;
}

float LimiterProcessor::processSample(float x, float detectSample, int channel)
{
    // Output variable
    float y = 0;

    float xUni = std::abs (detectSample); // Convert bi-polar signal to uni-polar on decibel scale
    float x_dB = 20.0f * std::log10 (xUni);
    if (x_dB < -144.f)
    {
        x_dB = -144.f;
    } // Prevent -Inf values

    float gainSC; // This variable calculates the "desired" output level based on static characteristics
    if (x_dB > thresh + knee / 2.0f)
    {
        gainSC = thresh + knee / 2.0f; // + (x_dB - thresh) / ratio;
    }
    else if (x_dB > (thresh - knee / 2.0f))
    {
        gainSC = x_dB +((1.f / ratio - 1.f) * std::pow(x_dB - thresh + knee / 2.f, 2.f)) / (2.f * knee);
    }
    else
    {
        gainSC = x_dB;
    }

    float gainChange_dB = gainSC - x_dB;

    // First-order smoothing filter to control attack and release response time
    float gainSmooth;
    if (gainChange_dB < gainSmoothPrev[channel])
    { // Attack mode
        gainSmooth = (1.f - alphaA) * gainChange_dB + alphaA * gainSmoothPrev[channel];
    }
    else
    { // Release mode
        gainSmooth = (1.f - alphaR) * gainChange_dB + alphaR * gainSmoothPrev[channel];
    }

    gainSmoothPrev[channel] = gainSmooth; // Save for the next loop (i.e. delay sample in smoothing filter)

    linA = std::pow(10.f, gainSmooth / 20.f); // Determine linear amplitude

    y = linA * x; // Apply to input signal
    
    if (enhanceIsOn)
        y = enhanceProcess (y);
    
    return y * outputGain;
}

void LimiterProcessor::lookaheadDelay (AudioBuffer<float> & buffer, AudioBuffer<float> & delayedBuffer,
                                        const int numChannels, const int numSamples)
{
    float x;
    for (int c = 0 ; c < numChannels ; ++c)
    {
        for (int n = 0 ; n < numSamples ; ++n)
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


void LimiterProcessor::bypassDelay (AudioBuffer<float> & buffer, AudioBuffer<float> & delayedBuffer,
                                        const int numChannels, const int numSamples)
{
    float x;
    for (int c = 0 ; c < numChannels ; ++c)
    {
        for (int n = 0 ; n < numSamples ; ++n)
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

void LimiterProcessor::fillTruePeakFrameBuffer (AudioBuffer<float> inputBuffer, const int numChannels, const int numSamples)
{
    if (numChannels == 2)
    {
        float xL; float xR;
        for (int n = 0; n < numSamples; ++n)
        {
            xL = inputBuffer.getWritePointer (0)[n];
            xR = inputBuffer.getWritePointer (1)[n];
            float detectSample = std::sqrtf(0.5f * (std::powf(xL,2.0f) + std::powf(xR,2.0f)));

            tpAnalysisBuffer[tpIndex] = detectSample;
            
            if (truePeakIsOn)
            {
                detectSample = truePeakAnalysis.analyzePhase1 (tpAnalysisBuffer,tpIndex);
                detectSample = jmax (detectSample , truePeakAnalysis.analyzePhase2 (tpAnalysisBuffer,tpIndex));
                detectSample = jmax (detectSample , truePeakAnalysis.analyzePhase3 (tpAnalysisBuffer,tpIndex));
                detectSample = jmax (detectSample , truePeakAnalysis.analyzePhase4 (tpAnalysisBuffer,tpIndex));
            }
            truePeakFrameBuffer.getWritePointer (0)[n] = detectSample;
            tpIndex++;
            if (tpIndex >= TPSIZE)
                tpIndex = 0;
            
            inputBuffer.getWritePointer (0)[n] = latencyBuffer[latIndex][0];
            inputBuffer.getWritePointer (1)[n] = latencyBuffer[latIndex][1];
            latencyBuffer[latIndex][0] = xL;
            latencyBuffer[latIndex][1] = xR;
            
            latIndex++;
            if (latIndex >= LATENCY)
                latIndex = 0;
            
        }
    }
    else
    {
        // MONO
        float x;
        float detectSample;
        
        for (int n = 0; n < numSamples; ++n)
        {
            x = inputBuffer.getWritePointer (0)[n];

            tpAnalysisBuffer[tpIndex] = x;
            
            detectSample = truePeakAnalysis.analyzePhase1 (tpAnalysisBuffer,tpIndex);
            detectSample = jmax (detectSample , truePeakAnalysis.analyzePhase2 (tpAnalysisBuffer,tpIndex));
            detectSample = jmax (detectSample , truePeakAnalysis.analyzePhase3 (tpAnalysisBuffer,tpIndex));
            detectSample = jmax (detectSample , truePeakAnalysis.analyzePhase4 (tpAnalysisBuffer,tpIndex));
            
            truePeakFrameBuffer.getWritePointer (0)[n] = detectSample;
            
            tpIndex++;
            if (tpIndex >= TPSIZE)
                tpIndex = 0;
            
            inputBuffer.getWritePointer (0)[n] = latencyBuffer[latIndex][0];
            latencyBuffer[latIndex][0] = x;

            latIndex++;
            if (latIndex >= LATENCY)
                latIndex = 0;
        }
    }
}

void LimiterProcessor::setOversampling (bool isOn)
{
    overSamplingOn = isOn;
    if (isOn)
    {
        upsampling.prepare(OSFactor, OSQuality);
        downsampling.prepare(OSFactor, OSQuality);
    }
    setAttack (attack);
    setRelease (release);
}

void LimiterProcessor::setOfflineOS (bool isOn)
{
    offlineOSOn = isOn;
//    if (isOn)
//    {
//        upsampling.prepare(OSFactor, OSQuality);
//        downsampling.prepare(OSFactor, OSQuality);
//    }
//    setAttack (attack);
//    setRelease (release);
}

void LimiterProcessor::prepare(float sampleRate, int bufferSize)
{
    Fs = sampleRate;
    // Variables to set if Fs changes
    setAttack(attack);
    setRelease(release);
    truePeakFrameBuffer = AudioBuffer<float> (1 , bufferSize);
    
    lookaheadBuffer = AudioBuffer<float> (2 , bufferSize); // (numChannels, numSamples)
    bypassBuffer = AudioBuffer<float> (2 , bufferSize); // (numChannels, numSamples)
    
    upbuffer = AudioBuffer<float> (2 , bufferSize * OSFactor); // (numChannels, numSamples)
    upsampling.prepare(OSFactor, OSQuality);
    downsampling.prepare(OSFactor, OSQuality);
    
    
    int tempIndex = static_cast<int> (round(Fs*.1f)); // Write should be ahead of Read when indexing buffer
    indexLAWrite[0] = tempIndex; indexLAWrite[1] = tempIndex;
    indexLARead[0] = 0; indexLARead[1] = 0;
    
    indexBYWrite[0] = tempIndex; indexBYWrite[1] = tempIndex;
    indexBYRead[0] = 0; indexBYRead[1] = 0;
}

void LimiterProcessor::setThreshold(float threshold)
{
    thresh = jlimit (-64.0f, 0.0f, threshold - knee/2.f); // accounts for half of knee above thresh for limit level
    linThresh = pow (10.f,thresh/20.f);
}

float LimiterProcessor::getThreshold()
{
    return thresh;
}

void LimiterProcessor::setRatio(float rat)
{
    ratio = jlimit(1.0f, 100.0f, rat);
}

float LimiterProcessor::getRatio()
{
    return ratio;
}

void LimiterProcessor::setKnee(float kne)
{
    knee = jlimit(0.0f, 10.0f, kne);
}

float LimiterProcessor::getKnee()
{
    return knee;
}

void LimiterProcessor::setAttack(float att)
{
    attack = jlimit(0.001f, 0.5f, att);
    // Convert time in seconds to a coefficient value for the smoothing filter
    if (overSamplingOn)
        alphaA = expf(-logf(9.0f) / (Fs * static_cast<float> (OSFactor) * attack));
    else
        alphaA = expf(-logf(9.0f) / (Fs * attack));
}

float LimiterProcessor::getAttack()
{
    return attack;
}

void LimiterProcessor::setRelease(float rel)
{
    release = jlimit(0.01f, 1.0f, rel);
    if (overSamplingOn)
        alphaR = expf(-logf(9.0f) / (Fs * static_cast<float> (OSFactor) * release));
    else
        alphaR = expf(-logf(9.0f) / (Fs * release));
}

float LimiterProcessor::getRelease()
{
    return release;
}


void LimiterProcessor::setInputGain(float inputGain_dB)
{
    inputGain = std::pow(10.f,inputGain_dB/20.f);
}

float LimiterProcessor::getInputGain()
{
    return 20.f * log10(outputGain);
}

void LimiterProcessor::setOutputGain(float outputGain_dB)
{
    outputGain = std::pow(10.f,outputGain_dB/20.f);
}

float LimiterProcessor::getOutputGain()
{
    return 20.f * log10(outputGain);
}


float LimiterProcessor::getGainReduction()
{
    // This method can be find the amount of gain reduction at a given time
    // if the interface has a meter or display.
    return 20.f * log10(linA);
}

float LimiterProcessor::enhanceProcess(float x)
{
    float y = x;
    if (x > linThresh)
    {
        y = linThresh - ((x-linThresh) * enhanceAmount);
    }
    else if (x < -linThresh)
    {
        y = -linThresh - ((x+linThresh) * enhanceAmount);
    }
    return y;
}


void LimiterProcessor::applySmoothGain(AudioBuffer<float> & buffer, float targetGain, float & smoothGain)
{
    //const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    for (int n = 0; n < numSamples ; ++n)
    {
        smoothGain = 0.999f * smoothGain + 0.001f * targetGain;
        buffer.getWritePointer (0)[n] *= smoothGain;
        buffer.getWritePointer (1)[n] *= smoothGain;
    }
}



void LimiterProcessor::processAutoComp (AudioBuffer<float> & buffer, AudioBuffer<float> & delayedBuffer, const int numChannels, const int numSamples)
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
    float acRatio = 2.f; // 2:1 ratio
    // the lookahead buffer  is  delayed  by .1 sec, so I chose that for the attack time
    float acAlphaA = expf(-logf(9.0f) / (Fs * .1f));
    float acAlphaR = expf(-logf(9.0f) / (Fs * .2f)); // slightly longer release for smoothness
    
    // For the AutoComp, the original "buffer" is used for the sidechain analysis, and the result
    // is applied to the delayedBuffer so that the compressor gradually decreases amplitude prior
    // to the peaks occurring
    
    for (int n = 0 ; n < numSamples ; ++n)
    {
        float xL = buffer.getWritePointer (0)[n];
        float xR = buffer.getWritePointer (1)[n];
        float detectSample = std::sqrtf(0.5f * (std::powf(xL,2.0f) + std::powf(xR,2.0f)));
        
        float xUni = std::abs (detectSample); // Convert bi-polar signal to uni-polar on decibel scale
        float x_dB = 20.0f * std::log10 (xUni);
        if (x_dB < -144.f)
        {
            x_dB = -144.f;
        } // Prevent -Inf values

        float gainSC; // This variable calculates the "desired" output level based on static characteristics
        
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
        { // Attack mode
            gainSmooth = (1.f - acAlphaA) * gainChange_dB + acAlphaA * acGainSmoothPrev;
        }
        else
        { // Release mode
            gainSmooth = (1.f - acAlphaR) * gainChange_dB + acAlphaR * acGainSmoothPrev;
        }

        acGainSmoothPrev = gainSmooth; // Save for the next loop (i.e. delay sample in smoothing filter)

        float linGain = std::pow(10.f, gainSmooth / 20.f); // Determine linear amplitude

        delayedBuffer.getWritePointer (0)[n] *= linGain; // Apply to input signal for left channel
        delayedBuffer.getWritePointer (1)[n] *= linGain; // Apply to input signal for right channel
        
    }
}
