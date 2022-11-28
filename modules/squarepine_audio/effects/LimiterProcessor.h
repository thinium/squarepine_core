//
//  LimiterProcessor.h
//


#pragma once

//#include "Biquad.h" // Used for HPF to prevent pumping from low-frequencies.

class LimiterProcessor
{
public:
    LimiterProcessor();

    // Functions for Compressor
    void processBuffer (AudioBuffer<float>& buffer);
    void processStereoSample (float xL, float xR, float detectSample, float & yL, float & yR);
    float processSample (float x, float detectSample, int channel);

    void prepare (float sampleRate, int bufferSize);

    void setThreshold (float thresh);
    float getThreshold();

    void setRatio (float ratio);
    float getRatio();

    void setKnee (float knee);
    float getKnee();

    void setAttack (float attack);
    float getAttack();

    void setRelease (float release);
    float getRelease();
    
    void setInputGain (float inputGain_dB);
    float getInputGain();
    
    void setOutputGain (float outputGain_dB);
    float getOutputGain();
    
    void setConstantGainMonitoring (bool isOn) {constantGainMonitoring = isOn;}
    void setOversampling (bool isOn);
    void setBypassed (bool bypass) {bypassed = bypass;}

    float getGainReduction();

private:
    
    bool bypassed = false;
    
    float Fs = 48000.0f; // Sampling Rate

    // Variables for Static Characteristics
    float thresh = -36.0f; // threshold - dB Value
    float linThresh = pow (10.f,thresh/20.f);
    float ratio = 100.0f; // 1 = "1:1", 2 = "2:1"
    float knee = 1.0f; // Knee width in dB

    // Variables for Response Time
    float attack = 0.06f; // Time in seconds
    float release = 0.4f; // Time in seconds
    
    // Gain Variables
    float inputGain = 1.f;
    float outputGain = 1.f;
    float inputGainSmooth = 1.f;
    float inputInvSmooth = 1.f;
    float outputGainSmooth = 1.f;
    void applySmoothGain(AudioBuffer<float> & buffer, float targetGain, float & smoothGain);

    bool constantGainMonitoring = false;
    
    float alphaA = 0.999f; // smoothing parameter for attack
    float alphaR = 0.999f; // smoothing parameter for release

    // Because this variable needs to be saved from the previous loop for L&R, then it is an array
    float gainSmoothPrev[2] = { 0.0f }; // Variable for smoothing on dB scale

    std::atomic<float> linA = 1.0f; // Linear gain multiplied by the input signal at the end of the detection path

    static const int TPSIZE = 12;
    float tpAnalysisBuffer[TPSIZE] = {0.f};
    int tpIndex = 0;
    static const int LATENCY = 6; // True Peak Analysis introduces 6 samples of latency from filtering
    float latencyBuffer[LATENCY][2] = {{0.f}};
    int latIndex = 0;
    TruePeakAnalysis truePeakAnalysis;
    AudioBuffer<float> truePeakFrameBuffer;
    void fillTruePeakFrameBuffer(AudioBuffer<float> inputBuffer, const int numChannels, const int numSamples);
    
    int OSFactor = 2;
    static const int OSQuality = 3;
    bool overSamplingOn = true;
    AudioBuffer<float> upbuffer;
    UpSampling2Stage upsampling;
    DownSampling2Stage downsampling;
    
    static const int LASIZE = 48000;
    float lookahead[LASIZE][2] = {{0.f}};
    int indexLARead[2] = {0};
    int indexLAWrite[2] = {4800}; // .1 ms
    AudioBuffer<float> lookaheadBuffer;
    void lookaheadDelay (AudioBuffer<float> & buffer, AudioBuffer<float> & delayedBuffer, const int numChannels, const int numSamples);
    
    float enhanceProcess(float x);
    float enhanceAmount = 0.2f; // 20 % in Oxford Limiter
    
    void processAutoComp (AudioBuffer<float> & buffer, AudioBuffer<float> & delayedBuffer, const int numChannels, const int numSamples);

    // Because this variable needs to be saved from the previous loop for L&R, then it is an array
    float acGainSmoothPrev = 0.f; // Variable for smoothing on dB scale
};
