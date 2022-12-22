namespace djdawprocessor
{
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
    AudioParameterChoice* beatParam = nullptr;
    NotifiableAudioParameterFloat* timeParam = nullptr;
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterFloat* xPadParam = nullptr;
    AudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;
    PhaseIncrementer phase;
    FractionalDelay delayBlock;
    
    float wetSmooth[2] = {0.0};
    float depthSmooth[2] = {5.0};
    
};

}
