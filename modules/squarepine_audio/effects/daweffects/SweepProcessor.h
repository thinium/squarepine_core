namespace djdawprocessor
{
/// The sweep processor appears to be a combination of gate (Color turned left) and filter (color turned right)
/// The gate seems to be based more on the rate of change for the signal envelope (think transient designer). It
/// does not appear to be a standard gate effect where the gain reduction is based on thresh, ratio, attack, release.
/// The Nexus 1 has a Gate/Comp color effect, which is not implemented in DJDAW. However, I analyzed it and made some
/// more notes about the gate (see Crush effect).
///
/// The extra parameter appears to have in impact on how sensitive the gate is when turned to the left (no filter when
/// turned left). The extra parameter is the frequency control for the BPF when turned to the right.
///
/// The color control for the filter is the bandwidth of the filter. It causes the cut-off frequency on either side of
/// the BPF to narrow from 20 or 20k to the center frequency (set by the extra parameter).

class SweepProcessor final : public InternalProcessor,
public AudioProcessorParameter::Listener
{
public:
    //Constructor with ID
    SweepProcessor (int idNum = 1);
    ~SweepProcessor()override;
    
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
    NotifiableAudioParameterFloat* fxFrequencyParam = nullptr;
    NotifiableAudioParameterFloat* colourParam = nullptr;
    NotifiableAudioParameterFloat* otherParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;

    DigitalFilter lpf;
    DigitalFilter hpf;
        
    const float DEFAULTQ = 0.7071f;
    const float RESQ = 4.f;
    const float INITLPF = 20000.f;
    const float INITHPF = 10.f;
    
    int idNumber = 1;

    float wetSmooth[2] = {0.f};
    
    // Gate/Transient Designer (comparison of fast and slow  envelope)
    float gFast = 0.9991f; // Feedback gain for "fast" envelope
    float fbFast[2] = {0.f}; // delay variable
    float gSlow = 0.9999f; // feedback gain for "slow" envelope
    float fbSlow[2] = {0.f};
    
};

}
