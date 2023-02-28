namespace djdawprocessor
{

PingPongProcessor::PingPongProcessor (int idNum)
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

    StringArray options { "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16" };
    auto beat = std::make_unique<AudioParameterChoice> ("beat", "Beat Division", options, 3);

    NormalisableRange<float> timeRange = { 1.f, 4000.f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("time", "Time", timeRange, 500.f,
                                                                 true,// isAutomatable
                                                                 "Time ",
                                                                 AudioProcessorParameter::genericParameter,
                                                                 [] (float value, int) -> String
                                                                 {
                                                                     String txt (roundToInt (value));
                                                                     return txt << "ms";
                                                                     ;
                                                                 });

    NormalisableRange<float> beatRange = { 0.f, 8.0 };

    auto other = std::make_unique<NotifiableAudioParameterFloat> ("x Pad", "X Pad Division", beatRange, 3,
                                                                  false,// isAutomatable
                                                                  "X Pad Division ",
                                                                  AudioProcessorParameter::genericParameter,
                                                                  [] (float value, int) -> String
                                                                  {
                                                                      int val = roundToInt (value);
                                                                      String txt;
                                                                      switch (val)
                                                                      {
                                                                          case 0:
                                                                              txt = "1/16";
                                                                              break;
                                                                          case 1:
                                                                              txt = "1/8";
                                                                              break;
                                                                          case 2:
                                                                              txt = "1/4";
                                                                              break;
                                                                          case 3:
                                                                              txt = "1/2";
                                                                              break;
                                                                          case 4:
                                                                              txt = "1";
                                                                              break;
                                                                          case 5:
                                                                              txt = "2";
                                                                              break;
                                                                          case 6:
                                                                              txt = "4";
                                                                              break;
                                                                          case 7:
                                                                              txt = "8";
                                                                              break;
                                                                          case 8:
                                                                              txt = "16";
                                                                              break;
                                                                          default:
                                                                              txt = "1";
                                                                              break;
                                                                      }

                                                                      return txt;
                                                                  });
    
    NormalisableRange<float> feedbackRange = { 0.f, 1.0f };
    auto feedback = std::make_unique<NotifiableAudioParameterFloat> ("feedback", "Feedback", feedbackRange, 0.5f,
                                                                     true,// isAutomatable
                                                                     "Feedback ",
                                                                     AudioProcessorParameter::genericParameter,
                                                                     [] (float value, int) -> String {
                                                                         int percentage = roundToInt (value * 100);
                                                                         String txt (percentage);
                                                                         return txt << "%";
                                                                     });

    wetDryParam = wetdry.get();
    wetDryParam->addListener (this);

    fxOnParam = fxon.get();
    fxOnParam->addListener (this);

    beatParam = beat.get();
    beatParam->addListener (this);

    timeParam = time.get();
    timeParam->addListener (this);

    xPadParam = other.get();
    xPadParam->addListener (this);
    
    feedbackParam = feedback.get();
    feedbackParam->addListener (this);

    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    layout.add (std::move (beat));
    layout.add (std::move (time));
    layout.add (std::move (feedback));
    layout.add (std::move (other));
    setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
    
}

PingPongProcessor::~PingPongProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    beatParam->removeListener (this);
    timeParam->removeListener (this);
    xPadParam->removeListener (this);
    feedbackParam->removeListener (this);
}

//============================================================================== Audio processing
void PingPongProcessor::prepareToPlay (double sampleRate, int bufferSize)
{
    BandProcessor::prepareToPlay (sampleRate, bufferSize);
    Fs = static_cast<float> (sampleRate);
    delayLeft.setFs (Fs);
    delayRight.setFs (Fs);
    
    float samplesOfDelay = timeParam->get()/1000.f * static_cast<float> (sampleRate);
    delayLeft.setDelaySamples (samplesOfDelay);
    delayRight.setDelaySamples (samplesOfDelay);
}
void PingPongProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    
    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    
    float wet;
    bool bypass;
    float feedback;
    {
        const ScopedLock sl (getCallbackLock());
        wet = wetDryParam->get();
        bypass = !fxOnParam->get();
        feedback = feedbackParam->get();
    }
    
    if (bypass)
        return;
    
    fillMultibandBuffer (buffer);
    
    for (int n = 0; n < numSamples ; ++n)
    {
        float xL = multibandBuffer.getWritePointer(0) [n];
        float xR = multibandBuffer.getWritePointer(1) [n];

        float xSum = xL + xR;
        
        float dL = delayLeft.processSample (xSum + feedback * z,0);
        z = delayRight.processSample (dL,0);

        wetSmooth = 0.999f * wetSmooth + 0.001f * wet;
        
        //float dryAmp = 1.f - wetSmooth;
        
        //multibandBuffer.getWritePointer(0) [n] = dryAmp * xL + wetSmooth * dL;
        //multibandBuffer.getWritePointer(1) [n] = dryAmp * xR + wetSmooth * z;
        multibandBuffer.getWritePointer(0) [n] = dL;
        multibandBuffer.getWritePointer(1) [n] = z;
    }
        
    multibandBuffer.applyGain (wet);
    //buffer.applyGain (1.f-wet);
    
    for (int c = 0; c < numChannels; ++c)
        buffer.addFrom (c, 0, multibandBuffer.getWritePointer(c), numSamples);
}

const String PingPongProcessor::getName() const { return TRANS ("Ping Pong"); }
/** @internal */
Identifier PingPongProcessor::getIdentifier() const { return "Ping Pong" + String (idNumber); }
/** @internal */
bool PingPongProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void PingPongProcessor::parameterValueChanged (int paramIndex, float value)
{
    //If the beat division is changed, the delay time should be set.
    //If the X Pad is used, the beat div and subsequently, time, should be updated.

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
        
            break;
        }
        case (4):
        {
            float samplesOfDelay = value/1000.f * Fs;
            delayLeft.setDelaySamples (samplesOfDelay);
            delayRight.setDelaySamples (samplesOfDelay);
            break;
        }
        case (5):
        {
            
            break;
        }
    }
}

}
