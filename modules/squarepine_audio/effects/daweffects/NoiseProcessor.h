namespace djdawprocessor
{


/// This placeholder class with No DSP.  It's purpose is to provide an appropriate parameter interface for recording useful information..

class NoiseProcessor final : public InternalProcessor,
public AudioProcessorParameter::Listener
{
public:
    //Constructor with ID
    NoiseProcessor (int idNum = 1);
    ~NoiseProcessor()override;
    
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
    void parameterGestureChanged (int, bool) override{}
    
private:
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    NotifiableAudioParameterFloat* wetDryParam = nullptr;
    NotifiableAudioParameterFloat* colourParam = nullptr;
    NotifiableAudioParameterFloat* volumeParam = nullptr;
    AudioParameterBool* fxOnParam = nullptr;
    
    
    int idNumber = 1;

    double sampleRate = 48000.0;
    
    DigitalFilter hpf;
    DigitalFilter lpf;
    
    const int DEFAULTQ = 0.7071f;
    const int RESQ = 4.f;
    const int INITLPF = 20000.f;
    const int INITHPF = 10.f;
    
    float wetSmooth[2] = {0.f};
  
    Random generator; // Based on "dither" class
};


}
