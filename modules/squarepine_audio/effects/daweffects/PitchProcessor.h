namespace djdawprocessor
{
/// Pitch shifter from -50% (down octave) to +100% (up octave). Pitch amount is controlled
/// by "time" parameter as a percentage. Xpad also gives continous pitch control.
/// Other knob acts as a multiplier to the pitch amount. When at a minimum, pitch isn't
/// actually changed. When at 50%, multiplier = 1. When at a maximum, multiplier is 2.
/// Therefore, up 1 octave can actually be stetched to 2 octaves. And down an octave, can
/// actually be stretched down to -100% or no pitch (signal disappears)
namespace djdawprocessor
{
/// Pitch shifter from -50% (down octave) to +100% (up octave). Pitch amount is controlled
/// by "time" parameter as a percentage. Xpad also gives continous pitch control.
/// Other knob acts as a multiplier to the pitch amount. When at a minimum, pitch isn't
/// actually changed. When at 50%, multiplier = 1. When at a maximum, multiplier is 2.
/// Therefore, up 1 octave can actually be stetched to 2 octaves. And down an octave, can
/// actually be stretched down to -100% or no pitch (signal disappears)

class PitchProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    PitchProcessor (int idNum = 1);
    ~PitchProcessor()override;
    
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
    void parameterGestureChanged (int, bool) override{}
    
private:
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    NotifiableAudioParameterFloat* pitchParam = nullptr;
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;

    
    int idNumber = 1;

 
    PitchShifter pitchShifter;
    AudioBuffer<float> effectBuffer;
    
    float wetSmooth[2] = {0.f};
    
    bool useElastiquePro = false;
    zplane::ElastiquePtr elastique;
    float pitchFactorTarget = 1.f;
    float pitchFactorSmooth = 1.f;
    
    AudioBuffer<float> inputBuffer;
    AudioBuffer<float> outputBuffer;
};

}
