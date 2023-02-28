namespace djdawprocessor
{

class RollProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    RollProcessor (int idNum = 1);
    ~RollProcessor() override;

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
    NotifiableAudioParameterBool* onOffParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;

    double sampleRate = 0.0;
    int delayTimeInSamples = 0;
    
    AudioBuffer<float> segmentBuffer;
    int segmentFillIndex = 0;
    int segmentPlayIndex = 0;
    int maxSegmentIndex = 0;
    bool fillSegmentFlag = false;
    void fillSegmentBuffer (AudioBuffer<float> & buffer);
  
    AudioBuffer<float> tempBuffer; // used to multiband processing
    void fillTempBuffer ();
};

}
