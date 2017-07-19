/** 
 * @file llwindowwin32.h
 * @brief Windows implementation of LLWindow class
 *
 * $LicenseInfo:firstyear=2001&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * 
 * Linden Research, Inc., 945 Battery Street, San Francisco, CA  94111  USA
 * $/LicenseInfo$
 */

#ifndef LL_LLWINDOWWIN32_H
#define LL_LLWINDOWWIN32_H

// Limit Windows API to small and manageable set.
#include "llwin32headerslean.h"

#include "llwindow.h"
#include "llwindowcallbacks.h"
#include "lldragdropwin32.h"

class LLWindowWin32 : public LLWindow
{
public:
	/*virtual*/ void show() override;
	/*virtual*/ void hide() override;
	/*virtual*/ void close() override;
	/*virtual*/ BOOL getVisible() override;
	/*virtual*/ BOOL getMinimized() override;
	/*virtual*/ BOOL getMaximized() override;
	/*virtual*/ BOOL maximize() override;
	/*virtual*/ void minimize() override;
	/*virtual*/ void restore() override;
	/*virtual*/ BOOL getFullscreen() override;
	/*virtual*/ BOOL getPosition(LLCoordScreen *position) override;
	/*virtual*/ BOOL getSize(LLCoordScreen *size) override;
	/*virtual*/ BOOL getSize(LLCoordWindow *size) override;
	/*virtual*/ BOOL setPosition(LLCoordScreen position) override;
	/*virtual*/ BOOL setSizeImpl(LLCoordScreen size) override;
	/*virtual*/ BOOL setSizeImpl(LLCoordWindow size) override;
	/*virtual*/ BOOL switchContext(U32 window_mode, const LLCoordScreen &size, U32 vsync_setting, const LLCoordScreen * const posp = nullptr) override;
	/*virtual*/ BOOL setCursorPosition(LLCoordWindow position) override;
	/*virtual*/ BOOL getCursorPosition(LLCoordWindow *position) override;
	/*virtual*/ void showCursor() override;
	/*virtual*/ void hideCursor() override;
	/*virtual*/ void showCursorFromMouseMove() override;
	/*virtual*/ void hideCursorUntilMouseMove() override;
	/*virtual*/ BOOL isCursorHidden() override;
	/*virtual*/ void updateCursor() override;
	/*virtual*/ ECursorType getCursor() const override;
	/*virtual*/ void captureMouse() override;
	/*virtual*/ void releaseMouse() override;
	/*virtual*/ void setMouseClipping( BOOL b ) override;
	/*virtual*/ BOOL isClipboardTextAvailable() override;
	/*virtual*/ BOOL pasteTextFromClipboard(LLWString &dst) override;
	/*virtual*/ BOOL copyTextToClipboard(const LLWString &src) override;
	/*virtual*/ void setWindowTitle(const std::string& title) override;
	/*virtual*/ void flashIcon(F32 seconds) override;
	/*virtual*/ F32 getGamma() override;
	/*virtual*/ BOOL setGamma(const F32 gamma) override; // Set the gamma
	/*virtual*/ void setFSAASamples(const U32 fsaa_samples) override;
	/*virtual*/ U32 getFSAASamples() override;
	/*virtual*/ BOOL restoreGamma() override;			// Restore original gamma table (before updating gamma)
	/*virtual*/ ESwapMethod getSwapMethod() override { return mSwapMethod; }
	/*virtual*/ void gatherInput() override;
	/*virtual*/ void delayInputProcessing() override;
	/*virtual*/ void swapBuffers() override;
	/*virtual*/ void restoreGLContext() {};

	// handy coordinate space conversion routines
	/*virtual*/ BOOL convertCoords(LLCoordScreen from, LLCoordWindow *to) override;
	/*virtual*/ BOOL convertCoords(LLCoordWindow from, LLCoordScreen *to) override;
	/*virtual*/ BOOL convertCoords(LLCoordWindow from, LLCoordGL *to) override;
	/*virtual*/ BOOL convertCoords(LLCoordGL from, LLCoordWindow *to) override;
	/*virtual*/ BOOL convertCoords(LLCoordScreen from, LLCoordGL *to) override;
	/*virtual*/ BOOL convertCoords(LLCoordGL from, LLCoordScreen *to) override;

	/*virtual*/ LLWindowResolution* getSupportedResolutions(S32 &num_resolutions) override;
	/*virtual*/ F32	getNativeAspectRatio() override;
	/*virtual*/ F32 getPixelAspectRatio() override;
	/*virtual*/ void setNativeAspectRatio(F32 ratio) override { mOverrideAspectRatio = ratio; }

	/*virtual*/	BOOL dialogColorPicker(F32 *r, F32 *g, F32 *b ) override;

	/*virtual*/ void *getPlatformWindow() override;
	/*virtual*/ void bringToFront() override;
	/*virtual*/ void focusClient() override;

	/*virtual*/ void allowLanguageTextInput(LLPreeditor *preeditor, BOOL b) override;
	/*virtual*/ void setLanguageTextInput( const LLCoordGL & pos ) override;
	/*virtual*/ void updateLanguageTextInputArea() override;
	/*virtual*/ void interruptLanguageTextInput() override;
	/*virtual*/ void spawnWebBrowser(const std::string& escaped_url, bool async) override;

	/*virtual*/ F32 getSystemUISize() override;

	LLWindowCallbacks::DragNDropResult completeDragNDropRequest( const LLCoordGL gl_coord, const MASK mask, LLWindowCallbacks::DragNDropAction action, const std::string url );

	static std::vector<std::string> getDynamicFallbackFontList();
	static void setDPIAwareness();
protected:
	LLWindowWin32(LLWindowCallbacks* callbacks,
		const std::string& title, const std::string& name, int x, int y, int width, int height, U32 flags, 
		U32 window_mode, BOOL clearBg, U32 vsync_setting, BOOL use_gl,
		BOOL ignore_pixel_depth, U32 fsaa_samples);
	~LLWindowWin32();

	void	initCursors();
	void	initInputDevices();
	HCURSOR loadColorCursor(LPCTSTR name);
	BOOL	isValid() override;
	void	moveWindow(const LLCoordScreen& position,const LLCoordScreen& size);
	LLSD	getNativeKeyData() override;

	// Changes display resolution. Returns true if successful
	BOOL	setDisplayResolution(S32 width, S32 height, S32 bits, S32 refresh);

	// Go back to last fullscreen display resolution.
	BOOL	setFullscreenResolution();

	// Restore the display resolution to its value before we ran the app.
	BOOL	resetDisplayResolution();

	BOOL	shouldPostQuit() { return mPostQuit; }

	void	fillCompositionForm(const LLRect& bounds, COMPOSITIONFORM *form);
	void	fillCandidateForm(const LLCoordGL& caret, const LLRect& bounds, CANDIDATEFORM *form);
	void	fillCharPosition(const LLCoordGL& caret, const LLRect& bounds, const LLRect& control, IMECHARPOSITION *char_position);
	void	fillCompositionLogfont(LOGFONT *logfont);
	U32		fillReconvertString(const LLWString &text, S32 focus, S32 focus_length, RECONVERTSTRING *reconvert_string);
	void	handleStartCompositionMessage();
	void	handleCompositionMessage(U32 indexes);
	BOOL	handleImeRequests(WPARAM w_param, LPARAM l_param, LRESULT *result);

protected:
	//
	// Platform specific methods
	//

	BOOL	getClientRectInScreenSpace(RECT* rectp);
	void 	updateJoystick( );

	static LRESULT CALLBACK mainWindowProc(HWND h_wnd, UINT u_msg, WPARAM w_param, LPARAM l_param);
	static BOOL CALLBACK enumChildWindows(HWND h_wnd, LPARAM l_param);


	//
	// Platform specific variables
	//
	WCHAR		*mWindowTitle;
	WCHAR		*mWindowClassName;

	HWND		mWindowHandle;	// window handle
	HGLRC		mhRC;			// OpenGL rendering context
	HDC			mhDC;			// Windows Device context handle
	HINSTANCE	mhInstance;		// handle to application instance
	WNDPROC		mWndProc;		// user-installable window proc
	RECT		mOldMouseClip;  // Screen rect to which the mouse cursor was globally constrained before we changed it in clipMouse()
	WPARAM		mLastSizeWParam;
	F32			mOverrideAspectRatio;
	F32			mNativeAspectRatio;

	HCURSOR		mCursor[ UI_CURSOR_COUNT ];  // Array of all mouse cursors

	static BOOL sIsClassRegistered; // has the window class been registered?

	F32			mCurrentGamma;
	U32			mFSAASamples;
	WORD		mPrevGammaRamp[256*3];
	WORD		mCurrentGammaRamp[256*3];

	LPWSTR		mIconResource;
	BOOL		mMousePositionModified;
	BOOL		mInputProcessingPaused;

	// The following variables are for Language Text Input control.
	// They are all static, since one context is shared by all LLWindowWin32
	// instances.
	static BOOL		sLanguageTextInputAllowed;
	static BOOL		sWinIMEOpened;
	static HKL		sWinInputLocale;
	static DWORD	sWinIMEConversionMode;
	static DWORD	sWinIMESentenceMode;
	static LLCoordWindow sWinIMEWindowPosition;
	LLCoordGL		mLanguageTextInputPointGL;
	LLRect			mLanguageTextInputAreaGL;

	LLPreeditor		*mPreeditor;

	LLDragDropWin32* mDragDrop;

	U32				mKeyCharCode;
	U32				mKeyScanCode;
	U32				mKeyVirtualKey;
	U32				mRawMsg;
	U32				mRawWParam;
	U32				mRawLParam;

	friend class LLWindowManager;
};

class LLSplashScreenWin32 : public LLSplashScreen
{
public:
	LLSplashScreenWin32();
	virtual ~LLSplashScreenWin32();

	/*virtual*/ void showImpl() override;
	/*virtual*/ void updateImpl(const std::string& mesg) override;
	/*virtual*/ void hideImpl() override;

#if LL_WINDOWS
	static LRESULT CALLBACK windowProc(HWND h_wnd, UINT u_msg, 
		WPARAM w_param, LPARAM l_param);
#endif

private:
#if LL_WINDOWS
	HWND mWindow;
#endif
};

static void	handleMessage( const MSG& msg );

S32 OSMessageBoxWin32(const std::string& text, const std::string& caption, U32 type);

#endif //LL_LLWINDOWWIN32_H
