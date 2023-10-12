
namespace djdawprocessor
{

/// Stereo, alternative echo
/// There is some galluping pattern, see manual for hints.
/// XPad is sync'd delay time

class PingPongProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    PingPongProcessor (int idNum = 1);
    ~PingPongProcessor() override;

    //============================================================================== Audio processing
    void prepareToPlay (double Fs, int bufferSize) override;
    void processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&) override;
    //============================================================================== House keeping
    const String getName() const override;
    /** @internal */
    Identifier getIdentifier() const override;
    /** @internal */
    bool supportsDoublePrecisionProcessing() const override;
    //============================================================================== Parameter callbacks
    void parameterValueChanged (int paramNum, float value) override;
    void parameterGestureChanged (int, bool) override {}
private:
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    NotifiableAudioParameterFloat* timeParam = nullptr;
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;

    SmoothedValue<float, ValueSmoothingTypes::Linear> delayTime { 0.0f };

    const float feedback = 0.7f;// appears to be constant from hardware demo

    ModulatedDelay delayLeft;
    ModulatedDelay delayRight;

    ModulatedDelay delayLeft2;
    ModulatedDelay delayRight2;

    const float AMPSUMCHAN = 0.7071f;
    float z = 0.f;
    float zUnit2 = 0.f;
    float wetSmooth = 0.5f;

    float Fs = 48000.f;

    void getStereoDelayedSample (float xL, float xR, float& yL, float& yR);

    void getDelayFromDoubleBuffer (float xL, float xR, float& yL, float& yR);
    bool usingDelayBuffer1 = true;
    bool duringCrossfade = false;
    void getDelayDuringCrossfade (float xL, float xR, float& yL, float& yR);
    const int LENGTHOFCROSSFADE = 1024;
    int crossfadeIndex = 0;

    bool crossFadeFrom1to2 = true;

    void getDelayWithAmp (float xL, float xR, float& yL, float& yR, float ampA, float ampB);
};

}
