namespace djdawprocessor
{
/// TODO: default is 2000 ms (freq of LFO?) there appears to be some feedback/resonance (amount?)
/// TODO: depth of LFO is ~12 samples?, wet/dry is true blend
/// X-PAD control adds warble to the modulated delay, see the LFOFilter for a detailed description


class FlangerProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    FlangerProcessor (int idNum = 1);
    ~FlangerProcessor() override;

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
    NotifiableAudioParameterFloat* xPadParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;
    PhaseIncrementer phase;
    PhaseIncrementer phaseWarble;
    ModulatedDelay delayBlock;
    
    float wetSmooth[2] = {0.f};
    float warbleSmooth[2] = {1.f};
};

}
