
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
    //NotifiableAudioParameterFloat* feedbackParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;

    FractionalDelay delayLeft;
    FractionalDelay delayRight;
    
    float z = 0.f;
    float wetSmooth = 0.5f;
    
    float Fs = 48000.f;
};

}
