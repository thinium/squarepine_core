namespace djdawprocessor
{

EchoProcessor::EchoProcessor (int idNum)
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

    auto fxon = std::make_unique<AudioParameterBool> ("fxonoff", "FX On", true, "FX On/Off ", [] (bool value, int) -> String
                                                      {
                                                          if (value > 0)
                                                              return TRANS ("On");
                                                          return TRANS ("Off");
                                                          ;
                                                      });

    NormalisableRange<float> beatRange = { 0.f, 8.0 };
    StringArray options { "1/16", "1/8", "1/4", "1/2", "1", "2", "4", "8", "16" };
    auto beat = std::make_unique<AudioParameterChoice> ("beat", "Beat Division", options, 3);

    NormalisableRange<float> timeRange = { 10.f, 4000.f };
    auto time = std::make_unique<NotifiableAudioParameterFloat> ("time", "Time", timeRange, 10.f,
                                                                 true,// isAutomatable
                                                                 "Time ",
                                                                 AudioProcessorParameter::genericParameter,
                                                                 [] (float value, int) -> String
                                                                 {
                                                                     String txt (roundToInt (value));
                                                                     return txt << "ms";
                                                                     ;
                                                                 });

    NormalisableRange<float> otherRange = { 0.f, 1.0f };
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
    
    delayUnit.setDelaySamples (200 * 48);
    wetDry.setTargetValue (0.5);
    delayTime.setTargetValue (200 * 48);
    
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
    
//    syncParam = sync.get();
//    syncParam->addListener (this);

//    testParam = test.get();
//    testParam->addListener (this);
    
    auto layout = createDefaultParameterLayout (false);
    layout.add (std::move (fxon));
    layout.add (std::move (wetdry));
    addParameterWithCallback (wetDryParam, [&] (float& value) {
        wetDry.setTargetValue (jlimit (0.f, 1.f, value));
    });
    layout.add (std::move (beat));
    addParameterWithCallback (beatParam, [&] (float&) {
        DBG ("Beat");
    });
    layout.add (std::move (time));
    addParameterWithCallback (timeParam, [&] (float& value) {//delay time
        auto range = timeParam->getNormalisableRange().getRange();
        auto valMS = (float) jlimit ((int) range.getStart(), (int) range.getEnd(), roundToInt (value));
        auto t = (getSampleRate() / 1000) * valMS;
        delayTime.setTargetValue ((float) t);
    });
    layout.add (std::move (other));
    addParameterWithCallback (xPadParam, [&] (float&) {
      
    });

    layout.add (std::move (feedback));
    addParameterWithCallback (feedbackParam, [&] (float&) {
      
    });
    //setupBandParameters (layout);
    apvts.reset (new AudioProcessorValueTreeState (*this, nullptr, "parameters", std::move (layout)));

    setPrimaryParameter (wetDryParam);
    
    ParameterLinker timeParamLinker;
    
    timeParamLinker.setPrimaryParameter(timeParam);
    timeParamLinker.addParameterToLink(beatParam);
    
    linkParameters(timeParamLinker);
}

EchoProcessor::~EchoProcessor()
{
    wetDryParam->removeListener (this);
    fxOnParam->removeListener (this);
    beatParam->removeListener (this);
    timeParam->removeListener (this);
    xPadParam->removeListener (this);
    feedbackParam->removeListener (this);
}

//============================================================================== Audio processing
void EchoProcessor::prepareToPlay (double Fs, int bufferSize)
{
    const ScopedLock lock (getCallbackLock());
    InsertProcessor::prepareToPlay (Fs, bufferSize);

    delayUnit.setFs ((float) Fs);
    wetDry.reset (Fs, 0.001f);
    delayTime.reset (Fs, 0.001f);
    setRateAndBufferSizeDetails (Fs, bufferSize);
    
    sampleRate = Fs;
}
void EchoProcessor::processAudioBlock (juce::AudioBuffer<float>& buffer, MidiBuffer&)
{
    //TODO
    const auto numChannels = buffer.getNumChannels();
    const auto numSamples = buffer.getNumSamples();

    const ScopedLock lock (getCallbackLock());

    bool bypass = !fxOnParam->get();
    if (bypass)
        return;
    
    float dry, wet, x, y;
    for (int s = 0; s < numSamples; ++s)
    {
        wet = wetDry.getNextValue();
        dry = 1.f - wet;
        for (int c = 0; c < numChannels; ++c)
        {
            x = buffer.getWritePointer (c)[s];
            y = (z[c] * wet) + (x * dry);
            z[c] = delayUnit.processSample (y, c);
            buffer.getWritePointer (c)[s] = y;
        }
    }
    
}

const String EchoProcessor::getName() const { return TRANS ("Echo"); }
/** @internal */
Identifier EchoProcessor::getIdentifier() const { return "Echo" + String (idNumber); }
/** @internal */
bool EchoProcessor::supportsDoublePrecisionProcessing() const { return false; }
//============================================================================== Parameter callbacks
void EchoProcessor::parameterValueChanged (int paramIndex, float value)
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
            wetDry.setTargetValue (value);
            //
            break;
        }
        case (3):
        {
        
            break;
        }
        case (4):
        {
            float samplesOfDelay = value/1000.f * static_cast<float> (sampleRate);
            delayUnit.setDelaySamples (samplesOfDelay);
            break;
        }
        case (5):
        {    
            
            break;
        }
    }
    
}

}
