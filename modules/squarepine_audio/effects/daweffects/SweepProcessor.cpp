namespace djdawprocessor
{

SweepProcessor::SweepProcessor (int idNum)
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

    auto fxon = std::make_unique<NotifiableAudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String
                                                                {
                                                                    if (value > 0)
                                                                        return TRANS ("On");
                                                                    return TRANS ("Off");
                                                                    ;
                                                                });

    /*Turning the control to the left produces a gate effect, and turning it to the right produces a band pass filter effect.
     Turn counterclockwise: A gate effect makes the sound tighter, with a reduced sense of volume.
     Turn to right: The band pass filter bandwidth decreases steadily.
     */
    NormalisableRange<float> colourRange = { -1.0, 1.0f };
    auto colour = std::make_unique<NotifiableAudioParameterFloat> ("colour", "Colour", colourRange, 0.f,
                                                                   true,// isAutomatable
                                                                   "Colour ",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       String txt (round (value * 100.f) / 100.f);
                                                                       return txt;
                                                                       ;
                                                                   });
    /*
     Turning the [COLOR] control to the left adjusts the gate effect.
     Turn to the right to tighten the sound.
     Turning the [COLOR] to the right adjusts the center frequency.
     Turn to the right to increase the center frequency.
     
     */

    NormalisableRange<float> otherRange = { 0.f, 1.0f };
    auto other = std::make_unique<NotifiableAudioParameterFloat> ("tighten", "Tighten", otherRange, 0.5f,
                                                                  true,// isAutomatable
                                                                  "Tighten ",
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

    hpf.setFilterType (DigitalFilter::FilterType::HPF);
    hpf.setFreq (INITHPF);
    hpf.setQ (DEFAULTQ);
    lpf.setFilterType (DigitalFilter::FilterType::LPF);
    lpf.setFreq (INITLPF);
    lpf.setQ (DEFAULTQ);
}

SweepProcessor::~SweepProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    colourParam->removeListener (this);
    otherParam->removeListener (this);
}

//============================================================================== Audio processing
void SweepProcessor::prepareToPlay (double Fs, int)
{
    lpf.setFs (Fs);
    hpf.setFs (Fs);
}
void SweepProcessor::processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();

    float wet;
    float dry;
    bool bypass;
    float colour;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        dry = 1.f - wet;
        bypass = ! fxOnParam->get();
        colour = colourParam->get();
    }

    if (bypass)
        return;

    if (colour > 0)
    {
        for (int c = 0; c < numChannels; ++c)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                float x = buffer.getWritePointer (c)[n];

                float wetSample = lpf.processSample (x, c);
                wetSample = hpf.processSample (wetSample, c);

                float y = (1.f - wetSmooth[c]) * x + wetSmooth[c] * wetSample;
                wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;

                buffer.getWritePointer (c)[n] = y;
            }
        }
    }
    else
    {
        for (int c = 0; c < numChannels; ++c)
        {
            for (int n = 0; n < numSamples; ++n)
            {
                float x = buffer.getWritePointer (c)[n];

                fbFast[c] = (1.f - gFast) * 2.f * abs (x) + gFast * fbFast[c];
                fbSlow[c] = (1.f - gSlow) * 3.f * abs (x) + gSlow * fbSlow[c];

                float diffEnv = fbFast[c] - fbSlow[c];
                float susEnv = 1.f;
                if (diffEnv < 0)// Sustain section (attack is when diffEnv > 0)
                {
                    susEnv = jmax (0.f, (colour * -diffEnv * 20.f) + 1.f);
                }

                float wetSample = x * susEnv;
                float y = (1.f - wetSmooth[c]) * x + wetSmooth[c] * wetSample;
                wetSmooth[c] = 0.999f * wetSmooth[c] + 0.001f * wet;

                buffer.getWritePointer (c)[n] = y;
            }
        }
    }
}

const String SweepProcessor::getName() const { return TRANS ("Sweep"); }
/** @internal */
Identifier SweepProcessor::getIdentifier() const { return "Sweep" + String (idNumber); }
/** @internal */
bool SweepProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void SweepProcessor::parameterValueChanged (int paramIndex, float value)
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
        {
            //wetDry.setTargetValue (value);
            //
            break;
        }
        case (3):
        {
            // Colour
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
                float freqHz = 2.f * std::powf (10.f, normValue * 2.f + 2.f);// 20000 -> 200
                lpf.setFreq (freqHz);
                hpf.setFreq (INITHPF);
                hpf.setQ (DEFAULTQ);
                lpf.setQ (RESQ);
            }
            break;
        }
        case (4):
        {
            break;
        }
        case (5):
        {
            break;// Modulation
        }
    }
}

}
