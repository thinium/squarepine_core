//
//  Fractional delay class
//
//  Created by Eric Tarr on 2/6/20.
//  Copyright © 2020 Eric Tarr. All rights reserved.
//
namespace djdawprocessor
{

class ModulatedDelay
{
    // Use in situations when smoothing of the delay is unnecessary because it comes from an LFO
public:
    float processSample (float x, int channel);

    void setFs (float _Fs);

    void setDelaySamples (float _delay);

    void clearDelay();
private:
    float Fs = 48000.f;

    float delay = 5.f;

    static const int MAX_BUFFER_SIZE = 384000;
    float delayBuffer[MAX_BUFFER_SIZE][2] = { { 0.0f } };
    int index[2] = { 0 };
};

class FractionalDelay
{
    // Includes smoothing of delay, see ModulatedDelay when using an LFO (smoothing unnecessary)
public:
    float processSample (float x, int channel);

    void setFs (float _Fs);

    void setDelaySamples (float _delay);

    void clearDelay();
private:
    float Fs = 48000.f;

    float delay = 5.f;
    float smoothDelay[2] = { 5.f };

    static const int MAX_BUFFER_SIZE = 384000;
    float delayBuffer[MAX_BUFFER_SIZE][2] = { { 0.0f } };
    int index[2] = { 0 };
};

class AllPassDelay
{
public:
    float processSample (float x, int channel);

    void setFs (float _Fs);

    void setDelaySamples (float _delay);
    
    void setFeedbackAmount (double fb) {feedbackAmount = fb;}

    void clearDelay();
private:
    float Fs = 48000.f;

    FractionalDelay delayBlock;
    double feedbackAmount = 0.0;
    double feedbackSample[2] = {0.0};
};


//This is a wrapper around Eric Tarr's Fractional Delay class that can be integrated with Juce/Squarepine processors
// wet/dry is true 50/50 blend (increase + decrease dry/wet)
// initial value is 500 ms, min=1ms, max = 4000ms
/// XPad is sync'd delay time

class DelayProcessor final : public BandProcessor
{
public:
    //Constructor with ID
    DelayProcessor (int idNum = 1);
    ~DelayProcessor() override;

    //============================================================================== Audio processing
    void prepareToPlay (double Fs, int bufferSize) override;
    void processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&) override;
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
    void parameterGestureChanged (int, bool) override {}
private:
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterFloat* timeParam = nullptr;

    SmoothedValue<float, ValueSmoothingTypes::Linear> wetDry { 0.0f };
    SmoothedValue<float, ValueSmoothingTypes::Linear> delayTime{ 0.0f };

    NotifiableAudioParameterBool* fxOnParam = nullptr;

    int idNumber = 1;

    FractionalDelay delayUnit;
    float sampleRate = 44100.f;
};

}
