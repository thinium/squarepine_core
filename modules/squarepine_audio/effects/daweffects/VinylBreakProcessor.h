namespace djdawprocessor
{

namespace djdawprocessor
{

/// This appears to be a pitch shifting effect down. I guess it is supposed to simulate
/// what happens if a DJ stops a turn table. The time it takes for the "slow-down" or "brake"
/// seems to be based on the sync'd "time" and XPAD (both control similar time parameter).
/// The extra knob seems to be a multiplier on the time for the brake to happen. When
/// the knob is at a minimum, no pitch shifting occurs. When it is slightly above minimum,
/// the pitch shift all the way down takes a long time. When it is at higher settings (50% or 100%)
/// the pitch shift happens almost instantaneously.

class VinylBreakProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    VinylBreakProcessor (int idNum = 1);
    ~VinylBreakProcessor() override;

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
    NotifiableAudioParameterFloat* speedParam = nullptr;
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;
    
    float Fs = 44100.f;

    int idNumber = 1;

    AudioBuffer<float> effectBuffer;

    float wetSmooth[2] = { 0.f };

    bool useElastiquePro = false;
    zplane::ElastiquePtr elastique;
    float pitchFactorTarget = 1.f;
    float pitchFactorSmooth = 1.f;
    float alpha = 0.99f;

    AudioBuffer<float> inputBuffer;
    AudioBuffer<float> outputBuffer;
};

}
