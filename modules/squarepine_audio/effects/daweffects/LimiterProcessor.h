//
//  LimiterProcessor.h
//
namespace djdawprocessor
{
class LimiterProcessor
{
public:
    LimiterProcessor();

    // Functions for Compressor
    void processBuffer (AudioBuffer<float>& buffer);
    void processStereoSample (float xL, float xR, float detectSample, float& yL, float& yR);
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

    void setConstantGainMonitoring (bool isOn) { constantGainMonitoring = isOn; }
    void setOversampling (bool isOn);
    void setOfflineOS (bool isOn);
    void setBypassed (bool bypass) { bypassed = bypass; }
    void setEnhanceAmount (float amount) { enhanceAmount = amount / 100.f; }// convert from %
    void setEnhanceOn (bool isOn) { enhanceIsOn = isOn; }
    void setTruePeakOn (bool isOn) { truePeakIsOn = isOn; }
    void setAutoCompOn (bool isOn) { autoCompIsOn = isOn; }
    void setOverSamplingLevel (int level);

    float getGainReduction (bool linear);
    
    void setCeiling(float ceiling_dB);
    float getCeiling();
private:
    bool bypassed = false;

    float Fs = 48000.0f;// Sampling Rate
    int samplesPerBuffer = 1024;

    // Variables for Static Characteristics
    float ceilingLinearThresh = 0.999f;
    float thresh = -36.0f;// threshold - dB Value
    float linThresh = pow (10.f, thresh / 20.f);
    float ratio = 100.0f;// 1 = "1:1", 2 = "2:1"
    float knee = 1.0f;// Knee width in dB

    // Variables for Response Time
    float attack = 0.06f;// Time in seconds
    float release = 0.4f;// Time in seconds

    // Gain Variables
    float inputGain = 1.f;
    float outputGain = 1.f;
    float inputGainSmooth = 1.f;
    float inputInvSmooth = 1.f;
    float outputGainSmooth = 1.f;
    

    float ceiling = -0.1f;
    
    void applySmoothGain (AudioBuffer<float>& buffer, float targetGain, float& smoothGain);
    float applyCeilingReduction(float& value, bool isLeft);
    
    
    bool constantGainMonitoring = false;

    float alphaA = 0.999f;// smoothing parameter for attack
    float alphaR = 0.999f;// smoothing parameter for release

    // Because this variable needs to be saved from the previous loop for L&R, then it is an array
    float gainSmoothPrev[2] = { 0.0f };// Variable for smoothing on dB scale

    std::atomic<float> linA = 1.0f;// Linear gain multiplied by the input signal at the end of the detection path

    std::atomic<float> gainReduction = 1.0f;
    
    float additionalGainReductionL = 0.f;
    float additionalGainReductionR = 0.f;
    float additionalGainReductionSmoothL = 0.f;
    float additionalGainReductionSmoothR = 0.f;

    
    bool truePeakIsOn = true;
    TruePeakAnalysis truePeakAnalysis;
    AudioBuffer<float> truePeakFrameBuffer;
    
    TruePeakAnalysis truePeakPostAnalysis;
    AudioBuffer<float> truePeakPostBuffer;

    int OSFactor = 2;
    static const int OSQuality = 3;
    bool overSamplingOn = true;
    bool offlineOSOn = true;
    AudioBuffer<float> upbuffer;
    UpSampling2Stage upsampling;
    DownSampling2Stage downsampling;

    static const int LASIZE = 48000;
    float lookahead[LASIZE][2] = { { 0.f } };
    int indexLARead[2] = { 0 };
    int indexLAWrite[2] = { 4800 };// .1 ms
    AudioBuffer<float> lookaheadBuffer;
    void lookaheadDelay (AudioBuffer<float>& buffer, AudioBuffer<float>& delayedBuffer, const int numChannels, const int numSamples);

    float bypassArray[LASIZE][2] = { { 0.f } };
    int indexBYRead[2] = { 0 };
    int indexBYWrite[2] = { 4800 };// .1 ms
    AudioBuffer<float> bypassBuffer;
    void bypassDelay (AudioBuffer<float>& buffer, AudioBuffer<float>& delayedBuffer, const int numChannels, const int numSamples);

    float enhanceProcess (float x);
    float enhanceAmount = 0.2f;// 20 % in Oxford Limiter
    bool enhanceIsOn = true;

    bool autoCompIsOn = true;

    void processAutoComp (AudioBuffer<float>& buffer, AudioBuffer<float>& delayedBuffer, const int numChannels, const int numSamples);

    // Because this variable needs to be saved from the previous loop for L&R, then it is an array
    float acGainSmoothPrev = 0.f;// Variable for smoothing on dB scale
};
}
