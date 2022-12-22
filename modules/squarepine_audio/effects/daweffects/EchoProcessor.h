namespace djdawprocessor
{

/// This placeholder class with No DSP.  It's purpose is to provide an appropriate parameter interface for recording useful information..

class EchoProcessor final : public InsertProcessor

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
    AudioParameterChoice* beatParam = nullptr;
    NotifiableAudioParameterFloat* timeParam = nullptr;
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterFloat* xPadParam = nullptr;
    NotifiableAudioParameterFloat* feedbackParam = nullptr;
    AudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;
    
    double sampleRate = 44100.0;
    
    SmoothedValue<float, ValueSmoothingTypes::Linear> wetDry { 0.0f };
    SmoothedValue<float, ValueSmoothingTypes::Linear> delayTime{ 0.0f };
    
    FractionalDelay delayUnit;
    
    float z[2] = {0.f};
};

}
