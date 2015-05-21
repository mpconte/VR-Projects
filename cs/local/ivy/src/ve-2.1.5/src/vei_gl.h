#ifndef VE_IMPL_GL_H
#define VE_IMPL_GL_H

/** misc
    <p>
    vei_gl is an abstract implementation of an OpenGL-based VE system.
    All OpenGL implementations will provide the interfaces described
    herein.  The API itself is independent of any particular implementation.
    Applications which include the <code>vei_gl.h</code> header file do
    not need to include the OpenGL header files.  They are included 
    automatically.
    <p>
    Fair due should be given to the GLUT package (by Mark J. Kilgard) which
    is a source of much insight and code fragments.
*/

#if defined(WIN32)
#include <windows.h>
#pragma warning (disable:4244)          /* disable bogus conversion warnings */
#endif
#if defined(__APPLE__)
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif /* __APPLE__ */
#include <ve_core.h>
#include <ve_main.h>

#ifdef __cplusplus
extern "C" {
#endif
  /* The following is only there to keep the above from confusing indentation
     in Emacs */
#if 0
}
#endif

/** struct VeiGlBitmapChar
    A single bitmap character.  Adapted from the GLUT distribution.
*/
typedef struct {
  GLsizei width;
  GLsizei height;
  GLfloat xorig;
  GLfloat yorig;
  GLfloat advance;
  GLubyte *bitmap;
} VeiGlBitmapChar;

/** struct VeiGlBitmapFont
    A bitmap font as a collection of bitmap characters.  Adapted from
    the GLUT distribution.
*/
typedef struct {
  char *name;
  int num_chars;
  int first;
  int line_height;
  VeiGlBitmapChar **ch;
} VeiGlBitmapFont;

/** enum VeiGlFontType
    @value VEI_GL_FONT_NONE
    Reserved for future use and error returns.

    @value VEI_GL_FONT_BITMAP
    Indicates a bitmapped font (i.e. using VeiGlBitmapFont structures).
 */
typedef enum {
  VEI_GL_FONT_NONE,
  VEI_GL_FONT_BITMAP
} VeiGlFontType;

/** enum VeiGlJustify
    The justification that should be applied when drawing a string.
    See <code>veiGlDrawString()</code> for more information.
*/
typedef enum {
  VEI_GL_LEFT,
  VEI_GL_RIGHT,
  VEI_GL_CENTER
} VeiGlJustify;

/** struct VeiGlFont
    An abstract layer for representation of fonts.  The idea is to support
    multiple font types with the same font structure.  Currently only
    the <code>VEI_GL_FONT_BITMAP</code> font type is supported.
*/
typedef struct {
  char *name;
  VeiGlFontType type;
  void *font;
} VeiGlFont;

typedef void (*VeiGlWindowProc)(VeWindow *);
typedef void (*VeiGlDisplayProc)(VeWindow *, long, VeWallView *);
typedef void (*VeiGlPDisplayProc)(void);

/** function veiGlSetWindowCback
    Applications should set up a window callback.  This callback will
    be called once for each window after it has been created.  When
    called, the window's rendering context will be active.  This callback
    should be used for initial setup of the OpenGL environment, loading
    textures (if known ahead of time).  The callback has the following type:
    <blockquote><code>void (*VeiGlWindowProc)(VeWindow *)</code></blockquote>
    The single argument is a pointer to the window structure representing
    the window being initialized.

    @param winsetup
    The callback to set.  Only one window callback exists at a time.
    If this value is <code>NULL</code> then any existing callback is
    removed.
*/
void veiGlSetWindowCback(VeiGlWindowProc winsetup);

/** function veiGlSetDisplayCback
    Every application should create a display callback.  This is the function
    that is called once per window per frame to actually render data to that
    window.  On multiple display systems, this function may be called by
    multiple threads at the same time.  If you cannot make your rendering
    function thread-safe, be sure to lock it as a critical section
    appropriately (see the <code>ve_thread.h</code> module).  The display
    callback has the following type:
    <blockquote><code>void (*VeiGlDisplayProc)(VeWindow *w, long tm, VeWallView *wv)</code></blockquote>
    The arguments are:
    <ul>
    <li><i>w</i> - a pointer to the window structure for the window being
    rendered.
    <li><i>tm</i> - a timestamp (relative to the clock) for the time that
    this frame is expected to be rendered at.  This is an estimate.
    <li><i>wv</i> - a pointer to the wall view structure indicating the
    geometry for this window.
    </ul>
    <p>Rendering functions should not initialize projection or view
    matricies (the VE library handles that) but may multiply the view
    matrix if necessary for rendering.  Proper matrix stack handling
    should be observed (i.e. appropriate push/pop call nesting).
    
    @param dpy
    The display callback to set.
 */
void veiGlSetDisplayCback(VeiGlDisplayProc dpy);

/** function veiGlSetPreDisplayCback
    An application can set a function to be called immediately before
    all windows are displayed.  Unlike the display callback itself, this
    function is only called once per frame (rather than once per thread).
    See also <code>veiGlSetPostDisplayCback()</code>.  The callback must
    have the following type:
    <blockquote><code>typedef void (*VeiGlPDisplayProc)(void);</code></blockquote>

    @param cback
    The new callback to set.  If this value is <code>NULL</code> then
    any existing callback is cleared.
*/
void veiGlSetPreDisplayCback(VeiGlPDisplayProc cback);

/** function veiGlSetPostDisplayCback
    An application can set a function to be called immediately after
    all windows are displayed.  Unlike the display callback itself, this
    function is only called once per frame (rather than once per thread).
    See also <code>veiGlSetPostDisplayCback()</code>.  The callback must
    have the following type:
    <blockquote><code>typedef void (*VeiGlPDisplayProc)(void);</code></blockquote>

    @param cback
    The new callback to set.  If this value is <code>NULL</code> then
    any existing callback is cleared.
*/
void veiGlSetPostDisplayCback(VeiGlPDisplayProc cback);

/** function veiGlSetupWindow
    Indicates that a window should be setup by calling the window callback.
    If no window callback is set then this function has no effect.
    This function is generally not called from applications.  It is mostly
    used by specific OpenGL implementations.

    @param w
    The window to initialize.
*/
void veiGlSetupWindow(VeWindow *w);

/** function veiGlDisplayWindow
    Indicates that a window should be redrawn by calling the display callback.
    If no display callback has been set then this function has no effect.
    The parameters are the parameters that should be sent to the callback.
    This function is generally not called from applications.  It is mostly
    used by specific OpenGL implementations.
*/
void veiGlDisplayWindow(VeWindow *w, long tm, VeWallView *wv);

/** function veiGlPreDisplay
    Asks the common OpenGL layer to do any pre-display processing.
    This is only used by specific OpenGL implementations.
*/
void veiGlPreDisplay(void);

/** function veiGlPostDisplay
    Asks the common OpenGL layer to do any pre-display processing.
    This is only used by specific OpenGL implementations.
*/
void veiGlPostDisplay(void);

/** function veiGlGetOption
    Used internally to retrieve run-time options for a specific window.
    Values are searched for in the following order, stopping at the first
    value found:
    <ul>
    <li>In the given VeWindow structure</li>
    <li>Values returned from <code>veGetOption</code>, usually specified
    on the command-line using the "-ve_opt" option.</li>
    <li>In the associated VeWall structure</li>
    <li>In the associated VeEnv structure</li>
    </ul>

    @param name
    The name of the value to find.
    
    @returns
    A pointer to the value if found and <code>NULL</code> otherwise.
    The returned value should be treated as "read-only".
*/
char *veiGlGetOption(VeWindow *w, char *name);

/** function veiGlPushTextMode
    Sets the window up for drawing text with a 2D co-ordinate system
    running from (0,0) to (<i>w</i>,<i>h</i>) where <i>w</i> and <i>h</i>
    are the width and height of the window respectively.  The existing
    view and projection matrices are saved and can be restored with
    <code>veiGlPopTextMode()</code>.
    
    @param w
    The window on which we are pushing the text mode.  This should be the
    window for which the rendering context is currently active.
*/
void veiGlPushTextMode(VeWindow *w);

/** function veiGlPopTextMode
    Resets the viewing and projection matrices after having previously
    entered text mode.
*/
void veiGlPopTextMode();

/** function veiGlGetFont
    Looks up an internally defined font.  This function is only used to
    access fonts that are internal to the library.  Currently the following
    fonts are defined:
    <ul>
    <li>6x10
    <li>7x13
    <li>8x13
    <li>9x15
    </ul>
    A utility is included with the VE library (capturexfont) for generating
    VeiGlFont structures from X fonts in the "utils" directory of the
    distribution.  This program was directly ported from the GLUT distribution.

    @param name
    The name of the font you are looking for.  Currently, only the above
    names are recognized.
*/
VeiGlFont *veiGlGetFont(char *name);

/** function veiGlStringHeight
    Calculates the height of a string in pixels as it would be rendered
    with the given font.  This string is not actually rendered on screen.
    
    @param font
    The font the string would be rendered with.

    @param string
    The string that would be rendered.

    @returns
    The height of the rendered string in pixels.
 */
int veiGlStringHeight(VeiGlFont *font, char *string);

/** function veiGlStringWidth
    Calculates the width of a string in pixels as it would be rendered
    with the given font.  This string is not actually rendered on screen.
    
    @param font
    The font the string would be rendered with.

    @param string
    The string that would be rendered.

    @returns
    The width of the rendered string in pixels.
 */
int veiGlStringWidth(VeiGlFont *font, char *string);

/** function veiGlDrawChar
    Draws a single character, advancing the horizontal raster position.
    The character will be drawn at the current OpenGL raster position.
    
    @param font
    The font to render with.

    @param c
    The character to render.
*/
void veiGlDrawChar(VeiGlFont *font, int c);

/** function veiGlDrawString
    Draws a string at the current raster position.  The baseline of
    the string will be located at the current vertical raster position.
    The string will be aligned horizontally according to the <i>justify</i>
    parameter.  If this value is <code>VEI_GL_LEFT</code> then the
    horizontal raster position is the left side of the string.  If the value
    is <code>VEI_GL_RIGHT</code> then the horizontal raster position is the
    right side of the string.  If the value is <code>VEI_GL_CENTER</code>
    then the horizontal raster position is the center of the string.
    After drawing the string, the horizontal raster position will be located
    at the right of the string.  Vertical positioning characters
    (newlines, carriage returns, vertical tabs, etc.) do not change the
    vertical raster position and should be excluded from strings passed
    to this function.
    
    @param font
    The font to draw with.

    @param string
    The string to draw.
    
    @param justify
    Where, horizontally, to draw the string relative to the current
    raster position.
*/
void veiGlDrawString(VeiGlFont *font, char *string, VeiGlJustify justify);

/** function veiGlDrawStatistics
    A convenience function to draw all currently known statistics 
    (up to <i>max</i> statistics) using the given font.  Useful for
    debugging and profiling.  The strings will be drawn left-justified
    in the lower-left corner of the current window.

    @param font
    The font to draw with.

    @param max
    The maximum number of statistics to draw.  If this value is less
    than or equal to 0 then every available statistic is drawn.
*/
void veiGlDrawStatistics(VeiGlFont *font, int max);

/** function veiGlRenderMonoWindow
    Does the actual rendering of a window for a given eye.
    Calls the application's display callback as necessary.
    This function should not be called by an application - it is meant
    for use by specific OpenGL implementations.
    
    @param w
    The window to be rendered.

    @param tm
    An estimate of the time at which this window will be displayed.
    
    @param eye
    The eye for which this window is being rendered.

    @param steye
    Which stereo eye is being rendered - either <code>VE_EYE_LEFT</code>,
    <code>VE_EYE_RIGHT</code> or <code>VE_EYE_MONO</code>.
*/
void veiGlRenderMonoWindow(VeWindow *w, long tm, VeFrame *eye, int steye);


/** function veiGlRenderWindow
    Renders a window using veiGlRenderMonoWindow for the actual work.
    Determines how many buffers need to be rendered in the current window
    (e.g. two for stereo), and which eye (mono, left, right) applies to
    these buffers.  Assumes that the appropriate platform-dependent
    context is appropriately set.  This function should not be called by
    an application - it is meant for use by specific OpenGL implementations.
*/
void veiGlRenderWindow(VeWindow *w);

/** function veiGlParseGeom
    Parses a "geometry" string which describes the width and height and
    optionally the location of a window.  The string must have one of 
    two formats: <code><i>w</i>x<i>h</i></code> or <code><i>w</i>x<i>h</i>[+-]<i>x</i>[+-]<i>y</i></code>.  In the former case, x and y are set to 0.
    <p>If there is a parsing error, a fatal error is generated.</p>

    @param geom
    The geometry string to parse.

    @param w,h,x,y
    Pointers to the variables where the geometry values will be stored.
    All pointers must be non-NULL.
*/
void veiGlParseGeom(char *geom, int *x, int *y, int *w, int *h);

/** function veGlUpdateFrameRate
    To be called by OpenGL implementations when they have finished rendering
    a frame to allow the built-in frame-rate statistic to be updated.
    Applications should not call this function.
*/
void veiGlUpdateFrameRate(void);

/** function veiGlRenderRun
    This function should be called by the implementation-specific
    render activation routine (e.g. the GLX implementation of veRenderRun)
    in order to initialize common OpenGL pieces of the library.  Applications
    should not call this function.
*/
void veiGlRenderRun(void);

/** function veiGlIsMaster
    On systems where shared contexts are available this function allows
    the detection of masters vs. shared contexts.  Textures and display
    lists should only be initialized in master contexts for best performance.
    Once initialized in a master context, the same ids can be used in shared
    contexts.
    
    @returns
    1 if this is a master context, 0 if this is a shared context.
*/
int veiGlIsMaster(void);

/** function veiGlInit
    This function should be called by the implementation-specific
    initialization routine (e.g. the GLX initialization routine) in
    order to initialize common OpenGL pieces of the library.  Applications
    should not call this function.
*/
void veiGlInit(void);

#ifdef __cplusplus
}
#endif

#endif /* VE_IMPL_GL_H */
