
// CrossoverFilter.h
// Based on Linkwitz-Riley filters
// Which are 4th-order filters
// If a HPF and LPF are used in parallel
// their combined amplitude response is flat
// This type of 4th-order filter can be created by
// putting two 2nd-order butterworth filters in series.
// A butterworth filter can be made from a "DigitalFilter"
// by setting the Q=0.7071

class CrossoverFilter
{
public:
    CrossoverFilter (DigitalFilter::FilterType type, float cutoffFreq)
    {
        filter1.setFilterType (type);
        filter2.setFilterType (type);
        filter1.setFreq (cutoffFreq);
        filter2.setFreq (cutoffFreq);
        filter1.setQ (0.7071f);
        filter2.setQ (0.7071f);
    }

    void prepareToPlay (double Fs, int)
    {
        filter1.setFs (Fs);
        filter2.setFs (Fs);
    }

    void processBuffer (juce::AudioBuffer<float>& buffer, MidiBuffer& midi)
    {
        filter1.processBuffer (buffer, midi);
        filter2.processBuffer (buffer, midi);
    }

    void processToOutputBuffer (juce::AudioBuffer<float>& inBuffer, juce::AudioBuffer<float>& outBuffer)
    {
        filter1.processToOutputBuffer (inBuffer, outBuffer);
        filter2.processToOutputBuffer (inBuffer, outBuffer);
    }
private:
    DigitalFilter filter1;
    DigitalFilter filter2;
};

// CrossoverBPF is just a combination of a Linkwitz-Riley LPF and HPF
class CrossoverBPF
{
public:
    CrossoverBPF (float hpfFreq, float lpfFreq)
        : lpf (DigitalFilter::FilterType::LPF, lpfFreq),
          hpf (DigitalFilter::FilterType::HPF, hpfFreq)
    {
    }

    void prepareToPlay (double Fs, int samplesPerBuffer)
    {
        lpf.prepareToPlay (Fs, samplesPerBuffer);
        hpf.prepareToPlay (Fs, samplesPerBuffer);
    }

    void processBuffer (juce::AudioBuffer<float>& buffer, MidiBuffer& midi)
    {
        lpf.processBuffer (buffer, midi);
        hpf.processBuffer (buffer, midi);
    }

    void processToOutputBuffer (juce::AudioBuffer<float>& inBuffer, juce::AudioBuffer<float>& outBuffer)
    {
        lpf.processToOutputBuffer (inBuffer, outBuffer);
        hpf.processToOutputBuffer (inBuffer, outBuffer);
    }
private:
    CrossoverFilter lpf;
    CrossoverFilter hpf;
};
