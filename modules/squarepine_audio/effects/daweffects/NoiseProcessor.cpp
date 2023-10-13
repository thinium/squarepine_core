namespace djdawprocessor
{

NoiseProcessor::NoiseProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWetDelay", "Dry/Wet", wetDryRange, 1.f,
                                                                   true,// isAutomatable
                                                                   "Dry/Wet",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       int percentage = roundToInt (value * 100);
                                                                       String txt (percentage);
                                                                       return txt << "%";
                                                                   });

    auto fxon = std::make_unique<AudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String
                                                      {
                                                          if (value > 0)
                                                              return TRANS ("On");
                                                          return TRANS ("Off");
                                                          ;
                                                      });
    /*
     Turn counterclockwise: The cut-off frequency of the filter through which the white noise passes gradually descends.
     Turn clockwise: The cut-off frequency of the filter through which the white noise passes gradually rises.
     */
    NormalisableRange<float> colourRange = { -1.0, 1.0f };
    auto colour = std::make_unique<NotifiableAudioParameterFloat> ("colour", "Colour", colourRange, 0.f,
                                                                   true,// isAutomatable
                                                                   "Colour ",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       String txt (round (100.f * value) / 100.f);
                                                                       return txt;
                                                                       ;
                                                                   });
    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);

    fxOnParam = fxon.get();
    fxOnParam->addListener (this);

    colourParam = colour.get();
    colourParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (colour));

    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (colourParam);

    hpf.setFilterType (DigitalFilter::FilterType::HPF);
    hpf.setFreq (INITHPF);
    hpf.setQ (DEFAULTQ);
    lpf.setFilterType (DigitalFilter::FilterType::LPF);
    lpf.setFreq (INITLPF);
    lpf.setQ (DEFAULTQ);
}

NoiseProcessor::~NoiseProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    colourParam->removeListener (this);
}

//============================================================================== Audio processing
void NoiseProcessor::prepareToPlay (double Fs, int)
{
    sampleRate = Fs;
    hpf.setFs (sampleRate);
    lpf.setFs (sampleRate);
    generator.setSeedRandomly();
}
void NoiseProcessor::processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float wet;
    bool bypass;
    float colour;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        bypass = ! fxOnParam->get();
        colour = colourParam->get();
    }

    if (bypass || isBypassed())
        return;

    if (abs(colour) < 0.01f)
        wet = 0.f;
    
    for (int c = 0; c < numChannels; ++c)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            float x = buffer.getWritePointer (c)[n];

            float noise = 0.125f * (generator.nextFloat() - 0.5f);// (range = -.5 to +.5) scaled -18 dB

            auto filterNoise = hpf.processSample (noise, c);
            filterNoise = lpf.processSample (filterNoise, c);

            float y = x + wetSmooth[c] * filterNoise;

            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            buffer.getWritePointer (c)[n] = y;
        }
    }
}

const String NoiseProcessor::getName() const { return TRANS ("Noise"); }
/** @internal */
Identifier NoiseProcessor::getIdentifier() const { return "Noise" + String (idNumber); }
/** @internal */
bool NoiseProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void NoiseProcessor::parameterValueChanged (int paramIndex, float value)
{
    const ScopedLock sl (getCallbackLock());
    switch (paramIndex)
    {
        case (1):
        {
            // fx on/off (handled in processBlock)
            break;
        }
        case (2):
        {// Wet/dry (handled in processBlock)
            break;
        }
        case (3):
        {//Colour
            if (value > 0.f)
            {
                float freqHz = std::powf (10.f, value * 3.2f + 1.f);// 10 - 16000
                hpf.setFreq (freqHz);
                lpf.setFreq (INITLPF);
                lpf.setQ (DEFAULTQ);
                hpf.setQ (RESQ);
            }
            else
            {
                float normValue = 1.f + value;
                float freqHz = 2.f * std::powf (10.f, normValue * 3.f + 1.f) + 30.f;// 20030 -> 50
                lpf.setFreq (freqHz);
                hpf.setFreq (INITHPF);
                hpf.setQ (DEFAULTQ);
                lpf.setQ (RESQ);
            }
            break;
        }
    }
}

}
