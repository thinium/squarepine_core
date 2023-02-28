namespace djdawprocessor
{
/// TODO: default is 2000ms
/// TODO: Appears to be modulated LPF with a Q=4, 2nd-order, "level" is blend between wet/dry (true blend)
/// Delay time (min/max range) sync'd to tempo
/// X-Pad appears to add an extra modulated warble sinewave to the frequency sweep. Essentially, as the
// frequency sweeps across the spectrum at a relatively slow rate (2000 ms), there is another LFO that
// causes the frequency to warble a few Hz above and below the primary. This happens relatively quickly.
// When X-pad is at maximum (default), there is no warble. As the value goes down, the warble increases.

class LFOFilterProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    LFOFilterProcessor (int idNum = 1);
    ~LFOFilterProcessor() override;

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
    AudioParameterChoice* beatParam = nullptr;
    NotifiableAudioParameterFloat* timeParam = nullptr;
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterFloat* xPadParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;
    
    PhaseIncrementer phase;
    PhaseIncrementer phaseWarble;
    DigitalFilter bpf;
    
    int count = 0;
    static const int UPDATEFILTERS = 8;
    
    float wetSmooth[2] = {0.0};
    float warbleSmooth[2] = {5.0};
};

}
