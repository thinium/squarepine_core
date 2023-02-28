namespace djdawprocessor
{

///This is a wrapper class that can apply internal processing to a signal combinations of three frequency bands
///The Band Processor class has three main parameters, a toggle for each frequency band the processing should be applied to.
///Currently this class doesn't perform any of the processing
///
class BandProcessor : public InternalProcessor,
                      public AudioProcessorParameter::Listener
{
public:
    BandProcessor();
    ~BandProcessor() override;

    void prepareToPlay (double Fs, int bufferSize) override;
    void processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer& midi) override;
    
    void fillMultibandBuffer (juce::AudioBuffer<float>& buffer);

    //The abstract function wherein inhereted classes should perform their DSP
    virtual void processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer& midi) = 0;

    void setupBandParameters (AudioProcessorValueTreeState::ParameterLayout& layout);

    virtual void parameterValueChanged (int paramNum, float value) override;
protected:
    AudioBuffer<float> multibandBuffer;
    
private:
    AudioParameterBool *lowFrequencyToggleParam, *midFrequencyToggleParam, *highFrequencyToggleParam;
    
    static constexpr float lowCutoff = 300.f;
    static constexpr float highCutoff = 5000.f;
    CrossoverFilter lowbandFilter {DigitalFilter::FilterType::LPF, lowCutoff};
    CrossoverBPF midbandFilter {lowCutoff, highCutoff};
    CrossoverFilter highbandFilter {DigitalFilter::FilterType::HPF, highCutoff};
    
    AudioBuffer<float> tempBuffer;
};

}
