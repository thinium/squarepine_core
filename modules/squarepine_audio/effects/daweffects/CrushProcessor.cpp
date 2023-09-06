namespace djdawprocessor
{

CrushProcessor::CrushProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWetDelay", "Dry/Wet", wetDryRange, 0.5f,
                                                                   true,// isAutomatable
                                                                   "Dry/Wet",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       int percentage = roundToInt (value * 100);
                                                                       String txt (percentage);
                                                                       return txt << "%";
                                                                   });

    auto fxon = std::make_unique<NotifiableAudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", true, [] (bool value, int) -> String
                                                                {
                                                                    if (value > 0)
                                                                        return TRANS ("On");
                                                                    return TRANS ("Off");
                                                                    ;
                                                                });

    /*
     Turn counterclockwise: Increases the sound’s distortion.
     Turn clockwise: The sound is crushed before passing through the high pass filter.
     */
    NormalisableRange<float> colourRange = { -1.0, 1.0f };
    auto colour = std::make_unique<NotifiableAudioParameterFloat> ("colour", "Colour", colourRange, 0.f,
                                                                   true,// isAutomatable
                                                                   "Colour ",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       String txt (std::round (100.f * value) / 100.f);
                                                                       return txt;
                                                                       ;
                                                                   });

    NormalisableRange<float> otherRange = { 0.f, 1.0f };
    auto other = std::make_unique<NotifiableAudioParameterFloat> ("other", "Other", otherRange, 0.5f,
                                                                  true,// isAutomatable
                                                                  "Other ",
                                                                  AudioProcessorParameter::genericParameter,
                                                                  [] (float value, int) -> String
                                                                  {
                                                                      int percentage = roundToInt (value * 100);
                                                                      String txt (percentage);
                                                                      return txt << "%";
                                                                  });

    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);

    fxOnParam = fxon.get();
    fxOnParam->addListener (this);

    colourParam = colour.get();
    colourParam->addListener (this);

    otherParam = other.get();
    otherParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (colour));
    layout.add (std::move (other));

    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (colourParam);
}

CrushProcessor::~CrushProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    colourParam->removeListener (this);
    otherParam->removeListener (this);
}

//============================================================================== Audio processing
void CrushProcessor::prepareToPlay (double sampleRate, int bufferSize)
{
    bitCrusher.prepareToPlay (sampleRate, bufferSize);
    highPassFilter.setFilterType (DigitalFilter::FilterType::HPF);
    highPassFilter.setFs (sampleRate);
    delayBlock.setFs (static_cast<float> (sampleRate));
    dryBuffer = AudioBuffer<float> (2, bufferSize);
}
void CrushProcessor::processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float wet = 0.5f;
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
    
    
    highPassFilter.processBuffer (buffer, midi);
    bitCrusher.processBlock (buffer, midi);
    
    for (int c = 0; c < numChannels; ++c)
    {
        dryBuffer.copyFrom (c, 0, buffer, c, 0, buffer.getNumSamples());
    }
    
    for (int c = 0; c < numChannels; ++c)
    {
        for (int n = 0; n < numSamples; ++n)
        {
            float x = buffer.getWritePointer (c)[n];

            float wetSample = delayBlock.processSample (x, c);

            //float y = x + wetSmooth[c] * wetSample;
            buffer.getWritePointer (c)[n] = wetSmooth[c] * wetSample * colorSmooth[c];
            dryBuffer.getWritePointer (c)[n] *= (1.f - wetSmooth[c]);
            wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;
            colorSmooth[c] = 0.999f * colorSmooth[c] + 0.001f * colorSign;
        }
        buffer.addFrom (c, 0, dryBuffer, c, 0, buffer.getNumSamples());
    }
}

const String CrushProcessor::getName() const { return TRANS ("Crush"); }
/** @internal */
Identifier CrushProcessor::getIdentifier() const { return "Crush" + String (idNumber); }
/** @internal */
bool CrushProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void CrushProcessor::parameterValueChanged (int paramNum, float value)
{
    const ScopedLock sl (getCallbackLock());

    if (paramNum == 2) {}// wet/dry
    else if (paramNum == 3) // "color"
    {
        float other = otherParam->get();
        float samplesOfDelay = jmin (14.f, 1.f + abs (value) * 10.f + other * 10.f);
        delayBlock.setDelaySamples (samplesOfDelay);
        if (value <= 0.f)
        {
            highPassFilter.setFreq (20.0);
            float normValue = (value * -1.f);
            // 4 bits -> normValue = 0
            // 9 bits -> normValue = 1
            float bitDepth = 4.f * std::powf (1.f - normValue, 2.f) + 6.f;
            bitCrusher.setBitDepth (bitDepth);
            colorSign = 1.f;
        }
        else
        {
            float normValue = value;
            // freqHz = 20 -> 5000
            float freqHz = 2.f * std::powf (10.f, 3.f * (normValue * 0.8f) + 1.f);
            highPassFilter.setFreq (freqHz);
            // 9 bits -> value = 0.5, normValue = 0
            // 4 bits -> value = 1, normValue = 0
            float bitDepth = 4.f * std::powf (1.f - normValue, 2.f) + 6.f;
            bitCrusher.setBitDepth (bitDepth);
            colorSign = -1.f;
        }
    }
    else if (paramNum == 4)// "other"
    {
        float color = colourParam->get();
        float samplesOfDelay = jmin (14.f, 1.f + value * 10.f + abs (color) * 10.f);
        delayBlock.setDelaySamples (samplesOfDelay);
    }
}

}
