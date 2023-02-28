namespace djdawprocessor
{

BandProcessor::BandProcessor()
{
}
BandProcessor::~BandProcessor()
{
    if (lowFrequencyToggleParam != nullptr)
        lowFrequencyToggleParam->removeListener (this);
    if (midFrequencyToggleParam != nullptr)
        midFrequencyToggleParam->removeListener (this);
    if (highFrequencyToggleParam != nullptr)
        lowFrequencyToggleParam->removeListener (this);
}

void BandProcessor::setupBandParameters (AudioProcessorValueTreeState::ParameterLayout& layout)
{
    auto lowFrequencyToggle = std::make_unique<AudioParameterBool> ("lowonoff", "Low Frequency Processing", true, "Low Frequency Processing ", [] (bool value, int) -> String
                                                                    {
                                                                        if (value > 0)
                                                                            return TRANS ("On");
                                                                        return TRANS ("Off");
                                                                        ;
                                                                    });

    auto midFrequencyToggle = std::make_unique<AudioParameterBool> ("midonoff", "Mid Frequency Processing", true, "Mid Frequency Processing ", [] (bool value, int) -> String
                                                                    {
                                                                        if (value > 0)
                                                                            return TRANS ("On");
                                                                        return TRANS ("Off");
                                                                        ;
                                                                    });

    auto highFrequencyToggle = std::make_unique<AudioParameterBool> ("highonoff", "High Frequency Processing", true, "High Frequency Processing ", [] (bool value, int) -> String
                                                                     {
                                                                         if (value > 0)
                                                                             return TRANS ("On");
                                                                         return TRANS ("Off");
                                                                         ;
                                                                     });

    lowFrequencyToggleParam = lowFrequencyToggle.get();
    lowFrequencyToggleParam->addListener (this);

    midFrequencyToggleParam = midFrequencyToggle.get();
    midFrequencyToggleParam->addListener (this);

    highFrequencyToggleParam = highFrequencyToggle.get();
    highFrequencyToggleParam->addListener (this);

    layout.add (std::move (lowFrequencyToggle));
    layout.add (std::move (midFrequencyToggle));
    layout.add (std::move (highFrequencyToggle));
}

void BandProcessor::parameterValueChanged (int paramIndex, float)
{
    const ScopedLock sl (getCallbackLock());
    switch (paramIndex)
    {
        case (1):
            //Low frequency toggle changed
            break;
        case (2):
            //Mid frequency toggle changed
            break;
        case (3):
            //High frequency toggle changed
            break;
    }
}

void BandProcessor::prepareToPlay (double Fs, int bufferSize)
{
    setRateAndBufferSizeDetails (Fs, bufferSize);
    
    int numChannels = 2;
    multibandBuffer.setSize (numChannels, bufferSize);
    tempBuffer.setSize (numChannels, bufferSize);
}
void BandProcessor::processBlock (juce::AudioBuffer<float>& buffer, MidiBuffer& midi)
{
    // Called for each individual effect's processing
    processAudioBlock (buffer, midi);
    
}

void BandProcessor::fillMultibandBuffer (juce::AudioBuffer<float>& buffer)
{
    
    bool lowOn;
    bool midOn;
    bool highOn;
    {
        const ScopedLock sl (getCallbackLock());
        lowOn = lowFrequencyToggleParam->get();
        midOn = midFrequencyToggleParam->get();
        highOn = highFrequencyToggleParam->get();
    }
    
    if (!lowOn || !midOn || !highOn)
    {
        multibandBuffer.clear();
        if (lowOn)
        {
            tempBuffer.clear();
            lowbandFilter.processToOutputBuffer (buffer, tempBuffer);
            int numChannels = buffer.getNumChannels();
            int numSamples = buffer.getNumSamples();
            for (int c = 0; c < numChannels; ++c)
                multibandBuffer.addFrom (c, 0, tempBuffer.getWritePointer(c), numSamples);
            
        }
        if (midOn)
        {
            tempBuffer.clear();
            midbandFilter.processToOutputBuffer (buffer, tempBuffer);
            int numChannels = buffer.getNumChannels();
            int numSamples = buffer.getNumSamples();
            for (int c = 0; c < numChannels; ++c)
                multibandBuffer.addFrom (c, 0, tempBuffer.getWritePointer(c), numSamples);
        }
        if (highOn)
        {
            tempBuffer.clear();
            highbandFilter.processToOutputBuffer (buffer, tempBuffer);
            int numChannels = buffer.getNumChannels();
            int numSamples = buffer.getNumSamples();
            for (int c = 0; c < numChannels; ++c)
                multibandBuffer.addFrom (c, 0, tempBuffer.getWritePointer(c), numSamples);
        }
    }
    else
    {
        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();
        for (int c = 0; c < numChannels; ++c)
            multibandBuffer.copyFrom (c, 0, buffer.getWritePointer(c), numSamples);
        
    }
}

}
