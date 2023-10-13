namespace djdawprocessor
{
/// this appears to be a feedback delay with a distortion (soft-clipper? arctan?) in the feedback path
/// possible subtle modulation to delay
/// level parameter is wet/dry, but dry stays constant, the wet is just blended in
/// level parameter also controls feedback amount. For most of the range (0-80%) the feedback appears to be
/// pretty long (~80%) but it will decay. As the level parameter increases to 100%, the feedback amount goes to 100%.
/// There also appears to be a high-pass filter in the feedback path
/// Delay length (min/max range) is sync'd to tempo
// X-PAD seems to have some pitch shifting effect, but maybe this is just happening because the delay speeding up
// or slowing down continuously (see sound recording)

class SpiralProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    SpiralProcessor (int idNum = 1);
    ~SpiralProcessor() override;

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

    DigitalFilter hsf;
    DigitalFilter lsf;

    AllPassDelay apf;

    int idNumber = 1;

    FractionalDelay delayUnit;
    double sampleRate = 44100.0;
    float z[2] = { 0.f };

    float wetSmooth[2] = { 0.0 };
};

}
