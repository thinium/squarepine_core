
namespace djdawprocessor
{

/// Algorithmic reverb effect
/// Turning to left adds a LPF/LOW Bell (see recording) to boost low freqs below 1k (center ~500, wide Q)
/// Turning to right adds HPF/HIGH Bell to boost high freqs above 1k (center ~5k, wide Q)
/// Turning left or right sounds like a wet/dry blend. 50/50 at max settings left or right.
/// Completely dry in center
/// Extra parameter controls the reverb time from a short room to something super long (10-15 secs)
namespace djdawprocessor
{
/// Algorithmic reverb effect
/// Turning to left adds a LPF/LOW Bell (see recording) to boost low freqs below 1k (center ~500, wide Q)
/// Turning to right adds HPF/HIGH Bell to boost high freqs above 1k (center ~5k, wide Q)
/// Turning left or right sounds like a wet/dry blend. 50/50 at max settings left or right.
/// Completely dry in center
/// Extra parameter controls the reverb time from a short room to something super long (10-15 secs)

class SpaceProcessor final : public V10SendProcessor
{
public:
    //Constructor with ID
    SpaceProcessor (int idNum = 1);
    ~SpaceProcessor()override;
    
    //============================================================================== Audio processing
    void prepareToPlay (double Fs, int bufferSize) override;
    void processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&) override;
    /** @internal */
    void releaseResources() override;
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
    NotifiableAudioParameterFloat* timeParam = nullptr;
    NotifiableAudioParameterFloat* reverbColourParam = nullptr;
    NotifiableAudioParameterFloat* otherParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;
    //Using the Juce reverb
    Reverb reverb;
    void updateReverbParams();
    
    DigitalFilter filter;
    
    int idNumber = 1;
    
    
};

}
