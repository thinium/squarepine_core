
namespace djdawprocessor
{

/// TODO: default time is 500 ms. Seems to be square wave amplitude modulation (possibly smoothed to avoid discontinuities)
/// TODO: wet/dry is true blend between unprocessed and processed
// X-PAD sync'd control of time

/// The beat FX have three bands of processing (low, mid, high). This gives the user the opportunity
/// to only have the FX applied to specific frequency bands and not to others. For the most part,
/// many of the FX are parallel effects where the multi-band processing is only on the effected
/// signal. The TRANS effect is one where only the effected/wet signal is output when the extra
/// parameter is at the max. I used this to analyze the low/mid/high. It appears to use Linkwitz-Riley
/// filters with cut-offs at 300 Hz and 5 kHz. A recording of this was captured. 


class TransEffectProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    TransEffectProcessor (int idNum = 1);
    ~TransEffectProcessor()override;
    
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
    NotifiableAudioParameterFloat* timeParam = nullptr;
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;
    
    int idNumber = 1;

    PhaseIncrementer phase;
    
    float wetSmooth[2] = {0.0};
    float ampSmooth[2] = {1.0};
    float depthSmooth[2] = {1.0};
};

}
