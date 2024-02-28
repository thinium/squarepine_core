
// This model is based on the Virtual Analog analysis
// by Will Pirkle in Ch 7 of Designing Synthesizer Software textbook

namespace djdawprocessor
{

class SEMLowPassFilter
{
public:
    void prepareToPlay (double Fs, int)
    {
        sampleRate = (float) Fs;
        normFreqSmooth.reset (sampleRate, 0.0001f);
        resSmooth.reset (sampleRate, 0.0001f);
        updateCoefficients();
    }

    void setNormFreq (float newNormFreq)
    {
        if (targetFreq != newNormFreq)
        {
            targetFreq = newNormFreq;
            normFreqSmooth.setTargetValue (targetFreq);
            updateCoefficients();
        }
    }

    // Allowable range from 0.01f to ~10
    void setQValue (float q)
    {
        if (targetRes != q)
        {
            targetRes = q;
            resSmooth.setTargetValue (targetRes);
            updateCoefficients();
        }
    }

    float processSample (float x, int channel)
    {
        resSmooth.getNextValue();
        normFreqSmooth.getNextValue();
        // Filter Prep
        float yhp = (x - rho * s1[channel] - s2[channel]) * alpha0;

        float x1 = alpha1 * yhp;
        float y1 = s1[channel] + x1;
        float ybp = std::tanh (1.f * y1);
        s1[channel] = x1 + ybp;

        float x2 = alpha1 * ybp;
        float ylp = s2[channel] + x2;
        s2[channel] = x2 + ylp;

        return ylp;
    }
private:
    SmoothedValue<float, ValueSmoothingTypes::Linear> normFreqSmooth { 1.0f };
    SmoothedValue<float, ValueSmoothingTypes::Linear> resSmooth { 0.7071f };

    float targetFreq = 0.0f;
    float targetRes = 0.1f;

    float sampleRate;
    float R;// transformed bandwidth "Q"
    float g;
    float G;
    float alpha;
    float alpha0;
    float alpha1;
    float rho;
    float s1[2] = { 0.0f };
    float s2[2] = { 0.0f };
public:
    void updateCoefficients()
    {
        R = 1.f / (2.f * resSmooth.getNextValue());
        float normFreq = normFreqSmooth.getNextValue();

        float freqHz = 2.f * std::powf (10.f, 3.f * normFreq + 1.f);
        freqHz = jmin (freqHz, (sampleRate / 2.f) * 0.95f);
        float wd = 2.f * (float) M_PI * freqHz;
        float T = 1.f / sampleRate;
        float wa = (2.f / T) * std::tan (wd * T / 2.f);// Warping for BLT
        g = wa * T / 2.f;
        G = g / (1.f + g);
        alpha = G;
        alpha0 = 1.f / (1.f + 2.f * R * g + g * g);
        alpha1 = g;
        rho = 2.f * R + g;
    }
};

class SEMHighPassFilter
{
public:
    void prepareToPlay (double Fs, int)
    {
        sampleRate = (float) Fs;
        normFreqSmooth.reset (sampleRate, 0.0001);
        resSmooth.reset (sampleRate, 0.0001);
        updateCoefficients();
    }

    void setNormFreq (float newNormFreq)
    {
        if (targetFreq != newNormFreq)
        {
            targetFreq = newNormFreq;
            normFreqSmooth.setTargetValue (targetFreq);
            updateCoefficients();
        }
    }

    // Allowable range from 0.01f to ~10
    void setQValue (float q)
    {
        if (targetRes != q)
        {
            targetRes = q;
            resSmooth.setTargetValue (targetRes);
            updateCoefficients();
        }
    }

    float processSample (float x, int channel)
    {
        resSmooth.getNextValue();
        normFreqSmooth.getNextValue();

        resSmooth.getNextValue();
        normFreqSmooth.getNextValue();
        // Filter Prep
        float yhp = (x - rho * s1[channel] - s2[channel]) * alpha0;

        float x1 = alpha1 * yhp;
        float y1 = s1[channel] + x1;
        float ybp = std::tanh (1.f * y1);
        s1[channel] = x1 + ybp;

        float x2 = alpha1 * ybp;
        float ylp = s2[channel] + x2;
        s2[channel] = x2 + ylp;

        return yhp;
    }
private:
    SmoothedValue<float, ValueSmoothingTypes::Linear> normFreqSmooth { 0.0f };
    SmoothedValue<float, ValueSmoothingTypes::Linear> resSmooth { 0.7071f };

    float targetFreq = 0.0f;
    float targetRes = 0.1f;

    float sampleRate;
    float R;// transformed bandwidth "Q"
    float g;
    float G;
    float alpha;
    float alpha0;
    float alpha1;
    float rho;
    float s1[2] = { 0.0f };
    float s2[2] = { 0.0f };
public:

    void updateCoefficients()
    {
        R = 1.f / (2.f * resSmooth.getNextValue());
        float normFreq = normFreqSmooth.getNextValue();

        float freqHz = 2.f * std::powf (10.f, 3.f * normFreq + 1.f);
        freqHz = jmin (freqHz, (sampleRate / 2.f) * 0.95f);
        float wd = 2.f * (float) M_PI * freqHz;
        float T = 1.f / sampleRate;
        float wa = (2.f / T) * std::tan (wd * T / 2.f);// Warping for BLT
        g = wa * T / 2.f;
        G = g / (1.f + g);
        alpha = G;
        alpha0 = 1.f / (1.f + 2.f * R * g + g * g);
        alpha1 = g;
        rho = 2.f * R + g;
    }
};

// Added this class temporarily for the HPF. Eventually we will want to use a better model of the SEM HPF
class DigitalFilter
{
public:
    enum FilterType
    {
        LPF,
        HPF,
        BPF1,
        BPF2,
        NOTCH,
        LSHELF,
        HSHELF,
        PEAK,
        APF
    };

    void setFilterType (FilterType filterTypeParam)
    {
        this->filterType = filterTypeParam;
    }

    void processBuffer (juce::AudioBuffer<float>& buffer, MidiBuffer&)
    {
        for (int c = 0; c < buffer.getNumChannels(); ++c)
        {
            for (int n = 0; n < buffer.getNumSamples(); ++n)
            {
                float x = buffer.getWritePointer (c)[n];
                float y = processSample (x, c);
                buffer.getWritePointer (c)[n] = y;
            }
        }
    }

    void processToOutputBuffer (juce::AudioBuffer<float>& inBuffer, juce::AudioBuffer<float>& outBuffer)
    {
        for (int c = 0; c < inBuffer.getNumChannels(); ++c)
        {
            for (int n = 0; n < inBuffer.getNumSamples(); ++n)
            {
                float x = inBuffer.getWritePointer (c)[n];
                float y = processSample (x, c);
                outBuffer.getWritePointer (c)[n] = y;
            }
        }
    }

    float processSample (float x, int channel)
    {
        performSmoothing();

        // Output, processed sample (Direct Form 1)
        float y = b0 * x + b1 * x1[channel] + b2 * x2[channel]
                  + (-a1) * y1[channel] + (-a2) * y2[channel];

        x2[channel] = x1[channel];// store delay samples for next process step
        x1[channel] = x;
        y2[channel] = y1[channel];
        y1[channel] = y;

        return y;
    }

    void setFs (double newFs)
    {
        Fs = static_cast<float> (newFs);
        updateCoefficients();// Need to update if Fs changes
    }

    void setFreq (float newFreq)
    {
        freqTarget = jlimit (20.0f, 20000.0f, newFreq);
        freqTarget = jmin (freqTarget, (Fs / 2.f) * 0.95f);
    }

    void setQ (float newQ)
    {
        qTarget = jlimit (0.1f, 10.0f, newQ);
    }

    void setAmpdB (float newAmpdB)
    {
        ampdB = newAmpdB;
    }
private:
    FilterType filterType = LPF;

    float Fs = 48000.0;// Sampling Rate

    // Variables for User to Modify Filter
    float freqTarget = 20.0f;// frequency in Hz
    float freqSmooth = 20.0f;
    float qTarget = 0.7071f;// Q => [0.1 - 10]
    float qSmooth = 0.7071f;
    float ampdB = 0.0f;// Amplitude on dB scale

    // Variables for Biquad Implementation
    // 2 channels - L,R : Up to 2nd Order
    float x1[2] = { 0.0f };// 1 sample of delay feedforward
    float x2[2] = { 0.0f };// 2 samples of delay feedforward
    float y1[2] = { 0.0f };// 1 sample of delay feedback
    float y2[2] = { 0.0f };// 2 samples of delay feedback

    // Filter coefficients
    float b0 = 1.0f;// initialized to pass signal
    float b1 = 0.0f;// without filtering
    float b2 = 0.0f;
    float a0 = 1.0f;
    float a1 = 0.0f;
    float a2 = 0.0f;

    int smoothingCount = 0;
    const int SAMPLESFORSMOOTHING = 256;
    void performSmoothing()
    {
        float alpha = 0.9999f;
        freqSmooth = alpha * freqSmooth + (1.f - alpha) * freqTarget;
        qSmooth = alpha * qSmooth + (1.f - alpha) * qTarget;

        smoothingCount++;
        if (smoothingCount >= SAMPLESFORSMOOTHING)
        {
            updateCoefficients();
            smoothingCount = 0;
        }
    }

    void updateCoefficients()
    {
        float A = std::pow (10.0f, ampdB / 40.0f);// Linear amplitude

        // Normalize frequency
        float f_PI = static_cast<float> (M_PI);
        float w0 = (2.0f * f_PI) * freqSmooth / Fs;

        // Bandwidth/slope/resonance parameter
        float alpha = std::sin (w0) / (2.0f * qSmooth);

        float cw0 = std::cos (w0);
        switch (filterType)
        {
            case LPF:
            {
                a0 = 1.0f + alpha;
                float B0 = (1.0f - cw0) / 2.0f;
                b0 = B0 / a0;
                float B1 = 1.0f - cw0;
                b1 = B1 / a0;
                float B2 = (1.0f - cw0) / 2.0f;
                b2 = B2 / a0;
                float A1 = -2.0f * cw0;
                a1 = A1 / a0;
                float A2 = 1.0f - alpha;
                a2 = A2 / a0;
                break;
            }
            case HPF:
            {
                a0 = 1.0f + alpha;
                float B0 = (1.0f + cw0) / 2.0f;
                b0 = B0 / a0;
                float B1 = -(1.0f + cw0);
                b1 = B1 / a0;
                float B2 = (1.0f + cw0) / 2.0f;
                b2 = B2 / a0;
                float A1 = -2.0f * cw0;
                a1 = A1 / a0;
                float A2 = 1.0f - alpha;
                a2 = A2 / a0;
                break;
            }
            case BPF1:
            {
                float sw0 = std::sin (w0);
                a0 = 1.0f + alpha;
                float B0 = sw0 / 2.0f;
                b0 = B0 / a0;
                float B1 = 0.0f;
                b1 = B1 / a0;
                float B2 = -sw0 / 2.0f;
                b2 = B2 / a0;
                float A1 = -2.0f * cw0;
                a1 = A1 / a0;
                float A2 = 1.0f - alpha;
                a2 = A2 / a0;
                break;
            }
            case BPF2:
            {
                a0 = 1.0f + alpha;
                float B0 = alpha;
                b0 = B0 / a0;
                float B1 = 0.0f;
                b1 = B1 / a0;
                float B2 = -alpha;
                b2 = B2 / a0;
                float A1 = -2.0f * cw0;
                a1 = A1 / a0;
                float A2 = 1.0f - alpha;
                a2 = A2 / a0;

                break;
            }
            case NOTCH:
            {
                a0 = 1.0f + alpha;
                float B0 = 1.0f;
                b0 = B0 / a0;
                float B1 = -2.0f * cw0;
                b1 = B1 / a0;
                float B2 = 1.0f;
                b2 = B2 / a0;
                float A1 = -2.0f * cw0;
                a1 = A1 / a0;
                float A2 = 1.0f - alpha;
                a2 = A2 / a0;
                break;
            }
            case LSHELF:
            {
                float sqA = std::sqrt (A);
                a0 = (A + 1.0f) + (A - 1.0f) * cw0 + 2.0f * sqA * alpha;
                float B0 = A * ((A + 1.0f) - (A - 1.0f) * cw0 + 2.0f * sqA * alpha);
                b0 = B0 / a0;
                float B1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cw0);
                b1 = B1 / a0;
                float B2 = A * ((A + 1.0f) - (A - 1.0f) * cw0 - 2.0f * sqA * alpha);
                b2 = B2 / a0;
                float A1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cw0);
                a1 = A1 / a0;
                float A2 = (A + 1.0f) + (A - 1.0f) * cw0 - 2.0f * sqA * alpha;
                a2 = A2 / a0;

                break;
            }

            case HSHELF:
            {
                float sqA = std::sqrt (A);
                a0 = (A + 1.0f) - (A - 1.0f) * cw0 + 2.0f * sqA * alpha;
                float B0 = A * ((A + 1.0f) + (A - 1.0f) * cw0 + 2.0f * sqA * alpha);
                b0 = B0 / a0;
                float B1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cw0);
                b1 = B1 / a0;
                float B2 = A * ((A + 1.0f) + (A - 1.0f) * cw0 - 2.0f * sqA * alpha);
                b2 = B2 / a0;
                float A1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cw0);
                a1 = A1 / a0;
                float A2 = (A + 1.0f) - (A - 1.0f) * cw0 - 2.0f * sqA * alpha;
                a2 = A2 / a0;

                break;
            }

            case PEAK:

            {
                a0 = 1.0f + alpha / A;
                float B0 = 1.0f + alpha * A;
                b0 = B0 / a0;
                float B1 = -2.0f * cw0;
                b1 = B1 / a0;
                float B2 = 1.0f - alpha * A;
                b2 = B2 / a0;
                float A1 = -2.0f * cw0;
                a1 = A1 / a0;
                float A2 = 1.0f - alpha / A;
                a2 = A2 / a0;

                break;
            }

            case APF:
            {
                a0 = 1.0f + alpha;
                float B0 = 1.0f - alpha;
                b0 = B0 / a0;
                float B1 = -2.0f * cw0;
                b1 = B1 / a0;
                float B2 = 1.0f + alpha;
                b2 = B2 / a0;
                float A1 = -2.0f * cw0;
                a1 = A1 / a0;
                float A2 = 1.0f - alpha;
                a2 = A2 / a0;

                break;
            }
        }
    }
};

// The SEMFilter in the DJDAW is a combination of both a LPF and HPF
// By changing the value of the cut-off parameter to be above 0 (halfway),
// the filter becomes a HPF. If the value is below 0, it is a LPF
class SEMFilter final : public InternalProcessor,
                        public AudioProcessorParameter::Listener
{
public:
    SEMFilter (int idNum = 1)
        : idNumber (idNum)
    {
        reset();

        NormalisableRange<float> freqRange = { -1.f, 1.f };
        auto normFreq = std::make_unique<NotifiableAudioParameterFloat> ("freqSEM", "Frequency", freqRange, 0.0f,
                                                                         true,// isAutomatable
                                                                         "Cut-off",
                                                                         AudioProcessorParameter::genericParameter,
                                                                         [] (float value, int) -> String
                                                                         {
                                                                             if (approximatelyEqual (value, 0.0f))
                                                                                 return "BYP";

                                                                             if (value < 0.0f)
                                                                             {
                                                                                 float posFreq = value + 1.f;
                                                                                 float freqHz = 2.f * std::powf (10.f, 3.f * posFreq + 1.f);
                                                                                 return String (freqHz, 0);
                                                                             }
                                                                             else
                                                                             {
                                                                                 float freqHz = 2.f * std::powf (10.f, 3.f * value + 1.f);
                                                                                 return String (freqHz, 0);
                                                                             }
                                                                         });

        NormalisableRange<float> qRange = { 0.1f, 10.f };
        auto res = std::make_unique<NotifiableAudioParameterFloat> ("resSEM", "resonance", qRange, 0.7071f,
                                                                    true,// isAutomatable
                                                                    "Q",
                                                                    AudioProcessorParameter::genericParameter,
                                                                    [] (float value, int) -> String
                                                                    {
                                                                        if (approximatelyEqual (value, 0.1f))
                                                                            return "0.1";

                                                                        if (approximatelyEqual (value, 10.f))
                                                                            return "10";

                                                                        return String (value, 1);
                                                                    });

        setPrimaryParameter (normFreqParam);
        normFreqParam = normFreq.get();
        normFreqParam->addListener (this);

        resParam = res.get();
        resParam->addListener (this);

        auto layout = createDefaultParameterLayout (false);
        layout.add (std::move (normFreq));
        layout.add (std::move (res));
        apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));
    }

    ~SEMFilter() override
    {
        normFreqParam->removeListener (this);
        resParam->removeListener (this);
    }

    void prepareToPlay (double Fs, int bufferSize) override
    {
        const ScopedLock sl (getCallbackLock());
        lpf.prepareToPlay (Fs, bufferSize);
        hpf1.prepareToPlay (Fs, bufferSize);
        hpf2.prepareToPlay (Fs, bufferSize);
        mixLPF.reset (Fs, 0.001f);
        mixHPF.reset (Fs, 0.001f);
        setRateAndBufferSizeDetails (Fs, bufferSize);
    }

    //==============================================================================
    /** @internal */
    const String getName() const override { return TRANS ("SEM Filter"); }
    /** @internal */
    Identifier getIdentifier() const override { return "SEM Filter" + String (idNumber); }
    /** @internal */
    bool supportsDoublePrecisionProcessing() const override { return false; }

    void parameterValueChanged (int paramNum, float value) override
    {
        const ScopedLock sl (getCallbackLock());
        if (paramNum == 1)
        {// Frequency change
            lpf.setNormFreq (jmin (1.f, value + 1.f));
            hpf1.setNormFreq (jmax (0.0001f, value));
            hpf2.setNormFreq (jmax (0.0001f, value));

            if (value < 0.f)
            {
                mixLPF.setTargetValue (1.f);
                mixHPF.setTargetValue (0.f);
            }
            else if (value > 0.f)
            {
                mixLPF.setTargetValue (0.f);
                mixHPF.setTargetValue (1.f);
            }
            else
            {
                mixLPF.setTargetValue (0.f);
                mixHPF.setTargetValue (0.f);
            }
        }
        else
        {// Resonance change
            lpf.setQValue (value);
            hpf1.setQValue (value);
            hpf2.setQValue (value);
        }
    }

    void parameterGestureChanged (int, bool) override {   }

    void processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&) override { process (buffer); }
    //void processBlock (juce::AudioBuffer<double>& buffer, MidiBuffer&) override  { process (buffer); }

    void process (juce::AudioBuffer<float>& buffer)
    {
        if (isBypassed())
            return;
        lpf.updateCoefficients();
        hpf1.updateCoefficients();
        hpf2.updateCoefficients();

        const auto numChannels = buffer.getNumChannels();
        const auto numSamples = buffer.getNumSamples();

        const ScopedLock sl (getCallbackLock());

        float x, y, mix, hpv;
        for (int c = 0; c < numChannels; ++c)
        {
            for (int s = 0; s < numSamples; ++s)
            {
                mix = mixLPF.getNextValue();
                x = buffer.getWritePointer (c)[s];
                y = (1.f - mix) * x + mix * lpf.processSample (x, c);

                mix = mixHPF.getNextValue();
                hpv = (float) hpf1.processSample (y, c);
                hpv = (float) hpf2.processSample (hpv, c);
                y = (1.f - mix) * y + mix * hpv;
                buffer.getWritePointer (c)[s] = y;
            }
        }
    }

    void setNormFreq (float newNormFreq)
    {
        normFreqParam->setValueNotifyingHost (newNormFreq);
        lpf.setNormFreq (newNormFreq);
        hpf1.setNormFreq (newNormFreq);
        hpf2.setNormFreq (newNormFreq);

        if (newNormFreq < 0.f)
        {
            mixLPF.setTargetValue (1.f);
            mixHPF.setTargetValue (0.f);
        }
        else if (newNormFreq > 0.f)
        {
            mixLPF.setTargetValue (0.f);
            mixHPF.setTargetValue (1.f);
        }
        else
        {
            mixLPF.setTargetValue (0.f);
            mixHPF.setTargetValue (0.f);
        }
    }

    // Allowable range from 0.01f to ~10
    void setQValue (float q)
    {
        resParam->setValueNotifyingHost (q);
        lpf.setQValue (q);
        hpf1.setQValue (q);
        hpf2.setQValue (q);

    }
    void setID (int idNum)
    {
        idNumber = idNum;
    }
private:
    //==============================================================================
    AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    NotifiableAudioParameterFloat* normFreqParam = nullptr;
    NotifiableAudioParameterFloat* resParam = nullptr;

    SmoothedValue<float, ValueSmoothingTypes::Linear> mixLPF { 0.0f };
    SmoothedValue<float, ValueSmoothingTypes::Linear> mixHPF { 0.0f };

    int idNumber = 1;

    SEMLowPassFilter lpf;
    SEMHighPassFilter hpf1, hpf2;
};

}
