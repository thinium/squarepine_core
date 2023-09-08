namespace djdawprocessor
{

/// It seems like this effect is a combination of distortion (bit reduction?) and
/// short delay. The delay appears to increase from 0 samples of delay (knob at center)
/// to ~10 samples of delay when turned all the way left or right. When turned to the left,
/// it seems to do something like a flanger (white noise input). Low-frequencies pass through
/// when knob turned to left, likely delay/wet path has positive amplitude. When turned to the right,
/// low frequencies are reduced, likely delay/wet path has negative amplitude. Amount of distortion
/// also increases as the knob is turned away from center. It is difficult to measure the bit depth amount
/// because the distortion seems to only be applied to the wet path, which causes an interesting sweep to
/// output signal.
//  Extra parameter knob also appears to impact the delay time with a similar range as the Color knob. It does
/// not appear to have an impact on the filter. In the sound examples captured, the HPF is active when the extra
//  parameter is at a minimum. There is not a filter that appears active in the other direction (Color = counter CW)
//  while the extra parameter is at minimum.

/// Gate/comp effect. Knob at center, no effect. Knob to left = gate. Right = comp.
/// The knob "parameter" for the effect appears to control a "hysteresis" type characteristic of the effect.
/// The knob does not appear to simply adjust the threshold, rate, attack, or release. Rather, the gate and
/// compression to be dependent on the rate of change for the gain reduction. For instance, on the gate, if
/// a test tone sine/square wave is input, the gate will allow it to pass through at almost any level if you
/// leave it at steady-state. However, the gate will cut off the signal if it drops in amplitude relatively
/// quickly. This happens even if the sine wave is at 0 dBFS and drops down quickly to -15 dBFS. At this same
/// knob setting, if a sine wave is input at -30 dBFS, the gate will eventually allow it to go through. Therefore,
/// it seems like the gate is program-dependent and adjusts relative to the long-term level of the input signal.
/// The signal only gets cut-off when a rapid decrease in amplitude happens. It appears the compressor does a similar
/// thing, only in an opposite direction. I suspect that the gate/comp was implemented this way so that it would behave
/// consistently for a wide range of input signals (some that have been mastered and so at a much lower LUFS).

class CrushProcessor final : public InternalProcessor,
                             public AudioProcessorParameter::Listener
{
public:
    //Constructor with ID
    CrushProcessor (int idNum = 1);
    ~CrushProcessor() override;

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
    void parameterGestureChanged (int, bool) override {}
private:
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterFloat* colourParam = nullptr;
    NotifiableAudioParameterFloat* otherParam = nullptr;
    NotifiableAudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;

    double Fs = 48000.0;
    DigitalFilter highPassFilter;

    float wetSmooth[2] = { 0.f };
    float colorSmooth[2] = { 0.f };

    AudioBuffer<float> dryBuffer;
    
    
    ZeroOrderHoldResampler downSampler; //ZeroOrderHoldResampler
    ZeroOrderHoldResampler upSampler; //LinearResampler //LagrangeResampler
    AudioBuffer<float> resampledBuffer;
    double downFs = 48000.0;
};

}
