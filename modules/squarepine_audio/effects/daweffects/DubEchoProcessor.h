
namespace djdawprocessor
{
/// there seems to be subtle modulation of the delay.
/// Extra parameter does several things. It controls feedback amount.
/// It seems like when this parameter is less than 50%, it is increasing the
/// feedback amount from ~one repeat to many. When the parameter is 50% or above,
/// this parameter seems to be blending in a delay with 100% feedback. Although,
/// it doesn't sound like new audio is getting fed into the delay. Rather, it sounds
/// like a loop from when this parameter reached 50% is getting repeated endlessly.
/// Therefore, I think there are actually 2 different delays blended together. One
/// for the parameter below 50% and this other 100% feedback thing when knob is above 50%.

/// The color parameter has an effect on several things. There is a low-pass filter on the
/// repeats when color is turned left and a high-pass filter when color turned right. When
/// the color parameter is at the center, there is no effect. So, it seems like it is some
/// sort of a dry/wet control. Also, the length of the delay is controlled by the color knob.
/// When close to center, the delays are shorter than when the knob is twisted to the max/min
/// left and right.

class DubEchoProcessor final : public V10SendProcessor
{
public:
    //Constructor with ID
    DubEchoProcessor (int idNum = 1);
    ~DubEchoProcessor() override;

    //============================================================================== Audio processing
    void prepareToPlay (double Fs, int bufferSize) override;
    void processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&) override;
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

    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterFloat* timeParam = nullptr;
    NotifiableAudioParameterFloat* echoColourParam = nullptr;
    NotifiableAudioParameterFloat* feedbackParam = nullptr;
    AudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;

    double sampleRate = 48000.0;

    ModulatedDelay delayBlock;
    float feedbackSample[2] = { 0.f };
    PhaseIncrementer phase;
    float primaryDelay = 1000.f;

    DigitalFilter hpf;
    DigitalFilter lpf;

    const float lfoFreq = 0.3f; // subtle, slow modulation
    const float periodOfCycle = 1.f/lfoFreq;
    
    float gainSmooth[2] = { 0.f };
    float gain;
    float wetSmooth[2] = { 0.f };
};

}
