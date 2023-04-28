namespace djdawprocessor
{

/// This effect is an "echo" effect with some pitch shifting controlled by the XPAD.
/// The delay length is adjusted by the "time" knob.
/// The other main parameter controls the feedback amount and wet dry.
/// At a minimum setting, entirely dry with not echo.
/// At 50%, I think it is 50% dry/wet and feedback ~90%. Above 50%, I think the dry
/// signal is decreased and the wet signal is increased all the way to 100% at max setting.
/// The feedback is also 100% when the knob is >50%. This might be another case (like spiral)
/// where a special, separate delay is blended in because it sounds like the exact same section
/// of audio is looped (no new audio is added).

/// XPAD is a pitch shifter. It seems pitch shifting only happens to wet signal (blended with dry - not pitch shifted)
/// It seems like the pitch shifting happens based on a smoothed envelope from the XPAD parameter (does not track instantaneously).
/// Can get shifted both up and down (when XPAD parameter above/below mid point).  On thing to try out is whether the
/// pitch shifting should be before the feedback delay or whether it should be part of the feedback path. (EDIT: pitch shifting is not in the feedback path. The pitch does not continue changing with each repetition)

/// It seems possible that the pitch shifting is performed by using a modulated delay. By adjusting the "pitch"
/// on the XPAD, the actual delay time of the effect seems to change along with the pitch amount. (EDIT: for the sake of sound quality, Elastique was used for the pitch shifting).

class HelixProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    HelixProcessor (int idNum = 1);
    ~HelixProcessor() override;

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

    AudioBuffer<float> effectBuffer;

    float wetSmooth[2] = { 0.f };

    bool useElastiquePro = false;
    zplane::ElastiquePtr elastique;
    float pitchFactorTarget = 1.f;
    float pitchFactorSmooth = 1.f;

    AudioBuffer<float> inputBuffer;
    AudioBuffer<float> outputBuffer;

    FractionalDelay delayUnit;

    float z[2] = { 0.f };
    float sampleRate = 44100.f;
};

}
