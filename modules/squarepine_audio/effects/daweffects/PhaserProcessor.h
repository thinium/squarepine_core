
namespace djdawprocessor
{

/// TODO: there appears to be some feedback/resonance in effect, 3 notches/dips/peaks (need more APFS)
/// TODO: wet/dry is true blend, center freq of APFs goes from 400-5K, default time = 2000ms (freq of LFO)
/// X-PAD control adds warble to the modulated delay, see the LFOFilter for a detailed description

/// TODO: Robot processor = pitch shifting + ring modulation (need to create this class)
/// Default time = 0%, pitch shifting frequency is controlled by time, 0%=no change, -100% down octave, +100% up octave
/// When Level/depth = "min", only pitch shifted signal is output (no ring modulation, no true dry signal at all)
/// As level/depth is increased, a blend of pitched shifted + ring modulation occurs, there is always some pitch shifted present
/// Pitch shifted appears to be a little higher in amplitude than RM, even when fully "wet"
/// Ring modulation "wet" signal appears to have a constant modulation frequency (although the difference from original signal freq is not constant)
/// Another hypothesis is that instead of ring modulation, there are two additional pitch shifters +/- 1 semitone relative to main pitch shifter
/// that are blended in with the original. This seems to make the most sense at the moment based on changing the frequency of the input signal
/// X-PAD control does same thing as % amount with continuous control

/// TODO: Melodic processor = short note at a particular pitch (set by time parameter)
/// Appears to have rhythm dependent on input signal, but actually not during the notes of input, rather in-between notes

// Slip roll, roll, rev roll -> loop delay based on buffer of audio when effect is turned on
// (need to create these classes)
//
// Slip roll - when effect is turned on, read in audio into buffer based on time setting, use this for "wet" path (true wet/dry)
// When changing effect time, new audio from dry signal is read into buffer
/// X-PAD control is delay time

// Roll - when effect is turned on, read in full bar worth of audio into buffer, the audio stays in the buffer entire time effect is on
// When changing effect time, audio is played back from the start of the buffer and looped based on the duration of the time setting
/// X-PAD control is delay time

// Rev roll - identical to "roll" except audio is played back from the end of the buffer in reverse, same audio stays in buffer entire time
/// X-PAD control is delay time
namespace djdawprocessor
{

class PhaserProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    PhaserProcessor (int idNum = 1);
    ~PhaserProcessor() override;

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
    DigitalFilter apf1;
    DigitalFilter apf2;
    DigitalFilter apf3;
    
    int count = 0;
    static const int UPDATEFILTERS = 8;
    
    float wetSmooth[2] = {0.0};
    float warbleSmooth[2] = {1.0};
};

}
