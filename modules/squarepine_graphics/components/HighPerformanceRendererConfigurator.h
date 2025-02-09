//==============================================================================
#if JUCE_MODULE_AVAILABLE_juce_opengl
   #ifndef GL_VENDOR
    #define GL_VENDOR 0x1F00
   #endif

   #ifndef GL_RENDERER
    #define GL_RENDERER 0x1F01
   #endif

   #ifndef GL_VERSION
    #define GL_VERSION 0x1F02
   #endif

   #ifndef GL_EXTENSIONS
    #define GL_EXTENSIONS 0x1F03
   #endif

   #ifndef GL_MAJOR_VERSION
    #define GL_MAJOR_VERSION 0x821b
   #endif

   #ifndef GL_MINOR_VERSION
    #define GL_MINOR_VERSION 0x821c
   #endif

    /** @returns a juce::String from an OpenGL style string. */
    String getGLString (GLenum value);

    /** Attemps configuring an OpenGL context with version 3.1 and fancy things
        like continuous repainting.

        By default, this attempts to enable multisampling.
    */
    void configureContextWithModernGL (OpenGLContext& context, bool shouldEnableMultisampling = true);

    /** Logs any GPU/driver related information it can find.

        Note that this must be called from the OpenGL rendering thread
        or it won't work at best, or crash at worst!
    */
    void logOpenGLInfoCallback (OpenGLContext&);
#endif

//==============================================================================
/** A type of OpenGL configurator that will automatically select the
    most performant means of rendering the targeted Component.

    If the driver supplies OpenGL 3.1, this will configure the window to use that.
    Otherwise the software renderer will be used.
*/
class HighPerformanceRendererConfigurator
{
public:
    /** Constructor. */
    HighPerformanceRendererConfigurator() = default;

    /** Destructor. */
    virtual ~HighPerformanceRendererConfigurator() = default;

    //==============================================================================
    /** This will try to configure the window with OpenGL.

        If OpenGL is unavailable on the running platform, this function will do nothing.

        If the version of OpenGL present is too old (ie: pre 3.1),
        the context will be detached automatically (on the message thread),
        only to fallback to software rendering.

        @warning You must call this BEFORE calling DocumentWindow::setContentOwned().
    */
    virtual void configureWithOpenGLIfAvailable (Component& component);

    /** Call this in your rendering or painting routine. */
    void paintCallback();

    //==============================================================================
   #if JUCE_MODULE_AVAILABLE_juce_opengl
    std::unique_ptr<OpenGLContext> context;
   #endif

private:
    //==============================================================================
    class DetachContextMessage;
    friend class DetachContextMessage;
    std::atomic<bool> hasContextBeenForciblyDetached { false };

    //==============================================================================
    JUCE_DECLARE_WEAK_REFERENCEABLE (HighPerformanceRendererConfigurator)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HighPerformanceRendererConfigurator)
};
