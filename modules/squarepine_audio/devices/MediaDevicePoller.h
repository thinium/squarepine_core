/** Use an instance of this class to find out when
    an audio or MIDI device has been inserted or removed.
*/
class MediaDevicePoller final : private AsyncUpdater,
                                private Timer
{
public:
    /** Constructor

        @param audioDeviceManager The device manager to use for getting the lists
                                      of audio and MIDI devices
    */
    MediaDevicePoller (AudioDeviceManager& audioDeviceManager);

    /** Destructor. */
    ~MediaDevicePoller() override;

    //==============================================================================
    /** Obtain the most current list of audio input devices. */
    StringArray getListOfAudioInputDevices();

    /** Obtain the most current list of audio output devices. */
    StringArray getListOfAudioOutputDevices();

    /** */
    StringArray getListOfInputChannelNames();

    /** */
    StringArray getListOfOutputChannelNames();

    /** Obtain the most current list of MIDI input devices. */
    StringArray getListOfMIDIInputDevices();

    /** Obtain the most current list of MIDI output devices. */
    StringArray getListOfMIDIOutputDevices();

    /** */
    struct DeviceInfo
    {
        /** */
        String deviceName;
        /** */
        StringArray channelInfo;
    };

    /** */
    DeviceInfo getCurrentInputDeviceInfo();
    /** */
    DeviceInfo getCurrentOutputDeviceInfo();

    //==============================================================================
    /** Inherit from this Listener to check for MIDI or audio devices being added or removed!

        @note Bear in mind that device changes will occur on driver changes!
    */
    class Listener
    {
    public:
        /** */
        virtual ~Listener() { }

        /** */
        virtual void driverChanged (const String& driverName) = 0;
        /** */
        virtual void numInputsChanged (int numInputs, bool added) = 0;
        /** */
        virtual void numOutputsChanged (int numOutputs, bool added) = 0;
        /** */
        virtual void deviceAdded (const String& deviceName) = 0;
        /** */
        virtual void deviceRemoved (const String& deviceName) = 0;
    };

    /** Registers a listener that will be called when something changes. */
    void addListener (Listener* listener);

    /** Deregisters a previously-registered listener. */
    void removeListener (Listener* listener);

private:
    //==============================================================================
    AudioDeviceManager& deviceManager;
    ListenerList<Listener> listeners;
    String lastAPI, driverAPI;
    int numInputs = 0, lastNumInputs = 0, numOutputs = 0, lastNumOutputs = 0;
    StringArray currentAudioInputDevices, currentAudioOutputDevices, currentMidiInputDevices, currentMidiOutputDevices;
    String lastDeviceChanged;

    enum class ChangeType
    {
        noChange,
        driverChanged,
        deviceAdded,
        deviceRemoved,
        inputsChanged,
        outputsChanged
    };

    ChangeType changeType = ChangeType::noChange;

    //==============================================================================
    StringArray getListOfAudioDevices (bool giveMeInputDevices);
    StringArray getListOfChannelNames (bool giveMeInputDevices);
    DeviceInfo getCurrentDeviceInfo (bool giveMeInputDevices);
    void checkForDeviceChange (const StringArray& arrayNew, StringArray& arrayOld);
    static bool wasDeviceAdded (const StringArray& newList, const StringArray& oldList) noexcept;
    static String getChangedDeviceName (const StringArray& newList, const StringArray& oldList);

    //==============================================================================
    /** @internal */
    void setupDeviceLists();
    /** @internal */
    void handleAsyncUpdate() override;
    /** @internal */
    void timerCallback() override;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MediaDevicePoller)
};
