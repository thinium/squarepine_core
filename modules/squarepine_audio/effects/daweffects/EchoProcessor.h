
namespace djdawprocessor
{

/// Feedback seems to be constant and long, need to measure with IR to watch rate of reduction
/// Level parameter is wet/dry control. Dry signal is constant amplitude, wet signal is blended in by parameter
/// Delay length (min/max range) is sync'd to tempo
/// XPad is sync'd delay time

class EchoProcessor final : public BandProcessor

{
public:
    //Constructor with ID
    EchoProcessor (int idNum = 1);
    ~EchoProcessor() override;

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

    double sampleRate = 44100.0;

    SmoothedValue<float, ValueSmoothingTypes::Linear> wetDry { 0.0f };
    SmoothedValue<float, ValueSmoothingTypes::Linear> delayTime { 0.0f };

    FractionalDelay delayUnit;

    float z[2] = { 0.f };
};

}
