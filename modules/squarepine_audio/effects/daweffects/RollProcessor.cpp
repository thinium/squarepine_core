namespace djdawprocessor
{

RollProcessor::RollProcessor (int idNum)
    : idNumber (idNum)
{
    reset();

    NormalisableRange<float> wetDryRange = { 0.f, 1.f };
    auto wetdry = std::make_unique<NotifiableAudioParameterFloat> ("dryWet", "Dry/Wet", wetDryRange, 0.5f,
                                                                   true,// isAutomatable
                                                                   "Dry/Wet",
                                                                   AudioProcessorParameter::genericParameter,
                                                                   [] (float value, int) -> String
                                                                   {
                                                                       int percentage = roundToInt (value * 100);
                                                                       String txt (percentage);
                                                                       return txt << "%";
                                                                   });

    auto fxon = std::make_unique<NotifiableAudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String {
        if (value > 0)
            return TRANS ("On");
        return TRANS ("Off");
        ;
    });

    NormalisableRange<float> timeRange = { 1.f, 4000.f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("time", "Time", timeRange, 500.f,
                                                                 true,// isAutomatable
                                                                 "Time ",
                                                                 AudioProcessorParameter::genericParameter,
                                                                 [] (float value, int) -> String
                                                                 {
                                                                     String txt (roundToInt (value));
                                                                     return txt;
                                                                     ;
                                                                 });

    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);

    fxOnParam = fxon.get();
    fxOnParam->addListener (this);

    timeParam = time.get();
    timeParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (time));
    setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
    
    bool effectIsOn = fxOnParam->get();
    if (effectIsOn)
        fillSegmentFlag = true;
}

RollProcessor::~RollProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    timeParam->removeListener (this);
}

//============================================================================== Audio processing
void RollProcessor::prepareToPlay (double Fs, int bufferSize)
{
    BandProcessor::prepareToPlay (Fs, bufferSize);
    
    sampleRate = Fs;
    delayTimeInSamples = static_cast<int> (round (sampleRate * timeParam->get()/1000.0));
    
    int numChannels = 2;
    int segmentLenSec = 16;
    maxSegmentIndex = static_cast<int> (Fs) * segmentLenSec;
    segmentBuffer.setSize (numChannels, maxSegmentIndex);
    segmentBuffer.clear();
    
    tempBuffer.setSize (numChannels, bufferSize);
    tempBuffer.clear();
}
void RollProcessor::processAudioBlock (juce::AudioBuffer<float> & buffer, MidiBuffer&)
{
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    bool bypass;
    float wet;
    float dry;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        dry = 1.f - wetDryParam->get();
        bypass = !fxOnParam->get();
    }
    
    if (bypass)
        return;

    if (fillSegmentFlag)
        fillSegmentBuffer (buffer);

    fillTempBuffer(); // copy 1024 samples from segmentBuffer to tempBuffer for multiband processing
    
    fillMultibandBuffer (tempBuffer);

    multibandBuffer.applyGain (wet);
    buffer.applyGain (dry);
    
    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer(c), numSamples);
}

const String RollProcessor::getName() const { return TRANS ("Roll"); }
/** @internal */
Identifier RollProcessor::getIdentifier() const { return "Roll" + String (idNumber); }
/** @internal */
bool RollProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void RollProcessor::parameterValueChanged (int id, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.

    //Subtract the number of new parameters in this processor
    BandProcessor::parameterValueChanged (id, value);
    
    const ScopedLock sl (getCallbackLock());
    switch (id)
    {
        case (1):
        {
            bool effectIsOn = fxOnParam->get();
            if (effectIsOn)
            {
                fillSegmentFlag = true; // for this effect, only reset when change in on/off
                delayTimeInSamples = static_cast<int> (round (sampleRate * timeParam->get()/1000.0));
                segmentFillIndex = 0;
                segmentPlayIndex = 0;
            }
            break;
        }
        case (2):
        {
            //wetDry handled in processBlock
            //
            break;
        }
        case (3):
        {
            delayTimeInSamples = static_cast<int> (round (sampleRate * timeParam->get()/1000.0));
            break; // time
        }
    }
    
}

void RollProcessor::fillSegmentBuffer (AudioBuffer<float> & buffer)
{
    
    int bufferSize = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();
    
    if (segmentFillIndex + bufferSize < maxSegmentIndex)
    {
        // Condition when there is enough space to copy entire buffer to segmentBuffer
        for (int c = 0; c < numChannels; ++c)
            segmentBuffer.copyFrom (c, segmentFillIndex, buffer.getWritePointer(c), bufferSize);
        
        segmentFillIndex += bufferSize; // used for checking whether fillSegmentFlag should be changed
    }
    else
    {
        // Condition when there is only enough space to copy part of buffer to segmentBuffer
        int numSamples = maxSegmentIndex - segmentFillIndex;
        for (int c = 0; c < numChannels; ++c)
            segmentBuffer.copyFrom (c, segmentFillIndex, buffer.getWritePointer(c), numSamples);
        
        segmentFillIndex += numSamples; // used for checking whether fillSegmentFlag should be changed
    }
    
    if (segmentFillIndex > maxSegmentIndex)
        fillSegmentFlag = false;
}


void RollProcessor::fillTempBuffer()
{
    tempBuffer.clear();
    int bufferSize = tempBuffer.getNumSamples();
    int numChannels = tempBuffer.getNumChannels();
    
    if (segmentPlayIndex + bufferSize < delayTimeInSamples)
    {
        int currentIndex = segmentPlayIndex; // save to use for each channel
        // Condition when there is enough space to copy entire segmentBuffer to tempBuffer
        for (int c = 0; c < numChannels; ++c)
        {
            segmentPlayIndex = currentIndex;
            for (int n = 0 ; n < bufferSize; ++n)
            {
                tempBuffer.getWritePointer(c)[n] = segmentBuffer.getWritePointer(c)[segmentPlayIndex++];
            }
        }
    }
    else
    {
        // Condition when the end of segmentBuffer is copied to beginning tempBuffer, then restarts at beginning of segmentBuffer
        int numSamples = delayTimeInSamples - segmentPlayIndex; // remaining samples to use before restarting
        int currentIndex = segmentPlayIndex; // save to use for each channel
        
        // This first loop copies the remaining samples in segmentBuffer to the beginning of tempBuffer
        for (int c = 0; c < numChannels; ++c)
        {
            segmentPlayIndex = currentIndex;
            for (int n = 0 ; n < numSamples; ++n)
            {
                tempBuffer.getWritePointer(c)[n] = segmentBuffer.getWritePointer(c)[segmentPlayIndex++];
            }
          
        }
        
        // Reset the index of the segment back to the beginning for the next part
        currentIndex = 0;
        numSamples = bufferSize - numSamples;
        // Start copying the beginning of segmentBuffer to fill up the rest of tempBuffer
        for (int c = 0; c < numChannels; ++c)
        {
            segmentPlayIndex = currentIndex; // reset for each channel
            for (int n = numSamples ; n < bufferSize; ++n)
            {
                tempBuffer.getWritePointer(c)[n] = segmentBuffer.getWritePointer(c)[segmentPlayIndex++];
            }
        }
    }
}

}
