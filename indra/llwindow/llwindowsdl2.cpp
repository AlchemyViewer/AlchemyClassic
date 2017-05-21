// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file llwindowsdl2.cpp
 * @brief SDL implementation of LLWindow class
 * @author This module has many fathers, and it shows.
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

#if LL_SDL2

#include "linden_common.h"

#include "llwindowsdl2.h"

#include "llwindowcallbacks.h"
#include "llkeyboardsdl2.h"

#include "llerror.h"
#include "llgl.h"
#include "llglslshader.h"
#include "llstring.h"
#include "lldir.h"
#include "llfindlocale.h"
#include "llsdutil.h"

#include "SDL2/SDL_messagebox.h"
#include "SDL2/SDL_syswm.h"

#if LL_WINDOWS
#include "../newview/res/resource.h"
#endif

#include <array>

extern BOOL gDebugWindowProc;

const S32 MAX_NUM_RESOLUTIONS = 200;

//
// LLWindowSDL2
//

// Stash a pointer to the window implementation for use in static functions
static LLWindowSDL2 *gWindowImplementation = nullptr;

LLWindowSDL2::LLWindowSDL2(LLWindowCallbacks* callbacks,
	const std::string& title, const std::string& name, S32 x, S32 y, S32 width,
	S32 height, U32 flags,
	U32 window_mode, BOOL clearBg,
	U32 vsync_setting, BOOL use_gl,
	BOOL ignore_pixel_depth, U32 fsaa_samples)
	: LLWindow(callbacks, window_mode, flags),
	mGamma(1.0f)
{
	SDL_SetMainReady();

	if (SDL_Init(0) != 0)
	{
		LL_WARNS() << "Failed to initialize SDL due to error: " << SDL_GetError() << LL_ENDL;
		return;
	}

	// Initialize the keyboard
	gKeyboard = new LLKeyboardSDL2();
	gKeyboard->setCallbacks(callbacks);
	// Note that we can't set up key-repeat until after SDL has init'd video

	// Ignore use_gl for now, only used for drones on PC
	mWindow = nullptr;
	mGLContext = nullptr;
	mNeedsResize = FALSE;
	mOverrideAspectRatio = 0.f;
	mGrabbyKeyFlags = 0;
	mReallyCapturedCount = 0;
	mFSAASamples = fsaa_samples;

	// Assume 4:3 aspect ratio until we know better
	mOriginalAspectRatio = 1024.0 / 768.0;

	mWindowName = name;

	if (title.empty())
		mWindowTitle = "Alchemy Viewer";
	else
		mWindowTitle = title;

	// Create the GL context and set it up for windowed or fullscreen, as appropriate.
	if (createContext(x, y, width, height, 32, window_mode, vsync_setting))
	{
		gGLManager.initGL();

		//start with arrow cursor
		initCursors();
		setCursor(UI_CURSOR_ARROW);
	}

	stop_glerror();

	// Stash an object pointer for OSMessageBox()
	gWindowImplementation = this;

	mKeyScanCode = 0;
	mKeyVirtualKey = 0;
	mKeyModifiers = KMOD_NONE;
}

#if LL_WINDOWS
void LLWindowSDL2::setIconWindows()
{
	HINSTANCE handle = ::GetModuleHandle(NULL);
	mWinWindowIcon = LoadIcon(handle, MAKEINTRESOURCE(IDI_LL_ICON));

	HWND hwnd = (HWND) getPlatformWindow();

	::SetClassLongPtr(hwnd, GCLP_HICON, (LONG_PTR) mWinWindowIcon);
}
#endif

static SDL_Surface *Load_BMP_Resource(const char *basename)
{
	// Figure out where our BMP is living on the disk
	std::string path = llformat("%s%sres-sdl%s%s",
		gDirUtilp->getAppRODataDir().c_str(),
		gDirUtilp->getDirDelimiter().c_str(),
		gDirUtilp->getDirDelimiter().c_str(),
		basename);

	return SDL_LoadBMP(path.c_str());
}

BOOL LLWindowSDL2::createContext(int x, int y, int width, int height, int bits, U32 window_mode, U32 vsync_setting)
{
	LL_INFOS() << "createContext, fullscreen=" << window_mode <<
		" size=" << width << "x" << height << LL_ENDL;

	// captures don't survive contexts
	mGrabbyKeyFlags = 0;
	mReallyCapturedCount = 0;

	mWindowMode = window_mode;

#if LL_WINDOWS
	char* name_char = const_cast<char*>(mWindowName.c_str());
	SDL_RegisterApp(name_char, CS_HREDRAW | CS_VREDRAW | CS_OWNDC, GetModuleHandle(NULL));
#endif

	if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
	{
		LL_INFOS() << "sdl_init() failed! " << SDL_GetError() << LL_ENDL;
		setupFailure("sdl_init() failure,  window creation error", "error", OSMB_OK);
		return false;
	}

	SDL_version c_sdl_version;
	SDL_VERSION(&c_sdl_version);
	LL_INFOS() << "Compiled against SDL "
		<< int(c_sdl_version.major) << "."
		<< int(c_sdl_version.minor) << "."
		<< int(c_sdl_version.patch) << LL_ENDL;
	SDL_version r_sdl_version;
	SDL_GetVersion(&r_sdl_version);
	LL_INFOS() << " Running against SDL "
		<< int(r_sdl_version.major) << "."
		<< int(r_sdl_version.minor) << "."
		<< int(r_sdl_version.patch) << LL_ENDL;

	SDL_DisplayMode current_mode;
	if (SDL_GetCurrentDisplayMode(0, &current_mode) != 0)
	{
		LL_INFOS() << "SDL_GetCurrentDisplayMode failed! " << SDL_GetError() << LL_ENDL;
		setupFailure("SDL_GetCurrentDisplayMode failed!", "Error", OSMB_OK);
		return FALSE;
	}

	if (current_mode.h > 0)
	{
		mOriginalAspectRatio = (double) current_mode.w / (double) current_mode.h;
		LL_INFOS() << "Original aspect ratio was " << current_mode.w << ":" << current_mode.h << "=" << mOriginalAspectRatio << LL_ENDL;

	}

	if (mWindow == nullptr)
	{
		if (width == 0)
			width = 1024;
		if (height == 0)
			height = 768;

		if (x == 0)
			x = SDL_WINDOWPOS_UNDEFINED;
		if (y == 0)
			y = SDL_WINDOWPOS_UNDEFINED;

		LL_INFOS() << "Creating window " << width << "x" << height << "x" << bits << LL_ENDL;
		mWindow = SDL_CreateWindow(mWindowTitle.c_str(), x, y, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL);
		if (mWindow == nullptr)
		{
			LL_WARNS() << "Window creation failure. SDL: " << SDL_GetError() << LL_ENDL;
			setupFailure("Window creation error", "Error", OSMB_OK);
			return FALSE;
		}
	}

	if (getFullscreen())
	{ 
		Uint32 window_mode_flag = 0;
		if (window_mode == E_WINDOW_WINDOWED_FULLSCREEN)
		{
			window_mode_flag = SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
		else if (window_mode == E_WINDOW_FULLSCREEN_EXCLUSIVE)
		{
			window_mode_flag = SDL_WINDOW_FULLSCREEN;
		}
		int ret = SDL_SetWindowFullscreen(mWindow, window_mode_flag);
		if (ret == 0)
		{
			SDL_SetWindowResizable(mWindow, SDL_FALSE);
			SDL_SetWindowPosition(mWindow, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
			if (window_mode == E_WINDOW_WINDOWED_FULLSCREEN)
			{
				mWindowMode = E_WINDOW_WINDOWED_FULLSCREEN;
			}
			else if (window_mode == E_WINDOW_FULLSCREEN_EXCLUSIVE)
			{
				LL_INFOS() << "Setting up fullscreen " << width << "x" << height << LL_ENDL;

				// If the requested width or height is 0, find the best default for the monitor.
				if ((width == 0) || (height == 0))
				{
					// Scan through the list of modes, looking for one which has:
					//		height between 700 and 800
					//		aspect ratio closest to the user's original mode
					S32 resolutionCount = 0;
					LLWindowResolution *resolutionList = getSupportedResolutions(resolutionCount);

					if (resolutionList != nullptr)
					{
						F32 closestAspect = 0;
						U32 closestHeight = 0;
						U32 closestWidth = 0;
						int i;

						LL_INFOS() << "Searching for a display mode, original aspect is " << mOriginalAspectRatio << LL_ENDL;

						for (i = 0; i < resolutionCount; i++)
						{
							F32 aspect = (F32) resolutionList[i].mWidth / (F32) resolutionList[i].mHeight;

							LL_INFOS() << "Display mode width " << resolutionList[i].mWidth << " height " << resolutionList[i].mHeight << " aspect " << aspect << LL_ENDL;

							if ((resolutionList[i].mHeight >= 700) && (resolutionList[i].mHeight <= 800) &&
								(fabs(aspect - mOriginalAspectRatio) < fabs(closestAspect - mOriginalAspectRatio)))
							{
								LL_INFOS() << " (new closest mode) " << LL_ENDL;

								// This is the closest mode we've seen yet.
								closestWidth = resolutionList[i].mWidth;
								closestHeight = resolutionList[i].mHeight;
								closestAspect = aspect;
							}
						}

						width = closestWidth;
						height = closestHeight;
					}
				}

				if ((width == 0) || (height == 0))
				{
					// Mode search failed for some reason.  Use the old-school default.
					width = 1024;
					height = 768;
				}
				SDL_DisplayMode target_mode;
				SDL_zero(target_mode);
				target_mode.w = width;
				target_mode.h = height;

				SDL_DisplayMode closest_mode;
				SDL_zero(closest_mode);
				SDL_GetClosestDisplayMode(SDL_GetWindowDisplayIndex(mWindow), &target_mode, &closest_mode);
				if (SDL_SetWindowDisplayMode(mWindow, &closest_mode) == 0)
				{
					SDL_DisplayMode mode;
					SDL_GetWindowDisplayMode(mWindow, &mode);
					mWindowMode = E_WINDOW_FULLSCREEN_EXCLUSIVE;
					mFullscreenWidth = mode.w;
					mFullscreenHeight = mode.h;
					mFullscreenBits = SDL_BITSPERPIXEL(mode.format);
					mFullscreenRefresh = mode.refresh_rate;

					mOverrideAspectRatio = (float) mode.w / (float) mode.h;

					LL_INFOS() << "Running at " << mFullscreenWidth
						<< "x" << mFullscreenHeight
						<< "x" << mFullscreenBits
						<< " @ " << mFullscreenRefresh
						<< LL_ENDL;
				}
				else
				{
					LL_WARNS() << "Failed to set display mode for fullscreen. SDL: " << SDL_GetError() << LL_ENDL;
					// No fullscreen support
					mWindowMode = E_WINDOW_WINDOWED;
					mFullscreenWidth = -1;
					mFullscreenHeight = -1;
					mFullscreenBits = -1;
					mFullscreenRefresh = -1;

					std::string error = llformat("Unable to run fullscreen at %d x %d.\nRunning in window.", width, height);
					OSMessageBox(error, "Error", OSMB_OK);
					SDL_SetWindowFullscreen(mWindow, 0);
					SDL_SetWindowResizable(mWindow, SDL_TRUE);
				}
			}
		}
		else
		{
			LL_WARNS() << "Failed to set fullscreen. SDL: " << SDL_GetError() << LL_ENDL;
		}
	}

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, (bits <= 16) ? 16 : 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

	SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, (bits <= 16) ? 1 : 8);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

	if (mFSAASamples > 0)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, mFSAASamples);
	}

	if (LLRender::sGLCoreProfile)
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	}
#if !LL_DARWIN
	else
	{
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	}
#endif

	U32 context_flags = 0;
	if (gDebugGL)
	{
		context_flags |= SDL_GL_CONTEXT_DEBUG_FLAG;
	}
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, context_flags);

	U32 major_gl_version = 4;
	U32 minor_gl_version = 5;

	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major_gl_version);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor_gl_version);

	bool done = false;
	while (!done)
	{
		mGLContext = SDL_GL_CreateContext(mWindow);

		if (!mGLContext)
		{
			if (minor_gl_version > 0)
			{ //decrement minor version
				minor_gl_version--;
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor_gl_version);
			}
			else if (major_gl_version > 3)
			{ //decrement major version and start minor version over at 3
				major_gl_version--;
				minor_gl_version = 3;
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major_gl_version);
				SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor_gl_version);
			}
			else
			{ //we reached 3.0 and still failed, bail out
				done = true;
			}
		}
		else
		{
			LL_INFOS() << "Created OpenGL " << llformat("%d.%d", major_gl_version, minor_gl_version) <<
				(LLRender::sGLCoreProfile ? " core" : " compatibility") << " context." << LL_ENDL;
			done = true;

			if (LLRender::sGLCoreProfile)
			{
				LLGLSLShader::sNoFixedFunction = true;
			}
		}
	}

	if (!mGLContext)
	{
		LL_WARNS() << "OpenGL context creation failure. SDL: " << SDL_GetError() << LL_ENDL;
		setupFailure("Context creation error", "Error", OSMB_OK);
		SDL_DestroyWindow(mWindow);
		mWindow = nullptr;
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return FALSE;
	}
	if (SDL_GL_MakeCurrent(mWindow, mGLContext) != 0)
	{
		LL_WARNS() << "Failed to make context current. SDL: " << SDL_GetError() << LL_ENDL;
		setupFailure("Context current failure", "Error", OSMB_OK);
		SDL_GL_DeleteContext(mGLContext);
		mGLContext = nullptr;
		SDL_DestroyWindow(mWindow);
		mWindow = nullptr;
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		return FALSE;
	}

	switch (vsync_setting)
	{
	case E_VSYNC_DISABLED:
		SDL_GL_SetSwapInterval(0);
		break;
	case E_VSYNC_NORMAL:
		if (SDL_GL_SetSwapInterval(1) == -1)
		{
			SDL_GL_SetSwapInterval(0);
		}
		else
		{
			LL_INFOS() << "Enabled vsync" << LL_ENDL;
		}
		break;
	case E_VSYNC_ADAPTIVE:
		if (SDL_GL_SetSwapInterval(-1) == -1)
		{
			if (SDL_GL_SetSwapInterval(1) == -1)
			{
				SDL_GL_SetSwapInterval(0);
			}
			else
			{
				LL_INFOS() << "Enabled vsync due to adaptive vsync failure." << LL_ENDL;
			}
		}
		else
		{
			LL_INFOS() << "Enabled adaptive vsync" << LL_ENDL;
		}
		break;
	default:
		break;
	}

#if LL_WINDOWS
	setIconWindows();
#endif

	int r, g, b, a, d, s;
	SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
	SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
	SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
	SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
	SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &d);
	SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &s);

	LL_INFOS() << "GL buffer:" << LL_ENDL;
	LL_INFOS() << "  Red Bits " << r << LL_ENDL;
	LL_INFOS() << "  Green Bits " << g << LL_ENDL;
	LL_INFOS() << "  Blue Bits " << b << LL_ENDL;
	LL_INFOS() << "  Alpha Bits " << a << LL_ENDL;
	LL_INFOS() << "  Depth Bits " << d << LL_ENDL;
	LL_INFOS() << "  Stencil Bits " << s << LL_ENDL;

	//make sure multisampling is disabled by default
	glDisable(GL_MULTISAMPLE_ARB);

	// Don't need to get the current gamma, since there's a call that restores it to the system defaults.
	return TRUE;
}

// changing fullscreen resolution, or switching between windowed and fullscreen mode.
BOOL LLWindowSDL2::switchContext(U32 window_mode, const LLCoordScreen &size, U32 vsync_setting, const LLCoordScreen * const posp)
{
	const BOOL needsRebuild = TRUE;  // Just nuke the context and start over.
	BOOL result = true;

	LL_INFOS() << "switchContext, fullscreen=" << window_mode << LL_ENDL;
	stop_glerror();
	if (needsRebuild)
	{
		destroyContext();
		result = createContext(0, 0, size.mX, size.mY, 32, window_mode, vsync_setting);
		if (result)
		{
			gGLManager.initGL();

			//start with arrow cursor
			initCursors();
			setCursor(UI_CURSOR_ARROW);
		}
	}

	stop_glerror();

	return result;
}

void LLWindowSDL2::destroyContext()
{
	LL_INFOS() << "destroyContext begins" << LL_ENDL;

	// Clean up remaining GL state before blowing away window
	LL_INFOS() << "shutdownGL begins" << LL_ENDL;
	gGLManager.shutdownGL();
	LL_INFOS() << "SDL_QuitSS/VID begins" << LL_ENDL;
	SDL_GL_DeleteContext(mGLContext);
	mGLContext = nullptr;

	SDL_DestroyWindow(mWindow);
	mWindow = nullptr;

#if LL_WINDOWS
	::DestroyIcon(mWinWindowIcon);
#endif

	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

LLWindowSDL2::~LLWindowSDL2()
{
	quitCursors();
	destroyContext();

	if (mSupportedResolutions != nullptr)
	{
		delete [] mSupportedResolutions;
	}
	SDL_Quit();

	gWindowImplementation = nullptr;
}

void LLWindowSDL2::show()
{
	if (mWindow)
	{
		SDL_ShowWindow(mWindow);
	}
}

void LLWindowSDL2::hide()
{
	if (mWindow)
	{
		SDL_HideWindow(mWindow);
	}
}

//virtual
void LLWindowSDL2::minimize()
{
	if (mWindow)
	{
		SDL_MinimizeWindow(mWindow);
	}
}

//virtual
void LLWindowSDL2::restore()
{
	if (mWindow)
	{
		SDL_RestoreWindow(mWindow);
	}
}

// close() destroys all OS-specific code associated with a window.
// Usually called from LLWindowManager::destroyWindow()
void LLWindowSDL2::close()
{
	// Is window is already closed?
	//	if (!mWindow)
	//	{
	//		return;
	//	}

	// Make sure cursor is visible and we haven't mangled the clipping state.
	setMouseClipping(FALSE);
	showCursor();

	destroyContext();
}

BOOL LLWindowSDL2::isValid()
{
	return (mWindow != nullptr);
}

BOOL LLWindowSDL2::getVisible()
{
	BOOL result = FALSE;

	if (mWindow)
	{
		Uint32 flags = SDL_GetWindowFlags(mWindow);
		if (flags & SDL_WINDOW_SHOWN)
		{
			result = TRUE;
		}
		else if (flags & SDL_WINDOW_HIDDEN)
		{
			result = FALSE;
		}
	}
	return(result);
}

BOOL LLWindowSDL2::getMinimized()
{
	BOOL result = FALSE;
	if (mWindow)
	{
		Uint32 flags = SDL_GetWindowFlags(mWindow);
		if (flags & SDL_WINDOW_MINIMIZED)
		{
			result = TRUE;
		}
	}
	return result;
}

BOOL LLWindowSDL2::getMaximized()
{
	BOOL result = FALSE;
	if (mWindow)
	{
		Uint32 flags = SDL_GetWindowFlags(mWindow);
		if (flags & SDL_WINDOW_MAXIMIZED)
		{
			result = TRUE;
		}
	}

	return result;
}

BOOL LLWindowSDL2::maximize()
{
	BOOL result = FALSE;
	if (mWindow)
	{
		SDL_MaximizeWindow(mWindow);
		result = TRUE;
	}
	return result;
}

BOOL LLWindowSDL2::getFullscreen()
{
	return mWindowMode == E_WINDOW_FULLSCREEN_EXCLUSIVE || mWindowMode == E_WINDOW_WINDOWED_FULLSCREEN;
}

BOOL LLWindowSDL2::getPosition(LLCoordScreen *position)
{
	BOOL result = FALSE;
	if (mWindow)
	{
		SDL_GetWindowPosition(mWindow, &position->mX, &position->mY);
	}
	return result;
}

BOOL LLWindowSDL2::getSize(LLCoordScreen *size)
{
	if (mWindow)
	{
		SDL_GetWindowSize(mWindow, &size->mX, &size->mY);
		return TRUE;
	}

	return FALSE;
}

BOOL LLWindowSDL2::getSize(LLCoordWindow *size)
{
	if (mWindow)
	{
		SDL_GetWindowSize(mWindow, &size->mX, &size->mY);
		return (TRUE);
	}

	return (FALSE);
}

BOOL LLWindowSDL2::setPosition(LLCoordScreen position)
{
	if (mWindow)
	{
		SDL_SetWindowPosition(mWindow, position.mX, position.mY);
		return TRUE;
	}

	return FALSE;
}

BOOL LLWindowSDL2::setSizeImpl(LLCoordScreen size)
{
	if (mWindow)
	{
		S32 width = llmax(size.mX, (S32) mMinWindowWidth);
		S32 height = llmax(size.mY, (S32) mMinWindowHeight);

		if (mWindowMode == E_WINDOW_FULLSCREEN_EXCLUSIVE)
		{
			SDL_DisplayMode target_mode;
			SDL_zero(target_mode);
			target_mode.w = width;
			target_mode.h = height;

			SDL_DisplayMode closest_mode;
			SDL_zero(closest_mode);
			SDL_GetClosestDisplayMode(SDL_GetWindowDisplayIndex(mWindow), &target_mode, &closest_mode);
			if (SDL_SetWindowDisplayMode(mWindow, &closest_mode) == 0)
			{
				SDL_DisplayMode mode;
				SDL_GetWindowDisplayMode(mWindow, &mode);
				mFullscreenWidth = mode.w;
				mFullscreenHeight = mode.h;
				mFullscreenBits = SDL_BITSPERPIXEL(mode.format);
				mFullscreenRefresh = mode.refresh_rate;
				mOverrideAspectRatio = (float) mode.w / (float) mode.h;

				LL_INFOS() << "Running at " << mFullscreenWidth
					<< "x" << mFullscreenHeight
					<< "x" << mFullscreenBits
					<< " @ " << mFullscreenRefresh
					<< LL_ENDL;
			}
		}
		else
		{
			SDL_SetWindowSize(mWindow, width, height);
		}
		return TRUE;
	}

	return FALSE;
}

BOOL LLWindowSDL2::setSizeImpl(LLCoordWindow size)
{
	if (mWindow)
	{
		S32 width = llmax(size.mX, (S32) mMinWindowWidth);
		S32 height = llmax(size.mY, (S32) mMinWindowHeight);

		if (mWindowMode == E_WINDOW_FULLSCREEN_EXCLUSIVE)
		{
			SDL_DisplayMode target_mode;
			SDL_zero(target_mode);
			target_mode.w = width;
			target_mode.h = height;

			SDL_DisplayMode closest_mode;
			SDL_zero(closest_mode);
			SDL_GetClosestDisplayMode(SDL_GetWindowDisplayIndex(mWindow), &target_mode, &closest_mode);
			if (SDL_SetWindowDisplayMode(mWindow, &closest_mode) == 0)
			{
				SDL_DisplayMode mode;
				SDL_GetWindowDisplayMode(mWindow, &mode);
				mFullscreenWidth = mode.w;
				mFullscreenHeight = mode.h;
				mFullscreenBits = SDL_BITSPERPIXEL(mode.format);
				mFullscreenRefresh = mode.refresh_rate;
				mOverrideAspectRatio = (float) mode.w / (float) mode.h;

				LL_INFOS() << "Running at " << mFullscreenWidth
					<< "x" << mFullscreenHeight
					<< "x" << mFullscreenBits
					<< " @ " << mFullscreenRefresh
					<< LL_ENDL;
			}
		}
		else
		{
			SDL_SetWindowSize(mWindow, width, height);
		}
		return TRUE;
	}

	return FALSE;
}


void LLWindowSDL2::swapBuffers()
{
	if (mWindow)
	{
		SDL_GL_SwapWindow(mWindow);
	}
}

U32 LLWindowSDL2::getFSAASamples()
{
	return mFSAASamples;
}

void LLWindowSDL2::setFSAASamples(const U32 samples)
{
	mFSAASamples = samples;
}

F32 LLWindowSDL2::getGamma()
{
	return 1.f / mGamma;
}

BOOL LLWindowSDL2::restoreGamma()
{
	if (mWindow)
	{
		Uint16 ramp[256];
		SDL_CalculateGammaRamp(1.f, ramp);
		SDL_SetWindowGammaRamp(mWindow, ramp, ramp, ramp);
	}
	return true;
}

BOOL LLWindowSDL2::setGamma(const F32 gamma)
{
	if (mWindow)
	{
		Uint16 ramp[256];

		mGamma = gamma;
		if (mGamma == 0) mGamma = 0.1f;
		mGamma = 1.f / mGamma;

		SDL_CalculateGammaRamp(mGamma, ramp);
		SDL_SetWindowGammaRamp(mWindow, ramp, ramp, ramp);
	}
	return true;
}

BOOL LLWindowSDL2::isCursorHidden()
{
	return mCursorHidden;
}

// Constrains the mouse to the window.
void LLWindowSDL2::setMouseClipping(BOOL b)
{
	SDL_SetWindowGrab(mWindow, b ? SDL_TRUE : SDL_FALSE);
}

// virtual
void LLWindowSDL2::setMinSize(U32 min_width, U32 min_height, bool enforce_immediately)
{
	LLWindow::setMinSize(min_width, min_height, enforce_immediately);

	if (mWindow)
	{
		SDL_SetWindowMinimumSize(mWindow, mMinWindowWidth, mMinWindowHeight);
	}
}

BOOL LLWindowSDL2::setCursorPosition(LLCoordWindow position)
{
	BOOL result = TRUE;
	LLCoordScreen screen_pos;

	if (!convertCoords(position, &screen_pos))
	{
		return FALSE;
	}

	//LL_INFOS() << "setCursorPosition(" << screen_pos.mX << ", " << screen_pos.mY << ")" << LL_ENDL;

	// do the actual forced cursor move.
	SDL_WarpMouseInWindow(mWindow, screen_pos.mX, screen_pos.mY);

	//LL_INFOS() << llformat("llcw %d,%d -> scr %d,%d", position.mX, position.mY, screen_pos.mX, screen_pos.mY) << LL_ENDL;

	return result;
}

BOOL LLWindowSDL2::getCursorPosition(LLCoordWindow *position)
{
	//Point cursor_point;
	LLCoordScreen screen_pos;

	SDL_GetMouseState(&screen_pos.mX, &screen_pos.mY);
	return convertCoords(screen_pos, position);
}

F32 LLWindowSDL2::getNativeAspectRatio()
{
#if 0
	// RN: this hack presumes that the largest supported resolution is monitor-limited
	// and that pixels in that mode are square, therefore defining the native aspect ratio
	// of the monitor...this seems to work to a close approximation for most CRTs/LCDs
	S32 num_resolutions;
	LLWindowResolution* resolutions = getSupportedResolutions(num_resolutions);


	return ((F32) resolutions[num_resolutions - 1].mWidth / (F32) resolutions[num_resolutions - 1].mHeight);
	//rn: AC
#endif

	// MBW -- there are a couple of bad assumptions here.  One is that the display list won't include
	//		ridiculous resolutions nobody would ever use.  The other is that the list is in order.

	// New assumptions:
	// - pixels are square (the only reasonable choice, really)
	// - The user runs their display at a native resolution, so the resolution of the display
	//    when the app is launched has an aspect ratio that matches the monitor.

	//RN: actually, the assumption that there are no ridiculous resolutions (above the display's native capabilities) has 
	// been born out in my experience.  
	// Pixels are often not square (just ask the people who run their LCDs at 1024x768 or 800x600 when running fullscreen, like me)
	// The ordering of display list is a blind assumption though, so we should check for max values
	// Things might be different on the Mac though, so I'll defer to MBW

	// The constructor for this class grabs the aspect ratio of the monitor before doing any resolution
	// switching, and stashes it in mOriginalAspectRatio.  Here, we just return it.

	if (mOverrideAspectRatio > 0.f)
	{
		return mOverrideAspectRatio;
	}

	return mOriginalAspectRatio;
}

F32 LLWindowSDL2::getPixelAspectRatio()
{
	F32 pixel_aspect = 1.f;
	if (getFullscreen())
	{
		LLCoordScreen screen_size;
		if (getSize(&screen_size))
		{
			pixel_aspect = getNativeAspectRatio() * (F32) screen_size.mY / (F32) screen_size.mX;
		}
	}

	return pixel_aspect;
}

// This is to support 'temporarily windowed' mode so that
// dialogs are still usable in fullscreen.
void LLWindowSDL2::beforeDialog()
{
	if (getFullscreen())
	{
		SDL_MinimizeWindow(mWindow);
	}
}

void LLWindowSDL2::afterDialog()
{
	if (getFullscreen())
	{
		SDL_RestoreWindow(mWindow);
		SDL_RaiseWindow(mWindow);
	}
	SDL_GL_MakeCurrent(mWindow, mGLContext);
}

void LLWindowSDL2::flashIcon(F32 seconds)
{
#if LL_WINDOWS

	const F32	ICON_FLASH_TIME = 0.5f;
	FLASHWINFO flash_info;
	flash_info.cbSize = sizeof(FLASHWINFO);
	flash_info.hwnd = (HWND) getPlatformWindow();
	flash_info.dwFlags = FLASHW_TRAY;
	flash_info.uCount = UINT(seconds / ICON_FLASH_TIME);
	flash_info.dwTimeout = DWORD(1000.f * ICON_FLASH_TIME); // milliseconds
	FlashWindowEx(&flash_info);
#endif
}

BOOL LLWindowSDL2::isClipboardTextAvailable()
{
	return SDL_HasClipboardText();
}

BOOL LLWindowSDL2::pasteTextFromClipboard(LLWString &text)
{
	if (isClipboardTextAvailable())
	{
		char* data = SDL_GetClipboardText();
		if (data)
		{
			text = LLWString(utf8str_to_wstring(data));
			SDL_free(data);
			return TRUE;
		}
	}
	return FALSE; // failure
}

BOOL LLWindowSDL2::copyTextToClipboard(const LLWString &text)
{
	const std::string utf8 = wstring_to_utf8str(text);
	return SDL_SetClipboardText(utf8.c_str()) == 0; // success == 0
}

LLWindow::LLWindowResolution* LLWindowSDL2::getSupportedResolutions(S32 &num_resolutions)
{
	if (!mSupportedResolutions)
	{
		mSupportedResolutions = new LLWindowResolution[MAX_NUM_RESOLUTIONS];
	}
	mNumSupportedResolutions = 0;
	int mode_count = SDL_GetNumDisplayModes(0);
	for (int i = mode_count - 1; i >= 0; i--)
	{
		SDL_DisplayMode mode;
		if (SDL_GetDisplayMode(0, i, &mode) == 0)
		{
			int w = mode.w;
			int h = mode.h;
			if ((w >= 800) && (h >= 600))
			{
				// make sure we don't add the same resolution multiple times!
				if ((mNumSupportedResolutions == 0) ||
					((mSupportedResolutions[mNumSupportedResolutions - 1].mWidth != w) &&
					(mSupportedResolutions[mNumSupportedResolutions - 1].mHeight != h))
					)
				{
					mSupportedResolutions[mNumSupportedResolutions].mWidth = w;
					mSupportedResolutions[mNumSupportedResolutions].mHeight = h;
					mNumSupportedResolutions++;
					if (mNumSupportedResolutions >= MAX_NUM_RESOLUTIONS)
						break;
				}
			}
		}
	}
	num_resolutions = mNumSupportedResolutions;
	return mSupportedResolutions;
}

BOOL LLWindowSDL2::convertCoords(LLCoordGL from, LLCoordWindow *to)
{
	if (!to)
		return FALSE;

	if (!mWindow)
		return FALSE;
	S32 height;
	SDL_GetWindowSize(mWindow, nullptr, &height);

	to->mX = from.mX;
	to->mY = height - from.mY - 1;

	return TRUE;
}

BOOL LLWindowSDL2::convertCoords(LLCoordWindow from, LLCoordGL* to)
{
	if (!to)
		return FALSE;

	if (!mWindow)
		return FALSE;
	S32 height;
	SDL_GetWindowSize(mWindow, nullptr, &height);

	to->mX = from.mX;
	to->mY = height - from.mY - 1;

	return TRUE;
}

BOOL LLWindowSDL2::convertCoords(LLCoordScreen from, LLCoordWindow* to)
{
	if (!to)
		return FALSE;

	// In the fullscreen case, window and screen coordinates are the same.
	to->mX = from.mX;
	to->mY = from.mY;
	return (TRUE);
}

BOOL LLWindowSDL2::convertCoords(LLCoordWindow from, LLCoordScreen *to)
{
	if (!to)
		return FALSE;

	// In the fullscreen case, window and screen coordinates are the same.
	to->mX = from.mX;
	to->mY = from.mY;
	return (TRUE);
}

BOOL LLWindowSDL2::convertCoords(LLCoordScreen from, LLCoordGL *to)
{
	LLCoordWindow window_coord;

	return(convertCoords(from, &window_coord) && convertCoords(window_coord, to));
}

BOOL LLWindowSDL2::convertCoords(LLCoordGL from, LLCoordScreen *to)
{
	LLCoordWindow window_coord;

	return(convertCoords(from, &window_coord) && convertCoords(window_coord, to));
}

void LLWindowSDL2::setupFailure(const std::string& text, const std::string& caption, U32 type)
{
	destroyContext();

	OSMessageBox(text, caption, type);
}

BOOL LLWindowSDL2::SDLReallyCaptureInput(BOOL capture)
{
	SDL_SetWindowGrab(mWindow, (SDL_bool) capture);

	return SDL_GetWindowGrab(mWindow);
}

U32 LLWindowSDL2::SDLCheckGrabbyKeys(SDL_Keycode keysym, BOOL gain)
{
	/* part of the fix for SL-13243: Some popular window managers like
	   to totally eat alt-drag for the purposes of moving windows.  We
	   spoil their day by acquiring the exclusive X11 mouse lock for as
	   long as ALT is held down, so the window manager can't easily
	   see what's happening.  Tested successfully with Metacity.
	   And... do the same with CTRL, for other darn WMs.  We don't
	   care about other metakeys as SL doesn't use them with dragging
	   (for now). */

	   /* We maintain a bitmap of critical keys which are up and down
		  instead of simply key-counting, because SDL sometimes reports
		  misbalanced keyup/keydown event pairs to us for whatever reason. */

	U32 mask = 0;
	switch (keysym)
	{
	case SDLK_LALT:
		mask = 1U << 0; break;
	case SDLK_RALT:
		mask = 1U << 1; break;
	case SDLK_LCTRL:
		mask = 1U << 2; break;
	case SDLK_RCTRL:
		mask = 1U << 3; break;
	default:
		break;
	}

	if (gain)
		mGrabbyKeyFlags |= mask;
	else
		mGrabbyKeyFlags &= ~mask;

	//LL_INFOS() << "mGrabbyKeyFlags=" << mGrabbyKeyFlags << LL_ENDL;

	/* 0 means we don't need to mousegrab, otherwise grab. */
	return mGrabbyKeyFlags;
}

// virtual
void LLWindowSDL2::processMiscNativeEvents()
{
}

void LLWindowSDL2::gatherInput()
{
	// Handle all outstanding SDL events
	SDL_Event event;
	while (SDL_PollEvent(&event))
	{
		switch (event.type)
		{
		case SDL_MOUSEMOTION:
		{
			LLCoordWindow winCoord(event.motion.x, event.motion.y);
			MASK mask = gKeyboard->currentMask(TRUE);
			mCallbacks->handleMouseMove(this, winCoord.convert(), mask);
			break;
		}
		case SDL_MOUSEWHEEL:
		{
			mCallbacks->handleScrollWheel(this, -event.wheel.y);
			break;
		}
		case SDL_TEXTINPUT:
		{
			int i = 0;
			char* s = event.text.text;
			while (s[i])
			{
				handleUnicodeUTF16(s[i++],
					gKeyboard->currentMask(FALSE));
			}
			break;
		}
		case SDL_KEYDOWN:
		{
			mKeyScanCode = event.key.keysym.scancode;
			mKeyVirtualKey = event.key.keysym.sym;
			mKeyModifiers = event.key.keysym.mod;

			if (SDLCheckGrabbyKeys(event.key.keysym.sym, TRUE) != 0)
				SDLReallyCaptureInput(TRUE); // part of the fix for SL-13243

			gKeyboard->handleKeyDown(event.key.keysym.sym, event.key.keysym.mod);
			break;
		}
		case SDL_KEYUP:
			mKeyScanCode = event.key.keysym.scancode;
			mKeyVirtualKey = event.key.keysym.sym;
			mKeyModifiers = event.key.keysym.mod;

			if (SDLCheckGrabbyKeys(event.key.keysym.sym, FALSE) == 0)
				SDLReallyCaptureInput(FALSE); // part of the fix for SL-13243

			gKeyboard->handleKeyUp(event.key.keysym.sym, event.key.keysym.mod);
			break;

		case SDL_MOUSEBUTTONDOWN:
		{
			LLCoordWindow winCoord(event.button.x, event.button.y);
			LLCoordGL openGlCoord;
			convertCoords(winCoord, &openGlCoord);
			MASK mask = gKeyboard->currentMask(TRUE);

			if (event.button.button == SDL_BUTTON_LEFT)  // left
			{
				if (event.button.clicks >= 2)
					mCallbacks->handleDoubleClick(this, openGlCoord, mask);
				else
					mCallbacks->handleMouseDown(this, openGlCoord, mask);
			}

			else if (event.button.button == SDL_BUTTON_RIGHT)  // right
			{
				mCallbacks->handleRightMouseDown(this, openGlCoord, mask);
			}

			else if (event.button.button == SDL_BUTTON_MIDDLE)  // middle
			{
				mCallbacks->handleMiddleMouseDown(this, openGlCoord, mask);
			}

			break;
		}

		case SDL_MOUSEBUTTONUP:
		{
			LLCoordWindow winCoord(event.button.x, event.button.y);
			LLCoordGL openGlCoord;
			convertCoords(winCoord, &openGlCoord);
			MASK mask = gKeyboard->currentMask(TRUE);

			if (event.button.button == SDL_BUTTON_LEFT)  // left
				mCallbacks->handleMouseUp(this, openGlCoord, mask);
			else if (event.button.button == SDL_BUTTON_RIGHT)  // right
				mCallbacks->handleRightMouseUp(this, openGlCoord, mask);
			else if (event.button.button == SDL_BUTTON_MIDDLE)  // middle
				mCallbacks->handleMiddleMouseUp(this, openGlCoord, mask);
			// don't handle mousewheel here...

			break;
		}
		case SDL_WINDOWEVENT:
		{
			switch (event.window.event)
			{
				// Exposed
			case SDL_WINDOWEVENT_EXPOSED:  // VIDEOEXPOSE doesn't specify the damage, but hey, it's OpenGL...repaint the whole thing!
			{
				mCallbacks->handlePaint(this, 0, 0, event.window.data1, event.window.data2);
				break;
			}
			case SDL_WINDOWEVENT_SIZE_CHANGED:
			case SDL_WINDOWEVENT_RESIZED:  // *FIX: handle this?
			{
				LL_INFOS() << "Handling a resize event: " << event.window.data1 <<
					"x" << event.window.data2 << LL_ENDL;

				S32 width = llmax(event.window.data1, (S32) mMinWindowWidth);
				S32 height = llmax(event.window.data2, (S32) mMinWindowHeight);

				mCallbacks->handleResize(this, width, height);
				break;
			}
			// Restore
			case SDL_WINDOWEVENT_SHOWN:
				if (getFullscreen())
				{
					bringToFront();
				}
				mCallbacks->handleActivate(this, true);
				break;
				// Minimize
			case SDL_WINDOWEVENT_HIDDEN:
				mCallbacks->handleActivate(this, false);
				break;
				// Keyboard focus gained
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				mCallbacks->handleFocus(this);
				break;
				// Keyboard focus lost
			case SDL_WINDOWEVENT_FOCUS_LOST:
				mCallbacks->handleFocusLost(this);
				break;
				// Mouse Leave
			case SDL_WINDOWEVENT_LEAVE:
				mCallbacks->handleMouseLeave(this);
				break;
				// Close
			case SDL_WINDOWEVENT_CLOSE:
			{
				if (mCallbacks->handleCloseRequest(this))
				{
					// Get the app to initiate cleanup.
					mCallbacks->handleQuit(this);
					// The app is responsible for calling destroyWindow when done with GL
				}
				break;
			}
			default:
				//LL_INFOS() << "Unhandled SDL window event type " << event.type << LL_ENDL;
				break;
			}
			break;
		}
		default:
			//LL_INFOS() << "Unhandled SDL event type " << event.type << LL_ENDL;
			break;
		}
	}
	updateCursor();
}

static SDL_Cursor *makeSDLCursorFromBMP(const char *filename, int hotx, int hoty)
{
	SDL_Cursor *sdlcursor = nullptr;
	SDL_Surface *bmpsurface;

	// Load cursor pixel data from BMP file
	bmpsurface = Load_BMP_Resource(filename);
	if (bmpsurface && bmpsurface->w % 8 == 0)
	{
		
		sdlcursor = SDL_CreateColorCursor(bmpsurface, hotx, hoty);
		if (!sdlcursor)
		{
			LL_WARNS() << "Failed to create cursor: " << filename << " SDL Error: " << SDL_GetError() << LL_ENDL;
		}
		SDL_FreeSurface(bmpsurface);
	}
	else 
	{
		LL_WARNS() << "Failed to load bmp: " << filename << " SDL Error: " << SDL_GetError() << LL_ENDL;
	}

	return sdlcursor;
}

void LLWindowSDL2::updateCursor()
{
	if (mCurrentCursor != mNextCursor)
	{
		if (mNextCursor < UI_CURSOR_COUNT)
		{
			SDL_Cursor *sdlcursor = mSDLCursors[mNextCursor];
			// Try to default to the arrow for any cursors that
			// did not load correctly.
			if (!sdlcursor && mSDLCursors[UI_CURSOR_ARROW])
				sdlcursor = mSDLCursors[UI_CURSOR_ARROW];
			if (sdlcursor)
				SDL_SetCursor(sdlcursor);
		}
		else {
			LL_WARNS() << "Tried to set invalid cursor number " << mNextCursor << LL_ENDL;
		}
		mCurrentCursor = mNextCursor;
	}
	SDL_SetCursor(nullptr);
}

void LLWindowSDL2::initCursors()
{
	int i;
	// Blank the cursor pointer array for those we may miss.
	for (i = 0; i < UI_CURSOR_COUNT; ++i)
	{
		mSDLCursors[i] = nullptr;
	}
	// Pre-make an SDL cursor for each of the known cursor types.
	// We hardcode the hotspots - to avoid that we'd have to write
	// a .cur file loader.
	// NOTE: SDL doesn't load RLE-compressed BMP files.
	mSDLCursors[UI_CURSOR_ARROW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
	mSDLCursors[UI_CURSOR_WAIT] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAIT);
	mSDLCursors[UI_CURSOR_HAND] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND);
	mSDLCursors[UI_CURSOR_IBEAM] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM);
	mSDLCursors[UI_CURSOR_CROSS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
	mSDLCursors[UI_CURSOR_SIZENWSE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);
	mSDLCursors[UI_CURSOR_SIZENESW] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);
	mSDLCursors[UI_CURSOR_SIZEWE] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);
	mSDLCursors[UI_CURSOR_SIZENS] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);
	mSDLCursors[UI_CURSOR_NO] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO);
	mSDLCursors[UI_CURSOR_WORKING] = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_WAITARROW);
	mSDLCursors[UI_CURSOR_TOOLGRAB] = makeSDLCursorFromBMP("lltoolgrab.BMP", 2, 13);
	mSDLCursors[UI_CURSOR_TOOLLAND] = makeSDLCursorFromBMP("lltoolland.BMP", 1, 6);
	mSDLCursors[UI_CURSOR_TOOLFOCUS] = makeSDLCursorFromBMP("lltoolfocus.BMP", 8, 5);
	mSDLCursors[UI_CURSOR_TOOLCREATE] = makeSDLCursorFromBMP("lltoolcreate.BMP", 7, 7);
	mSDLCursors[UI_CURSOR_ARROWDRAG] = makeSDLCursorFromBMP("arrowdrag.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_ARROWCOPY] = makeSDLCursorFromBMP("arrowcop.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_ARROWDRAGMULTI] = makeSDLCursorFromBMP("llarrowdragmulti.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_ARROWCOPYMULTI] = makeSDLCursorFromBMP("arrowcopmulti.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_NOLOCKED] = makeSDLCursorFromBMP("llnolocked.BMP", 8, 8);
	mSDLCursors[UI_CURSOR_ARROWLOCKED] = makeSDLCursorFromBMP("llarrowlocked.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_GRABLOCKED] = makeSDLCursorFromBMP("llgrablocked.BMP", 2, 13);
	mSDLCursors[UI_CURSOR_TOOLTRANSLATE] = makeSDLCursorFromBMP("lltooltranslate.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_TOOLROTATE] = makeSDLCursorFromBMP("lltoolrotate.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_TOOLSCALE] = makeSDLCursorFromBMP("lltoolscale.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_TOOLCAMERA] = makeSDLCursorFromBMP("lltoolcamera.BMP", 7, 5);
	mSDLCursors[UI_CURSOR_TOOLPAN] = makeSDLCursorFromBMP("lltoolpan.BMP", 7, 5);
	mSDLCursors[UI_CURSOR_TOOLZOOMIN] = makeSDLCursorFromBMP("lltoolzoomin.BMP", 7, 5);
	mSDLCursors[UI_CURSOR_TOOLPICKOBJECT3] = makeSDLCursorFromBMP("toolpickobject3.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_TOOLPLAY] = makeSDLCursorFromBMP("toolplay.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_TOOLPAUSE] = makeSDLCursorFromBMP("toolpause.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_TOOLMEDIAOPEN] = makeSDLCursorFromBMP("toolmediaopen.BMP", 0, 0);
	mSDLCursors[UI_CURSOR_PIPETTE] = makeSDLCursorFromBMP("lltoolpipette.BMP", 2, 28);
	mSDLCursors[UI_CURSOR_TOOLSIT] = makeSDLCursorFromBMP("toolsit.BMP", 20, 15);
	mSDLCursors[UI_CURSOR_TOOLBUY] = makeSDLCursorFromBMP("toolbuy.BMP", 20, 15);
	mSDLCursors[UI_CURSOR_TOOLOPEN] = makeSDLCursorFromBMP("toolopen.BMP", 20, 15);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING] = makeSDLCursorFromBMP("lltoolpathfinding.BMP", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING_PATH_START] = makeSDLCursorFromBMP("lltoolpathfindingpathstart.BMP", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING_PATH_START_ADD] = makeSDLCursorFromBMP("lltoolpathfindingpathstartadd.BMP", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING_PATH_END] = makeSDLCursorFromBMP("lltoolpathfindingpathend.BMP", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLPATHFINDING_PATH_END_ADD] = makeSDLCursorFromBMP("lltoolpathfindingpathendadd.BMP", 16, 16);
	mSDLCursors[UI_CURSOR_TOOLNO] = makeSDLCursorFromBMP("llno.BMP", 8, 8);
}

void LLWindowSDL2::quitCursors()
{
	int i;
	if (mWindow)
	{
		for (i = 0; i < UI_CURSOR_COUNT; ++i)
		{
			if (mSDLCursors[i])
			{
				SDL_FreeCursor(mSDLCursors[i]);
				mSDLCursors[i] = nullptr;
			}
		}
	}
	else {
		// SDL doesn't refcount cursors, so if the window has
		// already been destroyed then the cursors have gone with it.
		LL_INFOS() << "Skipping quitCursors: mWindow already gone." << LL_ENDL;
		for (i = 0; i < UI_CURSOR_COUNT; ++i)
			mSDLCursors[i] = nullptr;
	}
}

void LLWindowSDL2::captureMouse()
{
	// SDL already enforces the semantics that captureMouse is
	// used for, i.e. that we continue to get mouse events as long
	// as a button is down regardless of whether we left the
	// window, and in a less obnoxious way than SDL_WM_GrabInput	
	// which would confine the cursor to the window too.
	//SDL_CaptureMouse(SDL_TRUE);
	LL_DEBUGS() << "LLWindowSDL2::captureMouse" << LL_ENDL;
}

void LLWindowSDL2::releaseMouse()
{
	// see LWindowSDL::captureMouse()
	//SDL_CaptureMouse(SDL_FALSE);
	LL_DEBUGS() << "LLWindowSDL2::releaseMouse" << LL_ENDL;
}

void LLWindowSDL2::hideCursor()
{
	if (!mCursorHidden)
	{
		// LL_INFOS() << "hideCursor: hiding" << LL_ENDL;
		mCursorHidden = TRUE;
		mHideCursorPermanent = TRUE;
		SDL_ShowCursor(0);
	}
	else
	{
		// LL_INFOS() << "hideCursor: already hidden" << LL_ENDL;
	}
}

void LLWindowSDL2::showCursor()
{
	if (mCursorHidden)
	{
		// LL_INFOS() << "showCursor: showing" << LL_ENDL;
		mCursorHidden = FALSE;
		mHideCursorPermanent = FALSE;
		SDL_ShowCursor(1);
	}
	else
	{
		// LL_INFOS() << "showCursor: already visible" << LL_ENDL;
	}
}

void LLWindowSDL2::showCursorFromMouseMove()
{
	if (!mHideCursorPermanent)
	{
		showCursor();
	}
}

void LLWindowSDL2::hideCursorUntilMouseMove()
{
	if (!mHideCursorPermanent)
	{
		hideCursor();
		mHideCursorPermanent = FALSE;
	}
}



// Implemented per-platform via #if
LLSplashScreenSDL2::LLSplashScreenSDL2()
{
}

LLSplashScreenSDL2::~LLSplashScreenSDL2()
{
}

void LLSplashScreenSDL2::showImpl()
{
#if LL_WINDOWS
	// This appears to work.  ???
	HINSTANCE hinst = GetModuleHandle(NULL);

	mWindow = CreateDialog(hinst,
		TEXT("SPLASHSCREEN"),
		NULL,	// no parent
		(DLGPROC) DefWindowProc);
	ShowWindow(mWindow, SW_SHOW);
#endif
}

void LLSplashScreenSDL2::updateImpl(const std::string& mesg)
{
#if LL_WINDOWS
	if (!mWindow) return;

	int output_str_len = MultiByteToWideChar(CP_UTF8, 0, mesg.c_str(), mesg.length(), NULL, 0);
	if (output_str_len>1024)
		return;

	WCHAR w_mesg[1025];//big enought to keep null terminatos

	MultiByteToWideChar(CP_UTF8, 0, mesg.c_str(), mesg.length(), w_mesg, output_str_len);

	//looks like MultiByteToWideChar didn't add null terminator to converted string, see EXT-4858
	w_mesg[output_str_len] = 0;

	SendDlgItemMessage(mWindow,
		666,		// HACK: text id
		WM_SETTEXT,
		FALSE,
		(LPARAM) w_mesg);
#endif
}

void LLSplashScreenSDL2::hideImpl()
{
#if LL_WINDOWS
	if (mWindow)
	{
		DestroyWindow(mWindow);
		mWindow = nullptr;
	}
	gWindowImplementation->bringToFront();
#endif
}

S32 OSMessageBoxSDL2(const std::string& text, const std::string& caption, U32 type)
{
	S32 rtn = OSBTN_CANCEL;
	U32 messagetype = SDL_MESSAGEBOX_INFORMATION;

	const SDL_MessageBoxButtonData buttons_ok [] = {
		/* .flags, .buttonid, .text */
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, OSBTN_OK, "Ok" },
	};

	const SDL_MessageBoxButtonData buttons_okcancel [] = {
		/* .flags, .buttonid, .text */
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, OSBTN_OK, "Ok" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, OSBTN_CANCEL, "Cancel" },
	};

	const SDL_MessageBoxButtonData buttons_yesno [] = {
		/* .flags, .buttonid, .text */
		{ SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, OSBTN_YES, "Yes" },
		{ SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, OSBTN_NO, "No" },
	};

	const SDL_MessageBoxButtonData* buttons;

	switch (type)
	{
	default:
	case OSMB_OK:
		buttons = buttons_ok;
		break;
	case OSMB_OKCANCEL:
		buttons = buttons_okcancel;
		break;
	case OSMB_YESNO:
		buttons = buttons_yesno;
		break;
	}

	const SDL_MessageBoxData messageboxdata = {
		messagetype, /* .flags */
		gWindowImplementation->getSDLWindow(), /* .window */
		caption.c_str(), /* .title */
		text.c_str(), /* .message */
		SDL_arraysize(buttons), /* .numbuttons */
		buttons, /* .buttons */
		NULL /* .colorScheme */
	};

	int buttonid;
	if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0)
	{
		return 1;
	}
	if (buttonid != -1) {
		rtn = buttonid;
	}

	return rtn;
}

LLSD LLWindowSDL2::getNativeKeyData()
{
	LLSD result = LLSD::emptyMap();

	U32 modifiers = 0; // pretend-native modifiers... oh what a tangled web we weave!

					   // we go through so many levels of device abstraction that I can't really guess
					   // what a plugin under GDK under Qt under SL under SDL under X11 considers
					   // a 'native' modifier mask.  this has been sort of reverse-engineered... they *appear*
					   // to match GDK consts, but that may be co-incidence.
	modifiers |= (mKeyModifiers & KMOD_LSHIFT) ? 0x0001 : 0;
	modifiers |= (mKeyModifiers & KMOD_RSHIFT) ? 0x0001 : 0;// munge these into the same shift
	modifiers |= (mKeyModifiers & KMOD_CAPS) ? 0x0002 : 0;
	modifiers |= (mKeyModifiers & KMOD_LCTRL) ? 0x0004 : 0;
	modifiers |= (mKeyModifiers & KMOD_RCTRL) ? 0x0004 : 0;// munge these into the same ctrl
	modifiers |= (mKeyModifiers & KMOD_LALT) ? 0x0008 : 0;// untested
	modifiers |= (mKeyModifiers & KMOD_RALT) ? 0x0008 : 0;// untested
														  // *todo: test ALTs - I don't have a case for testing these.  Do you?
														  // *todo: NUM? - I don't care enough right now (and it's not a GDK modifier).

	result["scan_code"] = (S32) mKeyScanCode;
	result["virtual_key"] = (S32) mKeyVirtualKey;
	result["modifiers"] = (S32) modifiers;

	return result;
}


BOOL LLWindowSDL2::dialogColorPicker(F32 *r, F32 *g, F32 *b)
{
	BOOL retval = FALSE;
#if LL_WINDOWS
	static CHOOSECOLOR cc;
	static COLORREF crCustColors[16];
	cc.lStructSize = sizeof(CHOOSECOLOR);
	cc.hwndOwner = (HWND) getPlatformWindow();
	cc.hInstance = NULL;
	cc.rgbResult = RGB((*r * 255.f), (*g *255.f), (*b * 255.f));
	//cc.rgbResult = RGB (0x80,0x80,0x80); 
	cc.lpCustColors = crCustColors;
	cc.Flags = CC_RGBINIT | CC_FULLOPEN;
	cc.lCustData = 0;
	cc.lpfnHook = NULL;
	cc.lpTemplateName = NULL;

	// This call is modal, so pause agent
	//send_agent_pause();	// this is in newview and we don't want to set up a dependency
	{
		retval = ChooseColor(&cc);
	}
	//send_agent_resume();	// this is in newview and we don't want to set up a dependency

	*b = ((F32) ((cc.rgbResult >> 16) & 0xff)) / 255.f;

	*g = ((F32) ((cc.rgbResult >> 8) & 0xff)) / 255.f;

	*r = ((F32) (cc.rgbResult & 0xff)) / 255.f;
#endif
	return retval;
}

// Open a URL with the user's default web browser.
// Must begin with protocol identifier.
void LLWindowSDL2::spawnWebBrowser(const std::string& escaped_url, bool async)
{
	bool found = false;
	S32 i;
	for (i = 0; i < gURLProtocolWhitelistCount; i++)
	{
		if (escaped_url.find(gURLProtocolWhitelist[i]) != std::string::npos)
		{
			found = true;
			break;
		}
	}

	if (!found)
	{
		LL_WARNS() << "spawn_web_browser called for url with protocol not on whitelist: " << escaped_url << LL_ENDL;
		return;
	}

#if LL_WINDOWS
	LL_INFOS("Window") << "Opening URL " << escaped_url << LL_ENDL;

	// replaced ShellExecute code with ShellExecuteEx since ShellExecute doesn't work
	// reliablly on Vista.

	// this is madness.. no, this is..
	LLWString url_wstring = utf8str_to_wstring(escaped_url);
	llutf16string url_utf16 = wstring_to_utf16str(url_wstring);

	// let the OS decide what to use to open the URL
	SHELLEXECUTEINFO sei = { sizeof(sei) };
	// NOTE: this assumes that SL will stick around long enough to complete the DDE message exchange
	// necessary for ShellExecuteEx to complete
	if (async)
	{
		sei.fMask = SEE_MASK_ASYNCOK;
	}
	sei.nShow = SW_SHOWNORMAL;
	sei.lpVerb = L"open";
	sei.lpFile = url_utf16.c_str();
	ShellExecuteEx(&sei);
#endif
}


void *LLWindowSDL2::getPlatformWindow()
{
	if (mWindow)
	{
		SDL_SysWMinfo info;
		SDL_VERSION(&info.version);
		if (SDL_GetWindowWMInfo(mWindow, &info))
		{
			switch (info.subsystem)
			{
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
			case SDL_SYSWM_WINDOWS:
				return (void*) info.info.win.window;
#endif
			default:
				break;
			}
		}
	}
	return nullptr;
}

void LLWindowSDL2::bringToFront()
{
	// This is currently used when we are 'launched' to a specific
	// map position externally.
	SDL_RaiseWindow(mWindow);
	LL_INFOS() << "bringToFront" << LL_ENDL;
}

//static
std::vector<std::string> LLWindowSDL2::getDynamicFallbackFontList()
{
	return std::vector<std::string>();
}

void LLWindowSDL2::setWindowTitle(const std::string& title)
{
	mWindowTitle = title;
	SDL_SetWindowTitle(mWindow, title.c_str());
}

#endif // LL_SDL2
