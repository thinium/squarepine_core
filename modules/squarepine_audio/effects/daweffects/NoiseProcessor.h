
namespace djdawprocessor
{

/// TODO: NoiseProcessor is a BPF for both directions of the knob. 12 dB/oct roll-off (2nd order), Q = 5
/// TODO: FREQ Range is 20-20k
/// Extra parameter: controls the amplitude of the additive noise (0 min to 1 max)

/// Filter color effect is similar to the noise processor (class needs to be added)
/// Filter appears to be a LPF and HPF, not a BPF. 2nd-order roll-off and same frequency range
/// Extra parameter is Q. Minimum appears to be 0.5 (definitely less than 0.7071) and max is Q=5
namespace djdawprocessor
{
/// NoiseProcessor is a BPF for both directions of the knob. 12 dB/oct roll-off (2nd order), Q = 5
/// FREQ Range is 20-20k
/// Extra parameter: controls the amplitude of the additive noise (0 min to 1 max)

class NoiseProcessor final : public InternalProcessor,
public AudioProcessorParameter::Listener
{
public:
    //Constructor with ID
    NoiseProcessor (int idNum = 1);
    ~NoiseProcessor()override;
    
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
    void parameterGestureChanged (int, bool) override{}
    
private:
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterFloat* colourParam = nullptr;
    NotifiableAudioParameterFloat* volumeParam = nullptr;
    AudioParameterBool* fxOnParam = nullptr;
    
    
    int idNumber = 1;

    double sampleRate = 48000.0;
    
    DigitalFilter hpf;
    DigitalFilter lpf;
    
    const float DEFAULTQ = 0.7071f;
    const float RESQ = 4.f;
    const float INITLPF = 20000.f;
    const float INITHPF = 10.f;
    
    float wetSmooth[2] = {0.f};
  
    Random generator; // Based on "dither" class
};


}
