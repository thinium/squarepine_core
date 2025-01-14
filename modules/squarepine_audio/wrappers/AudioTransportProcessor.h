/** An AudioSource object wrapped nicely in an AudioProcessor.

    Simply call setSource() to change the source!
*/
class AudioTransportProcessor final : public InternalProcessor
{
public:
    /** Constructor. */
    AudioTransportProcessor();

    /** Destructor. */
    ~AudioTransportProcessor() override;

    //==============================================================================
    /** */
    void play();
    /** */
    void playFromStart();
    /** */
    void stop();

    /** */
    void setLooping (bool shouldLoop);

    /** */
    bool isLooping() const;
    /** */
    bool isPlaying() const;

    /** */
    void setCurrentTime (double seconds);
    /** */
    void setCurrentTime (int64 samples);
    /** */
    void setResamplingRatio (double newRatio);

    /** */
    double getLengthSeconds() const noexcept;
    /** */
    int64 getLengthSamples() const noexcept;

    /** */
    double getCurrentTimeSeconds() const noexcept;
    /** */
    int64 getCurrentTimeSamples() const noexcept;

    /** */
    void clear();

    //==============================================================================
    /** */
    void setSource (PositionableAudioSource* source,
                    int readAheadBufferSize = 0,
                    TimeSliceThread* readAheadThread = nullptr,
                    double sourceSampleRateToCorrectFor = 0.0,
                    int maxNumChannels = 2);

    /** */
    void setSource (AudioFormatReaderSource* readerSource,
                    int readAheadBufferSize = 0,
                    TimeSliceThread* readAheadThread = nullptr);

    //==============================================================================
    /** @internal */
    void prepareToPlay (double, int) override;
    /** @internal */
    void processBlock (juce::AudioBuffer<float>&, MidiBuffer&) override;
    /** @internal */
    void releaseResources() override;
    /** @internal */
    bool isInstrument() const override { return true; }
    /** @internal */
    const String getName() const override;
    /** @internal */
    Identifier getIdentifier() const override;

private:
    //==============================================================================
    AudioSourceProcessor audioSourceProcessor;
    AudioTransportSource* transport = nullptr;
    PositionableAudioSource* source = nullptr;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioTransportProcessor)
};
