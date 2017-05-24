// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llgl.cpp
 * @brief LLGL implementation
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

// This file sets some global GL parameters, and implements some 
// useful functions for GL operations.
#include "linden_common.h"

#include <boost/tokenizer.hpp>

#include "llsys.h"

#include "llgl.h"
#include "llrender.h"

#include "llerror.h"
#include "llerrorcontrol.h"
#include "llquaternion.h"
#include "llmath.h"
#include "m4math.h"
#include "llstring.h"
#include "llstacktrace.h"

#include "llglheaders.h"
#include "llglslshader.h"

#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#ifdef _DEBUG
//#define GL_STATE_VERIFY
#endif


BOOL gDebugSession = FALSE;
BOOL gDebugGL = FALSE;
BOOL gClothRipple = FALSE;
BOOL gHeadlessClient = FALSE;
BOOL gGLActive = FALSE;
BOOL gGLDebugLoggingEnabled = TRUE;

static const std::string HEADLESS_VENDOR_STRING("Linden Lab");
static const std::string HEADLESS_RENDERER_STRING("Headless");
static const std::string HEADLESS_VERSION_STRING("1.0");

llofstream gFailLog;

#if GL_ARB_debug_output

#ifndef APIENTRY
#define APIENTRY
#endif

void APIENTRY gl_debug_callback(GLenum source,
                                GLenum type,
                                GLuint id,
                                GLenum severity,
                                GLsizei length,
                                const GLchar* message,
                                GLvoid* userParam)
{
	if (gGLDebugLoggingEnabled)
	{
	if (severity == GL_DEBUG_SEVERITY_HIGH_ARB)
	{
		LL_WARNS() << "----- GL ERROR --------" << LL_ENDL;
	}
	else
	{
		LL_WARNS() << "----- GL WARNING -------" << LL_ENDL;
	}
	LL_WARNS() << "Source: " << std::hex << source << std::dec << LL_ENDL;
	LL_WARNS() << "Type: " << std::hex << type << std::dec << LL_ENDL;
	LL_WARNS() << "ID: " << std::hex << id << std::dec << LL_ENDL;
	LL_WARNS() << "Severity: " << std::hex << severity << std::dec << LL_ENDL;
	LL_WARNS() << "Message: " << message << LL_ENDL;
	LL_WARNS() << "-----------------------" << LL_ENDL;
	if (severity == GL_DEBUG_SEVERITY_HIGH_ARB)
	{
		LL_ERRS() << "Halting on GL Error" << LL_ENDL;
	}
}
}
#endif

void parse_glsl_version(S32& major, S32& minor);
std::string parse_gl_ext_to_str(F32 glversion);

void ll_init_fail_log(const std::string& filename)
{
	gFailLog.open(filename.c_str());
}


void ll_fail(const std::string& msg)
{
	
	if (gDebugSession)
	{
		std::vector<std::string> lines;

		gFailLog << LLError::utcTime() << " " << msg << std::endl;

		gFailLog << "Stack Trace:" << std::endl;

		ll_get_stack_trace(lines);
		
		for(size_t i = 0; i < lines.size(); ++i)
		{
			gFailLog << lines[i] << std::endl;
		}

		gFailLog << "End of Stack Trace." << std::endl << std::endl;

		gFailLog.flush();
	}
};

void ll_close_fail_log()
{
	gFailLog.close();
}

LLMatrix4 gGLObliqueProjectionInverse;

std::list<LLGLUpdate*> LLGLUpdate::sGLQ;

LLGLManager gGLManager;

LLGLManager::LLGLManager() :
	mInited(FALSE),
	mIsDisabled(FALSE),

	mHasMultitexture(FALSE),
	mHasATIMemInfo(FALSE),
	mHasNVXMemInfo(FALSE),
#if LL_LINUX
	mHasMESAQueryRenderer(FALSE),
#endif
	mNumTextureUnits(1),
	mHasMipMapGeneration(FALSE),
	mHasCompressedTextures(FALSE),
	mHasFramebufferObject(FALSE),
	//mMaxSamples(0),
	mHasFramebufferMultisample(FALSE),
	mHasBlendFuncSeparate(FALSE),
	mHasSync(FALSE),
	mHasVertexBufferObject(FALSE),
	mHasVertexArrayObject(FALSE),
	mHasMapBufferRange(FALSE),
	mHasFlushBufferRange(FALSE),
	mHasPBuffer(FALSE),
	mHasShaderObjects(FALSE),
	mNumTextureImageUnits(0),
	mHasOcclusionQuery(FALSE),
	mHasTimerQuery(FALSE),
	mHasOcclusionQuery2(FALSE),
	mHasPointParameters(FALSE),
	mHasDrawBuffers(FALSE),
	mHasDepthClamp(FALSE),
	mHasTransformFeedback(FALSE),
	mMaxSampleMaskWords(0),
	mMaxColorTextureSamples(0),
	mMaxDepthTextureSamples(0),
	//mMaxIntegerSamples(0),

	mHasAnisotropic(FALSE),
	mHasARBEnvCombine(FALSE),
	mHasCubeMap(FALSE),
	mHasDebugOutput(FALSE),
	mHassRGBTexture(FALSE),
	mHassRGBFramebuffer(FALSE),
	mHasAdaptiveVSync(FALSE),
	mHasTextureSwizzle(FALSE),
	mHasGpuShader5(FALSE),
	mIsATI(FALSE),
	mIsNVIDIA(FALSE),
	mIsIntel(FALSE),
	mIsHD3K(FALSE),
	mATIOffsetVerticalLines(FALSE),
	mATIOldDriver(FALSE),
#if LL_DARWIN
	mIsMobileGF(FALSE),
#endif
	mHasRequirements(TRUE),

	mHasSeparateSpecularColor(FALSE),

	mDebugGPU(FALSE),

	mDriverVersionMajor(1),
	mDriverVersionMinor(0),
	mDriverVersionRelease(0),
	mGLVersion(1.0f),
	mGLSLVersionMajor(0),
	mGLSLVersionMinor(0),		
	mVRAM(0),
	mGLMaxVertexRange(0),
	mGLMaxIndexRange(0),
	mGLMaxTextureSize(0),
	mGLMaxVertexUniformComponents(0)
{
}

//---------------------------------------------------------------------
// Global initialization for GL
//---------------------------------------------------------------------
void LLGLManager::initWGL()
{
	mHasPBuffer = FALSE;
#if LL_WINDOWS && !LL_MESA_HEADLESS
	GLenum err = wglewInit();
	if (GLEW_OK != err)
	{
		LL_WARNS("RenderInit") << "GLEW WGL Extension init failure." << LL_ENDL;
	}

	if (!WGLEW_ARB_pixel_format)
	{
		LL_WARNS("RenderInit") << "No ARB pixel format extensions" << LL_ENDL;
	}

	if (!WGLEW_ARB_create_context)
	{
		LL_WARNS("RenderInit") << "No ARB create context extensions" << LL_ENDL;
	}
	if (!WGLEW_EXT_swap_control)
	{
		LL_WARNS("RenderInit") << "No EXT WGL swap control extensions" << LL_ENDL;
	}

	if (!WGLEW_ARB_pbuffer)
	{
		LL_WARNS("RenderInit") << "No ARB WGL PBuffer extensions" << LL_ENDL;
	}

	if( !WGLEW_ARB_render_texture )
	{
		LL_WARNS("RenderInit") << "No ARB WGL render texture extensions" << LL_ENDL;
	}

	mHasPBuffer = WGLEW_ARB_pbuffer && WGLEW_ARB_render_texture && WGLEW_ARB_pixel_format;
#endif
}

// return false if unable (or unwilling due to old drivers) to init GL
bool LLGLManager::initGL()
{
	if (mInited)
	{
		LL_ERRS("RenderInit") << "Calling init on LLGLManager after already initialized!" << LL_ENDL;
	}

	stop_glerror();

#if !LL_DARWIN
	GLenum err = glewInit();
	if (GLEW_OK != err)
	{
		LL_WARNS("RenderInit") << "GLEW Extension init failure." << LL_ENDL;
		return false;
	}
	stop_glerror();
#endif

	// Extract video card strings and convert to upper case to
	// work around driver-to-driver variation in capitalization.
	mGLVendor = std::string((const char *)glGetString(GL_VENDOR));
	LLStringUtil::toUpper(mGLVendor);

	mGLRenderer = std::string((const char *)glGetString(GL_RENDERER));
	LLStringUtil::toUpper(mGLRenderer);

	parse_gl_version( &mDriverVersionMajor, 
		&mDriverVersionMinor, 
		&mDriverVersionRelease, 
		&mDriverVersionVendorString,
		&mGLVersionString);

	mGLVersion = mDriverVersionMajor + mDriverVersionMinor * .1f;

	if (mGLVersion >= 2.f)
	{
		parse_glsl_version(mGLSLVersionMajor, mGLSLVersionMinor);

#if LL_DARWIN
		//never use GLSL greater than 1.20 on OSX
		if (mGLSLVersionMajor > 1 || mGLSLVersionMinor >= 30)
		{
			mGLSLVersionMajor = 1;
			mGLSLVersionMinor = 20;
		}
#endif
	}

	if (mGLVersion >= 2.1f && LLImageGL::sCompressTextures)
	{ //use texture compression
		glHint(GL_TEXTURE_COMPRESSION_HINT, GL_NICEST);
	}
	else
	{ //GL version is < 3.0, always disable texture compression
		LLImageGL::sCompressTextures = false;
	}
	
	// Trailing space necessary to keep "nVidia Corpor_ati_on" cards
	// from being recognized as ATI.
	if (mGLVendor.substr(0,4) == "ATI "
//#if LL_LINUX
//		// The Mesa-based drivers put this in the Renderer string,
//		// not the Vendor string.
//		|| mGLRenderer.find("AMD") != std::string::npos
//#endif //LL_LINUX
		)
	{
		mGLVendorShort = "ATI";
		// *TODO: Fix this?
		mIsATI = TRUE;

#if LL_WINDOWS && !LL_MESA_HEADLESS
		if (mDriverVersionRelease < 3842)
		{
			mATIOffsetVerticalLines = TRUE;
		}
#endif // LL_WINDOWS

#if (LL_WINDOWS || LL_LINUX) && !LL_MESA_HEADLESS
		// count any pre OpenGL 3.0 implementation as an old driver
		if (mGLVersion < 3.f) 
		{
			mATIOldDriver = TRUE;
		}
#endif // (LL_WINDOWS || LL_LINUX) && !LL_MESA_HEADLESS
	}
	else if (mGLVendor.find("NVIDIA ") != std::string::npos)
	{
		mGLVendorShort = "NVIDIA";
		mIsNVIDIA = TRUE;
#if LL_DARWIN
		if ((mGLRenderer.find("9400M") != std::string::npos)
			  || (mGLRenderer.find("9600M") != std::string::npos)
			  || (mGLRenderer.find("9800M") != std::string::npos))
		{
			mIsMobileGF = TRUE;
		}
#endif

	}
	else if (mGLVendor.find("INTEL") != std::string::npos
#if LL_LINUX
		 // The Mesa-based drivers put this in the Renderer string,
		 // not the Vendor string.
		 || mGLRenderer.find("INTEL") != std::string::npos
#endif //LL_LINUX
		 )
	{
		mGLVendorShort = "INTEL";
		mIsIntel = TRUE;
#if LL_WINDOWS
		if (mGLRenderer.find("HD") != std::string::npos 
			&& ((mGLRenderer.find("2000") != std::string::npos || mGLRenderer.find("3000") != std::string::npos) 
				|| (mGLVersion == 3.1f && mGLRenderer.find("INTEL(R) HD GRAPHICS") != std::string::npos)))
		{
			mIsHD3K = TRUE;
		}
#endif
	}
	else
	{
		mGLVendorShort = "MISC";
	}
	
	stop_glerror();
	// This is called here because it depends on the setting of mIsGF2or4MX, and sets up mHasMultitexture.
	initExtensions();
	stop_glerror();

	S32 old_vram = mVRAM;

#if GLX_MESA_query_renderer
	if (mHasMESAQueryRenderer)
	{
		U32 video_memory = 0;
		glXQueryCurrentRendererIntegerMESA(GLX_RENDERER_VIDEO_MEMORY_MESA, &video_memory);
		mVRAM = video_memory;
	}
	else
#endif
	if (mHasATIMemInfo)
	{ //ask the gl how much vram is free at startup and attempt to use no more than half of that
		S32 meminfo[4];
		glGetIntegerv(GL_TEXTURE_FREE_MEMORY_ATI, meminfo);

		mVRAM = meminfo[0]/1024;
	}
	else if (mHasNVXMemInfo)
	{
		S32 dedicated_memory;
		glGetIntegerv(GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX, &dedicated_memory);
		mVRAM = dedicated_memory/1024;
	}

	if (mVRAM < 256)
	{ //something likely went wrong using the above extensions, fall back to old method
		mVRAM = old_vram;
	}

	stop_glerror();

	stop_glerror();

	if (mHasShaderObjects)
	{
		GLint num_tex_image_units;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &num_tex_image_units);
		mNumTextureImageUnits = llmin(num_tex_image_units, 32);

		//According to the spec, the resulting value should never be less than 512. We need at least 1024 to use skinned shaders.
		glGetIntegerv(GL_MAX_VERTEX_UNIFORM_COMPONENTS_ARB, &mGLMaxVertexUniformComponents);
		if (mIsHD3K)
		{
			mGLMaxVertexUniformComponents = llmax(mGLMaxVertexUniformComponents, 1024);
		}
	}

	if (LLRender::sGLCoreProfile)
	{
		mNumTextureUnits = llmin(mNumTextureImageUnits, MAX_GL_TEXTURE_UNITS);
	}
	else if (mHasMultitexture)
	{
		GLint num_tex_units;		
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &num_tex_units);
		mNumTextureUnits = llmin(num_tex_units, (GLint)MAX_GL_TEXTURE_UNITS);
		if (mIsIntel)
		{
			mNumTextureUnits = llmin(mNumTextureUnits, 2);
		}
	}
	else
	{
		mHasRequirements = FALSE;

		// We don't support cards that don't support the GL_ARB_multitexture extension
		LL_WARNS("RenderInit") << "GL Drivers do not support GL_ARB_multitexture" << LL_ENDL;
		return false;
	}
	
	stop_glerror();

	//if (mHasFramebufferMultisample)
	//{
	//	glGetIntegerv(GL_MAX_INTEGER_SAMPLES, &mMaxIntegerSamples);
	//	glGetIntegerv(GL_MAX_SAMPLES, &mMaxSamples);
	//}

	stop_glerror();

#if LL_WINDOWS
	if (mHasDebugOutput && gDebugGL)
	{ //setup debug output callback
		//glDebugMessageControlARB(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_LOW_ARB, 0, NULL, GL_TRUE);
		glDebugMessageCallbackARB((GLDEBUGPROCARB) gl_debug_callback, nullptr);
		if (mIsNVIDIA)
		{
			GLuint annoyingspam[1] = { 131185 };
			glDebugMessageControlARB(GL_DEBUG_SOURCE_API_ARB, GL_DEBUG_TYPE_OTHER_ARB, GL_DONT_CARE, 1, annoyingspam, GL_FALSE);
		}
		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);
	}
#endif

	stop_glerror();


#if LL_WINDOWS
	if (mIsIntel && mGLVersion <= 3.f)
	{ //never try to use framebuffer objects on older intel drivers (crashy)
		mHasFramebufferObject = FALSE;
	}
#endif

	setToDebugGPU();

	stop_glerror();

	initGLStates();

	stop_glerror();

	return true;
}

void LLGLManager::setToDebugGPU()
{
	//"MOBILE INTEL(R) 965 EXPRESS CHIP", 
	if (mGLRenderer.find("INTEL") != std::string::npos && mGLRenderer.find("965") != std::string::npos)
	{
		mDebugGPU = TRUE ;
	}

	return ;
}

void LLGLManager::getGLInfo(LLSD& info)
{
	if (gHeadlessClient)
	{
		info["GLInfo"]["GLVendor"] = HEADLESS_VENDOR_STRING;
		info["GLInfo"]["GLRenderer"] = HEADLESS_RENDERER_STRING;
		info["GLInfo"]["GLVersion"] = HEADLESS_VERSION_STRING;
		return;
	}
	else
	{
		info["GLInfo"]["GLVendor"] = std::string((const char *)glGetString(GL_VENDOR));
		info["GLInfo"]["GLRenderer"] = std::string((const char *)glGetString(GL_RENDERER));
		info["GLInfo"]["GLVersion"] = std::string((const char *)glGetString(GL_VERSION));
	}

#if !LL_MESA_HEADLESS
	std::string all_exts = parse_gl_ext_to_str(mGLVersion);
	boost::char_separator<char> sep(" ");
	boost::tokenizer<boost::char_separator<char> > tok(all_exts, sep);
	for(boost::tokenizer<boost::char_separator<char> >::iterator i = tok.begin(); i != tok.end(); ++i)
	{
		info["GLInfo"]["GLExtensions"].append(*i);
	}
#endif
}

std::string LLGLManager::getGLInfoString()
{
	std::string info_str;

	if (gHeadlessClient)
	{
		info_str += std::string("GL_VENDOR      ") + HEADLESS_VENDOR_STRING + std::string("\n");
		info_str += std::string("GL_RENDERER    ") + HEADLESS_RENDERER_STRING + std::string("\n");
		info_str += std::string("GL_VERSION     ") + HEADLESS_VERSION_STRING + std::string("\n");
	}
	else
	{
		info_str += std::string("GL_VENDOR      ") + ll_safe_string((const char *)glGetString(GL_VENDOR)) + std::string("\n");
		info_str += std::string("GL_RENDERER    ") + ll_safe_string((const char *)glGetString(GL_RENDERER)) + std::string("\n");
		info_str += std::string("GL_VERSION     ") + ll_safe_string((const char *)glGetString(GL_VERSION)) + std::string("\n");
	}

#if !LL_MESA_HEADLESS
	std::string all_exts = parse_gl_ext_to_str(mGLVersion);
	LLStringUtil::replaceChar(all_exts, ' ', '\n');
	info_str += std::string("GL_EXTENSIONS:\n") + all_exts + std::string("\n");
#endif
	
	return info_str;
}

void LLGLManager::printGLInfoString()
{
	if (gHeadlessClient)
	{
		LL_INFOS("RenderInit") << "GL_VENDOR:     " << HEADLESS_VENDOR_STRING << LL_ENDL;
		LL_INFOS("RenderInit") << "GL_RENDERER:   " << HEADLESS_RENDERER_STRING << LL_ENDL;
		LL_INFOS("RenderInit") << "GL_VERSION:    " << HEADLESS_VERSION_STRING << LL_ENDL;
	}
	else
	{
		LL_INFOS("RenderInit") << "GL_VENDOR:     " << ((const char *)glGetString(GL_VENDOR)) << LL_ENDL;
		LL_INFOS("RenderInit") << "GL_RENDERER:   " << ((const char *)glGetString(GL_RENDERER)) << LL_ENDL;
		LL_INFOS("RenderInit") << "GL_VERSION:    " << ((const char *)glGetString(GL_VERSION)) << LL_ENDL;
	}

#if !LL_MESA_HEADLESS
	std::string all_exts = parse_gl_ext_to_str(mGLVersion);
	LLStringUtil::replaceChar(all_exts, ' ', '\n');
	LL_DEBUGS("RenderInit") << "GL_EXTENSIONS:\n" << all_exts << LL_ENDL;
#endif
}

std::string LLGLManager::getRawGLString()
{
	std::string gl_string;
	if (gHeadlessClient)
	{
		gl_string = HEADLESS_VENDOR_STRING + " " + HEADLESS_RENDERER_STRING;
	}
	else
	{
		gl_string = ll_safe_string((char*)glGetString(GL_VENDOR)) + " " + ll_safe_string((char*)glGetString(GL_RENDERER));
	}
	return gl_string;
}

void LLGLManager::shutdownGL()
{
	if (mInited)
	{
		glFinish();
		stop_glerror();
		mInited = FALSE;
	}
}

// these are used to turn software blending on. They appear in the Debug/Avatar menu
// presence of vertex skinning/blending or vertex programs will set these to FALSE by default.

void LLGLManager::initExtensions()
{
#if LL_MESA_HEADLESS
# ifdef GL_ARB_multitexture
	mHasMultitexture = TRUE;
# else
	mHasMultitexture = FALSE;
# endif // GL_ARB_multitexture
# ifdef GL_ARB_texture_env_combine
	mHasARBEnvCombine = TRUE;	
# else
	mHasARBEnvCombine = FALSE;
# endif // GL_ARB_texture_env_combine
# ifdef GL_ARB_texture_compression
	mHasCompressedTextures = TRUE;
# else
	mHasCompressedTextures = FALSE;
# endif // GL_ARB_texture_compression
# ifdef GL_ARB_vertex_buffer_object
	mHasVertexBufferObject = TRUE;
# else
	mHasVertexBufferObject = FALSE;
# endif // GL_ARB_vertex_buffer_object
# ifdef GL_EXT_framebuffer_object
	mHasFramebufferObject = TRUE;
# else
	mHasFramebufferObject = FALSE;
# endif // GL_EXT_framebuffer_object
# ifdef GL_EXT_framebuffer_multisample
	mHasFramebufferMultisample = TRUE;
# else
	mHasFramebufferMultisample = FALSE;
# endif // GL_EXT_framebuffer_multisample
# ifdef GL_ARB_draw_buffers
	mHasDrawBuffers = TRUE;
#else
	mHasDrawBuffers = FALSE;
# endif // GL_ARB_draw_buffers
# if defined(GL_NV_depth_clamp) || defined(GL_ARB_depth_clamp)
	mHasDepthClamp = TRUE;
#else
	mHasDepthClamp = FALSE;
#endif // defined(GL_NV_depth_clamp) || defined(GL_ARB_depth_clamp)
# if GL_EXT_blend_func_separate
	mHasBlendFuncSeparate = TRUE;
#else
	mHasBlendFuncSeparate = FALSE;
# endif // GL_EXT_blend_func_separate
	mHasMipMapGeneration = FALSE;
	mHasSeparateSpecularColor = FALSE;
	mHasAnisotropic = FALSE;
	mHasCubeMap = FALSE;
	mHasOcclusionQuery = FALSE;
	mHasPointParameters = FALSE;
	mHasShaderObjects = FALSE;
#elif LL_DARWIN
    std::set<std::string> extensions;
    const auto extensionString = glGetString(GL_EXTENSIONS);
    
    if (extensionString)
    {
        std::istringstream stream{reinterpret_cast<const char *>(extensionString)};
        auto extensionName = std::string{};
        while (std::getline(stream, extensionName, ' '))
        {
            extensions.insert(extensionName);
        }
    }
    
    mHasMultitexture = extensions.find("GL_ARB_multitexture") != extensions.end();
    mHasATIMemInfo = extensions.find("GL_ATI_meminfo") != extensions.end();
    mHasNVXMemInfo = extensions.find("GL_NVX_gpu_memory_info") != extensions.end();
    mHasSeparateSpecularColor = extensions.find("GL_EXT_separate_specular_color") != extensions.end();
    mHasAnisotropic = extensions.find("GL_EXT_texture_filter_anisotropic") != extensions.end();
    mHasCubeMap = extensions.find("GL_ARB_texture_cube_map") != extensions.end();
    mHasARBEnvCombine = extensions.find("GL_ARB_texture_env_combine") != extensions.end();
    mHasCompressedTextures = extensions.find("GL_ARB_texture_compression") != extensions.end();
    mHasOcclusionQuery = extensions.find("GL_ARB_occlusion_query") != extensions.end();
    mHasTimerQuery = extensions.find("GL_ARB_timer_query") != extensions.end();
    mHasOcclusionQuery2 = extensions.find("GL_ARB_occlusion_query2") != extensions.end();
    mHasVertexBufferObject = extensions.find("GL_ARB_vertex_buffer_object") != extensions.end();
    mHasVertexArrayObject = extensions.find("GL_ARB_vertex_array_object") != extensions.end();
    mHasSync = extensions.find("GL_ARB_sync") != extensions.end();
    mHasMapBufferRange = extensions.find("GL_ARB_map_buffer_range") != extensions.end();
    mHasFlushBufferRange = extensions.find("GL_APPLE_flush_buffer_range") != extensions.end();
    mHasDepthClamp = extensions.find("GL_ARB_depth_clamp") != extensions.end()
                     || extensions.find("GL_NV_depth_clamp") != extensions.end();
    // mask out FBO support when packed_depth_stencil isn't there 'cause we need it for LLRenderTarget -Brad
    #ifdef GL_ARB_framebuffer_object
    mHasFramebufferObject = extensions.find("GL_ARB_framebuffer_object") != extensions.end();
#else
    mHasFramebufferObject = extensions.find("GL_EXT_framebuffer_object") != extensions.end() &&
    extensions.find("GL_EXT_framebuffer_blit") != extensions.end() &&
    extensions.find("GL_EXT_framebuffer_multisample") != extensions.end() &&
    extensions.find("GL_EXT_packed_depth_stencil") != extensions.end();
#endif

    mHasFramebufferMultisample = mHasFramebufferObject && extensions.find("GL_EXT_framebuffer_multisample") != extensions.end();

#ifdef GL_EXT_texture_sRGB
    mHassRGBTexture = extensions.find("GL_EXT_texture_sRGB") != extensions.end();
#endif
    
#ifdef GL_ARB_framebuffer_sRGB
    mHassRGBFramebuffer = extensions.find("GL_ARB_framebuffer_sRGB") != extensions.end();
#else
    mHassRGBFramebuffer = extensions.find("GL_EXT_framebuffer_sRGB") != extensions.end();
#endif
    
    mHasMipMapGeneration = mHasFramebufferObject || mGLVersion >= 1.4f;
    
    mHasDrawBuffers = extensions.find("GL_ARB_draw_buffers") != extensions.end();
    mHasBlendFuncSeparate = extensions.find("GL_EXT_blend_func_separate") != extensions.end();
    mHasDebugOutput = extensions.find("GL_ARB_debug_output") != extensions.end();
    mHasTransformFeedback = mGLVersion >= 4.f || extensions.find("GL_EXT_transform_feedback") != extensions.end();
#if !LL_DARWIN
    mHasPointParameters = !mIsATI && extensions.find("GL_ARB_point_parameters") != extensions.end();
#endif
    mHasShaderObjects = mGLVersion >= 2.f;
    
#ifdef GL_ARB_texture_swizzle
    mHasTextureSwizzle = extensions.find("GL_ARB_texture_swizzle") != extensions.end();
#endif
#ifdef GL_ARB_gpu_shader5
    mHasGpuShader5 = extensions.find("GL_ARB_gpu_shader5") != extensions.end();
#endif
    
#else // LL_MESA_HEADLESS
	mHasMultitexture = GLEW_ARB_multitexture;
	mHasATIMemInfo = GLEW_ATI_meminfo;
	mHasNVXMemInfo = GLEW_NVX_gpu_memory_info;
#if GLX_MESA_query_renderer
	mHasMESAQueryRenderer = FALSE; //GLXEW_MESA_query_renderer;
#endif
	mHasSeparateSpecularColor = GLEW_EXT_separate_specular_color;
	mHasAnisotropic = GLEW_EXT_texture_filter_anisotropic;
	mHasCubeMap = GLEW_ARB_texture_cube_map;
	mHasARBEnvCombine = GLEW_ARB_texture_env_combine;
	mHasCompressedTextures = GLEW_ARB_texture_compression;
	mHasOcclusionQuery = GLEW_ARB_occlusion_query;
	mHasTimerQuery = GLEW_ARB_timer_query;
	mHasOcclusionQuery2 = GLEW_ARB_occlusion_query2;
	mHasVertexBufferObject = GLEW_ARB_vertex_buffer_object;
	mHasVertexArrayObject = GLEW_ARB_vertex_array_object;
	mHasSync = GLEW_ARB_sync;
	mHasMapBufferRange = GLEW_ARB_map_buffer_range;
	mHasFlushBufferRange = GLEW_APPLE_flush_buffer_range;
	mHasDepthClamp = GLEW_ARB_depth_clamp || GLEW_NV_depth_clamp;
	// mask out FBO support when packed_depth_stencil isn't there 'cause we need it for LLRenderTarget -Brad
#ifdef GL_ARB_framebuffer_object
	mHasFramebufferObject = GLEW_ARB_framebuffer_object;
#else
	mHasFramebufferObject = GLEW_EXT_framebuffer_object &&
							GLEW_EXT_framebuffer_blit &&
							GLEW_EXT_framebuffer_multisample &&
							GLEW_EXT_packed_depth_stencil;
#endif

	mHasFramebufferMultisample = mHasFramebufferObject && GLEW_EXT_framebuffer_multisample;

#ifdef GL_EXT_texture_sRGB
	mHassRGBTexture = GLEW_EXT_texture_sRGB;
#endif
	
#ifdef GL_ARB_framebuffer_sRGB
	mHassRGBFramebuffer = GLEW_ARB_framebuffer_sRGB;
#else
	mHassRGBFramebuffer = GLEW_EXT_framebuffer_sRGB;
#endif

	mHasMipMapGeneration = mHasFramebufferObject || mGLVersion >= 1.4f;

	mHasDrawBuffers = GLEW_ARB_draw_buffers;
	mHasBlendFuncSeparate = GLEW_EXT_blend_func_separate;
	mHasDebugOutput = GLEW_ARB_debug_output;
	mHasTransformFeedback = mGLVersion >= 4.f || GLEW_EXT_transform_feedback;
#if !LL_DARWIN
	mHasPointParameters = !mIsATI && GLEW_ARB_point_parameters;
#endif
	mHasShaderObjects = mGLVersion >= 2.f;
#endif

#if WGL_EXT_swap_control && WGL_EXT_extensions_string
	mHasAdaptiveVSync = WGLEW_EXT_swap_control_tear;
#elif LL_LINUX && GLX_EXT_swap_control_tear
	mHasAdaptiveVSync = GLXEW_EXT_swap_control_tear;
#endif
#ifdef GL_ARB_texture_swizzle
	mHasTextureSwizzle = GLEW_ARB_texture_swizzle;
#endif
#ifdef GL_ARB_gpu_shader5
	mHasGpuShader5 = GLEW_ARB_gpu_shader5;
#endif

#if LL_LINUX
	LL_INFOS() << "initExtensions() checking shell variables to adjust features..." << LL_ENDL;
	// Our extension support for the Linux Client is very young with some
	// potential driver gotchas, so offer a semi-secret way to turn it off.
	if (getenv("LL_GL_NOEXT"))
	{
		//mHasMultitexture = FALSE; // NEEDED!
		mHasDepthClamp = FALSE;
		mHasARBEnvCombine = FALSE;
		mHasCompressedTextures = FALSE;
		mHasVertexBufferObject = FALSE;
		mHasFramebufferObject = FALSE;
		mHasDrawBuffers = FALSE;
		mHasBlendFuncSeparate = FALSE;
		mHasMipMapGeneration = FALSE;
		mHasSeparateSpecularColor = FALSE;
		mHasAnisotropic = FALSE;
		mHasCubeMap = FALSE;
		mHasOcclusionQuery = FALSE;
		mHasPointParameters = FALSE;
		mHasShaderObjects = FALSE;
		mHasTextureSwizzle = FALSE;
		mHasGpuShader5 = FALSE;
		LL_WARNS("RenderInit") << "GL extension support DISABLED via LL_GL_NOEXT" << LL_ENDL;
	}
	else if (getenv("LL_GL_BASICEXT"))	/* Flawfinder: ignore */
	{
		// This switch attempts to turn off all support for exotic
		// extensions which I believe correspond to fatal driver
		// bug reports.  This should be the default until we get a
		// proper blacklist/whitelist on Linux.
		mHasMipMapGeneration = FALSE;
		mHasAnisotropic = FALSE;
		//mHasCubeMap = FALSE; // apparently fatal on Intel 915 & similar
		//mHasOcclusionQuery = FALSE; // source of many ATI system hangs
		mHasShaderObjects = FALSE;
		mHasBlendFuncSeparate = FALSE;
		LL_WARNS("RenderInit") << "GL extension support forced to SIMPLE level via LL_GL_BASICEXT" << LL_ENDL;
	}
	if (getenv("LL_GL_BLACKLIST"))	/* Flawfinder: ignore */
	{
		// This lets advanced troubleshooters disable specific
		// GL extensions to isolate problems with their hardware.
		// SL-28126
		const char *const blacklist = getenv("LL_GL_BLACKLIST");	/* Flawfinder: ignore */
		LL_WARNS("RenderInit") << "GL extension support partially disabled via LL_GL_BLACKLIST: " << blacklist << LL_ENDL;
		if (strchr(blacklist,'a')) mHasARBEnvCombine = FALSE;
		if (strchr(blacklist,'b')) mHasCompressedTextures = FALSE;
		if (strchr(blacklist,'c')) mHasVertexBufferObject = FALSE;
		if (strchr(blacklist,'d')) mHasMipMapGeneration = FALSE;//S
// 		if (strchr(blacklist,'f')) mHasNVVertexArrayRange = FALSE;//S
// 		if (strchr(blacklist,'g')) mHasNVFence = FALSE;//S
		if (strchr(blacklist,'h')) mHasSeparateSpecularColor = FALSE;
		if (strchr(blacklist,'i')) mHasAnisotropic = FALSE;//S
		if (strchr(blacklist,'j')) mHasCubeMap = FALSE;//S
// 		if (strchr(blacklist,'k')) mHasATIVAO = FALSE;//S
		if (strchr(blacklist,'l')) mHasOcclusionQuery = FALSE;
		if (strchr(blacklist,'m')) mHasShaderObjects = FALSE;//S
		if (strchr(blacklist,'p')) mHasPointParameters = FALSE;//S
		if (strchr(blacklist,'q')) mHasFramebufferObject = FALSE;//S
		if (strchr(blacklist,'r')) mHasDrawBuffers = FALSE;//S
		if (strchr(blacklist,'t')) mHasBlendFuncSeparate = FALSE;//S
		if (strchr(blacklist,'u')) mHasDepthClamp = FALSE;
		
	}
#endif // LL_LINUX
	
	if (!mHasMultitexture)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize multitexturing" << LL_ENDL;
	}
	if (!mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize mipmap generation" << LL_ENDL;
	}
	if (!mHasARBEnvCombine)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_texture_env_combine" << LL_ENDL;
	}
	if (!mHasSeparateSpecularColor)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize separate specular color" << LL_ENDL;
	}
	if (!mHasAnisotropic)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize anisotropic filtering" << LL_ENDL;
	}
	if (!mHasCompressedTextures)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_texture_compression" << LL_ENDL;
	}
	if (!mHasOcclusionQuery)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_occlusion_query" << LL_ENDL;
	}
	if (!mHasOcclusionQuery2)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_occlusion_query2" << LL_ENDL;
	}
	if (!mHasPointParameters)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_point_parameters" << LL_ENDL;
	}
	if (!mHasShaderObjects)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_shader_objects" << LL_ENDL;
	}
	if (!mHasBlendFuncSeparate)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_EXT_blend_func_separate" << LL_ENDL;
	}
	if (!mHasDrawBuffers)
	{
		LL_INFOS("RenderInit") << "Couldn't initialize GL_ARB_draw_buffers" << LL_ENDL;
	}

	// Disable certain things due to known bugs
	if (mIsIntel && mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Disabling mip-map generation for Intel GPUs" << LL_ENDL;
		mHasMipMapGeneration = FALSE;
	}
// This bug has been fixed since this.
#if 0
	if (mIsATI && mHasMipMapGeneration)
	{
		LL_INFOS("RenderInit") << "Disabling mip-map generation for ATI GPUs (performance opt)" << LL_ENDL;
		mHasMipMapGeneration = FALSE;
	}
#endif
	
	// Misc
	glGetIntegerv(GL_MAX_ELEMENTS_VERTICES, (GLint*) &mGLMaxVertexRange);
	glGetIntegerv(GL_MAX_ELEMENTS_INDICES, (GLint*) &mGLMaxIndexRange);
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, (GLint*) &mGLMaxTextureSize);

	mInited = TRUE;
}

void flush_glerror()
{
	glGetError();
}

const std::string get_gl_err_string(GLenum error)
{
	switch (error)
	{
	case GL_NO_ERROR:          return "No error";
	case GL_INVALID_ENUM:      return "Invalid enum";
	case GL_INVALID_VALUE:     return "Invalid value";
	case GL_INVALID_OPERATION: return "Invalid operation";
	case GL_INVALID_FRAMEBUFFER_OPERATION: return "Invalid framebuffer operation";
	case GL_OUT_OF_MEMORY:     return "Out of memory";
	case GL_STACK_OVERFLOW:    return "Stack overflow";
	case GL_STACK_UNDERFLOW:   return "Stack underflow";
	case GL_TABLE_TOO_LARGE:   return "Table too large";
	default:                   return "";
	}
}

//this function outputs gl error to the log file, does not crash the code.
void log_glerror()
{
	if (LL_UNLIKELY(!gGLManager.mInited))
	{
		return ;
	}
	//  Create or update texture to be used with this data 
	GLenum error;
	error = glGetError();
	while (LL_UNLIKELY(error))
	{
		const std::string gl_error_msg = get_gl_err_string(error);
		if (!gl_error_msg.empty())
		{
			LL_WARNS() << "GL Error: " << error << " GL Error String: " << gl_error_msg.c_str() << LL_ENDL ;			
		}
		else
		{
			// you'll probably have to grep for the number in glext.h.
			LL_WARNS() << "GL Error: UNKNOWN 0x" << std::hex << error << std::dec << LL_ENDL;
		}
		error = glGetError();
	}
}

void do_assert_glerror()
{
	//  Create or update texture to be used with this data 
	GLenum error;
	error = glGetError();
	BOOL quit = FALSE;
	while (LL_UNLIKELY(error))
	{
		quit = TRUE;
		const std::string gl_error_msg = get_gl_err_string(error);
		if (!gl_error_msg.empty())
		{
			LL_WARNS("RenderState") << "GL Error:" << error<< LL_ENDL;
			LL_WARNS("RenderState") << "GL Error String:" << gl_error_msg.c_str() << LL_ENDL;

			if (gDebugSession)
			{
				gFailLog << "GL Error:" << gl_error_msg << std::endl;
			}
		}
		else
		{
			// you'll probably have to grep for the number in glext.h.
			LL_WARNS("RenderState") << "GL Error: UNKNOWN 0x" << std::hex << error << std::dec << LL_ENDL;

			if (gDebugSession)
			{
				gFailLog << "GL Error: UNKNOWN 0x" << std::hex << error << std::dec << std::endl;
			}
		}
		error = glGetError();
	}

	if (quit)
	{
		if (gDebugSession)
		{
			ll_fail("assert_glerror failed");
		}
		else
		{
			LL_ERRS() << "One or more unhandled GL errors." << LL_ENDL;
		}
	}
}

void assert_glerror()
{
/*	if (!gGLActive)
	{
		//LL_WARNS() << "GL used while not active!" << LL_ENDL;

		if (gDebugSession)
		{
			//ll_fail("GL used while not active");
		}
	}
*/

	if (!gDebugGL) 
	{
		//funny looking if for branch prediction -- gDebugGL is almost always false and assert_glerror is called often
	}
	else
	{
		do_assert_glerror();
	}
}
	

void clear_glerror()
{
	glGetError();
}

///////////////////////////////////////////////////////////////
//
// LLGLState
//

// Static members
boost::unordered_map<LLGLenum, LLGLboolean> LLGLState::sStateMap;

GLboolean LLGLDepthTest::sDepthEnabled = GL_FALSE; // OpenGL default
GLenum LLGLDepthTest::sDepthFunc = GL_LESS; // OpenGL default
GLboolean LLGLDepthTest::sWriteEnabled = GL_TRUE; // OpenGL default

//static
void LLGLState::initClass() 
{
	sStateMap[GL_DITHER] = GL_TRUE;
	// sStateMap[GL_TEXTURE_2D] = GL_TRUE;
	
	//make sure multisample defaults to disabled
	sStateMap[GL_MULTISAMPLE_ARB] = GL_FALSE;
	glDisable(GL_MULTISAMPLE_ARB);
}

//static
void LLGLState::restoreGL()
{
	sStateMap.clear();
	initClass();
}

//static
// Really shouldn't be needed, but seems we sometimes do.
void LLGLState::resetTextureStates()
{
	gGL.flush();
	GLint maxTextureUnits;
	
	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &maxTextureUnits);
	for (S32 j = maxTextureUnits-1; j >=0; j--)
	{
		gGL.getTexUnit(j)->activate();
		glClientActiveTextureARB(GL_TEXTURE0_ARB+j);
		j == 0 ? gGL.getTexUnit(j)->enable(LLTexUnit::TT_TEXTURE) : gGL.getTexUnit(j)->disable();
	}
}

void LLGLState::dumpStates() 
{
	LL_INFOS("RenderState") << "GL States:" << LL_ENDL;
	for (boost::unordered_map<LLGLenum, LLGLboolean>::iterator iter = sStateMap.begin();
		 iter != sStateMap.end(); ++iter)
	{
		LL_INFOS("RenderState") << llformat(" 0x%04x : %s",(S32)iter->first,iter->second?"TRUE":"FALSE") << LL_ENDL;
	}
}

void LLGLState::checkStates(const std::string& msg)  
{
	if (!gDebugGL)
	{
		return;
	}

	stop_glerror();

	GLint src;
	GLint dst;
	glGetIntegerv(GL_BLEND_SRC, &src);
	glGetIntegerv(GL_BLEND_DST, &dst);
	
	stop_glerror();

	BOOL error = FALSE;

	if (src != GL_SRC_ALPHA || dst != GL_ONE_MINUS_SRC_ALPHA)
	{
		if (gDebugSession)
		{
			gFailLog << "Blend function corrupted: " << std::hex << src << " " << std::hex << dst << "  " << msg << std::dec << std::endl;
			error = TRUE;
		}
		else
		{
			LL_GL_ERRS << "Blend function corrupted: " << std::hex << src << " " << std::hex << dst << "  " << msg << std::dec << LL_ENDL;
		}
	}
	
	for (boost::unordered_map<LLGLenum, LLGLboolean>::iterator iter = sStateMap.begin();
		 iter != sStateMap.end(); ++iter)
	{
		LLGLenum state = iter->first;
		LLGLboolean cur_state = iter->second;
		stop_glerror();
		LLGLboolean gl_state = glIsEnabled(state);
		stop_glerror();
		if(cur_state != gl_state)
		{
			dumpStates();
			if (gDebugSession)
			{
				gFailLog << llformat("LLGLState error. State: 0x%04x",state) << std::endl;
				error = TRUE;
			}
			else
			{
				LL_GL_ERRS << llformat("LLGLState error. State: 0x%04x",state) << LL_ENDL;
			}
		}
	}
	
	if (error)
	{
		ll_fail("LLGLState::checkStates failed.");
	}
	stop_glerror();
}

void LLGLState::checkTextureChannels(const std::string& msg)
{
#if 0
	if (!gDebugGL)
	{
		return;
	}
	stop_glerror();

	GLint activeTexture;
	glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &activeTexture);
	stop_glerror();

	BOOL error = FALSE;

	if (activeTexture == GL_TEXTURE0_ARB)
	{
		GLint tex_env_mode = 0;

		glGetTexEnviv(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, &tex_env_mode);
		stop_glerror();

		if (tex_env_mode != GL_MODULATE)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL_TEXTURE_ENV_MODE invalid: " << std::hex << tex_env_mode << std::dec << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL_TEXTURE_ENV_MODE invalid: " << std::hex << tex_env_mode << std::dec << std::endl;
			}
		}
	}

	static const char* label[] =
	{
		"GL_TEXTURE_2D",
		"GL_TEXTURE_COORD_ARRAY",
		"GL_TEXTURE_1D",
		"GL_TEXTURE_CUBE_MAP_ARB",
		"GL_TEXTURE_GEN_S",
		"GL_TEXTURE_GEN_T",
		"GL_TEXTURE_GEN_Q",
		"GL_TEXTURE_GEN_R",
		"GL_TEXTURE_RECTANGLE_ARB"
	};

	static GLint value[] =
	{
		GL_TEXTURE_2D,
		GL_TEXTURE_COORD_ARRAY,
		GL_TEXTURE_1D,
		GL_TEXTURE_CUBE_MAP_ARB,
		GL_TEXTURE_GEN_S,
		GL_TEXTURE_GEN_T,
		GL_TEXTURE_GEN_Q,
		GL_TEXTURE_GEN_R,
		GL_TEXTURE_RECTANGLE_ARB
	};

	GLint stackDepth = 0;

	glh::matrix4f mat;
	glh::matrix4f identity;
	identity.identity();

	for (GLint i = 1; i < gGLManager.mNumTextureUnits; i++)
	{
		gGL.getTexUnit(i)->activate();

		if (i < gGLManager.mNumTextureUnits)
		{
			glClientActiveTextureARB(GL_TEXTURE0_ARB+i);
			stop_glerror();
			glGetIntegerv(GL_TEXTURE_STACK_DEPTH, &stackDepth);
			stop_glerror();

			if (stackDepth != 1)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "Texture matrix stack corrupted." << LL_ENDL;

				if (gDebugSession)
				{
					gFailLog << "Texture matrix stack corrupted." << std::endl;
				}
			}

			glGetFloatv(GL_TEXTURE_MATRIX, (GLfloat*) mat.m);
			stop_glerror();

			if (mat != identity)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "Texture matrix in channel " << i << " corrupt." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "Texture matrix in channel " << i << " corrupt." << std::endl;
				}
			}
				
			for (S32 j = (i == 0 ? 1 : 0); 
				j < (gGLManager.mHasTextureRectangle ? 9 : 8); j++)
				
				if (glIsEnabled(value[j]))
				{
					error = TRUE;
					LL_WARNS("RenderState") << "Texture channel " << i << " still has " << label[j] << " enabled." << LL_ENDL;
					if (gDebugSession)
					{
						gFailLog << "Texture channel " << i << " still has " << label[j] << " enabled." << std::endl;
					}
				}
				stop_glerror();
			}

			glGetFloatv(GL_TEXTURE_MATRIX, mat.m);
			stop_glerror();

			if (mat != identity)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "Texture matrix " << i << " is not identity." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "Texture matrix " << i << " is not identity." << std::endl;
				}
			}
		}

		{
			GLint tex = 0;
			stop_glerror();
			glGetIntegerv(GL_TEXTURE_BINDING_2D, &tex);
			stop_glerror();

			if (tex != 0)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "Texture channel " << i << " still has texture " << tex << " bound." << LL_ENDL;

				if (gDebugSession)
				{
					gFailLog << "Texture channel " << i << " still has texture " << tex << " bound." << std::endl;
				}
			}
		}
	}

	stop_glerror();
	gGL.getTexUnit(0)->activate();
	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	stop_glerror();

	if (error)
	{
		if (gDebugSession)
		{
			ll_fail("LLGLState::checkTextureChannels failed.");
		}
		else
		{
			LL_GL_ERRS << "GL texture state corruption detected.  " << msg << LL_ENDL;
		}
	}
#endif
}

void LLGLState::checkClientArrays(const std::string& msg, U32 data_mask)
{
	if (!gDebugGL || LLGLSLShader::sNoFixedFunction)
	{
		return;
	}

	stop_glerror();
	BOOL error = FALSE;

	GLint active_texture;
	glGetIntegerv(GL_CLIENT_ACTIVE_TEXTURE_ARB, &active_texture);

	if (active_texture != GL_TEXTURE0_ARB)
	{
		LL_WARNS() << "Client active texture corrupted: " << active_texture << LL_ENDL;
		if (gDebugSession)
		{
			gFailLog << "Client active texture corrupted: " << active_texture << std::endl;
		}
		error = TRUE;
	}

	/*glGetIntegerv(GL_ACTIVE_TEXTURE_ARB, &active_texture);
	if (active_texture != GL_TEXTURE0_ARB)
	{
		LL_WARNS() << "Active texture corrupted: " << active_texture << LL_ENDL;
		if (gDebugSession)
		{
			gFailLog << "Active texture corrupted: " << active_texture << std::endl;
		}
		error = TRUE;
	}*/

	static const char* label[] =
	{
		"GL_VERTEX_ARRAY",
		"GL_NORMAL_ARRAY",
		"GL_COLOR_ARRAY",
		"GL_TEXTURE_COORD_ARRAY"
	};

	static GLint value[] =
	{
		GL_VERTEX_ARRAY,
		GL_NORMAL_ARRAY,
		GL_COLOR_ARRAY,
		GL_TEXTURE_COORD_ARRAY
	};

	 U32 mask[] = 
	{ //copied from llvertexbuffer.h
		0x0001, //MAP_VERTEX,
		0x0002, //MAP_NORMAL,
		0x0010, //MAP_COLOR,
		0x0004, //MAP_TEXCOORD
	};


	for (S32 j = 1; j < 4; j++)
	{
		if (glIsEnabled(value[j]))
		{
			if (!(mask[j] & data_mask))
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL still has " << label[j] << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "GL still has " << label[j] << " enabled." << std::endl;
				}
			}
		}
		else
		{
			if (mask[j] & data_mask)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL does not have " << label[j] << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog << "GL does not have " << label[j] << " enabled." << std::endl;
				}
			}
		}
	}

	glClientActiveTextureARB(GL_TEXTURE1_ARB);
	gGL.getTexUnit(1)->activate();
	if (glIsEnabled(GL_TEXTURE_COORD_ARRAY))
	{
		if (!(data_mask & 0x0008))
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL still has GL_TEXTURE_COORD_ARRAY enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL still has GL_TEXTURE_COORD_ARRAY enabled on channel 1." << std::endl;
			}
		}
	}
	else
	{
		if (data_mask & 0x0008)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL does not have GL_TEXTURE_COORD_ARRAY enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL does not have GL_TEXTURE_COORD_ARRAY enabled on channel 1." << std::endl;
			}
		}
	}

	/*if (glIsEnabled(GL_TEXTURE_2D))
	{
		if (!(data_mask & 0x0008))
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL still has GL_TEXTURE_2D enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL still has GL_TEXTURE_2D enabled on channel 1." << std::endl;
			}
		}
	}
	else
	{
		if (data_mask & 0x0008)
		{
			error = TRUE;
			LL_WARNS("RenderState") << "GL does not have GL_TEXTURE_2D enabled on channel 1." << LL_ENDL;
			if (gDebugSession)
			{
				gFailLog << "GL does not have GL_TEXTURE_2D enabled on channel 1." << std::endl;
			}
		}
	}*/

	glClientActiveTextureARB(GL_TEXTURE0_ARB);
	gGL.getTexUnit(0)->activate();

	if (gGLManager.mHasShaderObjects && LLGLSLShader::sNoFixedFunction)
	{	//make sure vertex attribs are all disabled
		GLint count;
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &count);
		for (GLint i = 0; i < count; i++)
		{
			GLint enabled;
			glGetVertexAttribiv((GLuint) i, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &enabled);
			if (enabled)
			{
				error = TRUE;
				LL_WARNS("RenderState") << "GL still has vertex attrib array " << i << " enabled." << LL_ENDL;
				if (gDebugSession)
				{
					gFailLog <<  "GL still has vertex attrib array " << i << " enabled." << std::endl;
				}
			}
		}
	}

	if (error)
	{
		if (gDebugSession)
		{
			ll_fail("LLGLState::checkClientArrays failed.");
		}
		else
		{
			LL_GL_ERRS << "GL client array corruption detected.  " << msg << LL_ENDL;
		}
	}
}

///////////////////////////////////////////////////////////////////////

LLGLState::LLGLState(LLGLenum state, S32 enabled) :
	mState(state), mWasEnabled(FALSE), mIsEnabled(FALSE)
{
	if (LLGLSLShader::sNoFixedFunction)
	{ //always ignore state that's deprecated post GL 3.0
		switch (state)
		{
			case GL_ALPHA_TEST:
			case GL_NORMALIZE:
			case GL_TEXTURE_GEN_R:
			case GL_TEXTURE_GEN_S:
			case GL_TEXTURE_GEN_T:
			case GL_TEXTURE_GEN_Q:
			case GL_LIGHTING:
			case GL_COLOR_MATERIAL:
			case GL_FOG:
			case GL_LINE_STIPPLE:
			case GL_POLYGON_STIPPLE:
				mState = 0;
				break;
		}
	}

	stop_glerror();
	if (mState)
	{
		mWasEnabled = sStateMap[state];
		llassert(mWasEnabled == glIsEnabled(state));
		setEnabled(enabled);
		stop_glerror();
	}
}

void LLGLState::setEnabled(S32 enabled)
{
	if (!mState)
	{
		return;
	}
	if (enabled == CURRENT_STATE)
	{
		enabled = sStateMap[mState] == GL_TRUE ? TRUE : FALSE;
	}
	else if (enabled == TRUE && sStateMap[mState] != GL_TRUE)
	{
		gGL.flush();
		glEnable(mState);
		sStateMap[mState] = GL_TRUE;
	}
	else if (enabled == FALSE && sStateMap[mState] != GL_FALSE)
	{
		gGL.flush();
		glDisable(mState);
		sStateMap[mState] = GL_FALSE;
	}
	mIsEnabled = enabled;
}

LLGLState::~LLGLState() 
{
	stop_glerror();
	if (mState)
	{
		if (gDebugGL)
		{
			if (!gDebugSession)
			{
				llassert_always(sStateMap[mState] == glIsEnabled(mState));
			}
			else
			{
				if (sStateMap[mState] != glIsEnabled(mState))
				{
					ll_fail("GL enabled state does not match expected");
				}
			}
		}

		if (mIsEnabled != mWasEnabled)
		{
			gGL.flush();
			if (mWasEnabled)
			{
				glEnable(mState);
				sStateMap[mState] = GL_TRUE;
			}
			else
			{
				glDisable(mState);
				sStateMap[mState] = GL_FALSE;
			}
		}
	}
	stop_glerror();
}

////////////////////////////////////////////////////////////////////////////////

void LLGLManager::initGLStates()
{
	//gl states moved to classes in llglstates.h
	LLGLState::initClass();
}

////////////////////////////////////////////////////////////////////////////////

void parse_gl_version( S32* major, S32* minor, S32* release, std::string* vendor_specific, std::string* version_string )
{
	// GL_VERSION returns a null-terminated string with the format: 
	// <major>.<minor>[.<release>] [<vendor specific>]

	const char* version = (const char*) glGetString(GL_VERSION);
	*major = 0;
	*minor = 0;
	*release = 0;
	vendor_specific->assign("");

	if( !version )
	{
		return;
	}

	version_string->assign(version);

	std::string ver_copy( version );
	S32 len = (S32)strlen( version );	/* Flawfinder: ignore */
	S32 i = 0;
	S32 start;
	// Find the major version
	start = i;
	for( ; i < len; i++ )
	{
		if( '.' == version[i] )
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(major_str, *major);

	if( '.' == version[i] )
	{
		i++;
	}

	// Find the minor version
	start = i;
	for( ; i < len; i++ )
	{
		if( ('.' == version[i]) || isspace(version[i]) )
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(minor_str, *minor);

	// Find the release number (optional)
	if( '.' == version[i] )
	{
		i++;

		start = i;
		for( ; i < len; i++ )
		{
			if( isspace(version[i]) )
			{
				break;
			}
		}

		std::string release_str = ver_copy.substr(start,i-start);
		LLStringUtil::convertToS32(release_str, *release);
	}

	// Skip over any white space
	while( version[i] && isspace( version[i] ) )
	{
		i++;
	}

	// Copy the vendor-specific string (optional)
	if( version[i] )
	{
		vendor_specific->assign( version + i );
	}
}


void parse_glsl_version(S32& major, S32& minor)
{
	// GL_SHADING_LANGUAGE_VERSION returns a null-terminated string with the format: 
	// <major>.<minor>[.<release>] [<vendor specific>]

	const char* version = (const char*) glGetString(GL_SHADING_LANGUAGE_VERSION);
	major = 0;
	minor = 0;
	
	if( !version )
	{
		return;
	}

	std::string ver_copy( version );
	S32 len = (S32)strlen( version );	/* Flawfinder: ignore */
	S32 i = 0;
	S32 start;
	// Find the major version
	start = i;
	for( ; i < len; i++ )
	{
		if( '.' == version[i] )
		{
			break;
		}
	}
	std::string major_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(major_str, major);

	if( '.' == version[i] )
	{
		i++;
	}

	// Find the minor version
	start = i;
	for( ; i < len; i++ )
	{
		if( ('.' == version[i]) || isspace(version[i]) )
		{
			break;
		}
	}
	std::string minor_str = ver_copy.substr(start,i-start);
	LLStringUtil::convertToS32(minor_str, minor);
}

std::string parse_gl_ext_to_str(F32 glversion)
{
	std::string ret;
#if GL_VERSION_3_0
	if (glversion >= 3.0)
	{
		GLint n = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &n);
		GLint i;
		const char* ext;
		for (i = 0; i < n; ++i)
		{
			ext = (const char *) glGetStringi(GL_EXTENSIONS, i);
			ret.append(llformat("%s ", ext));
		}
	}
	else
#endif
	{
		const char* ext = (const char *) glGetString(GL_EXTENSIONS);
		ret.append(ext);
	}
	return ret;
}

LLGLUserClipPlane::LLGLUserClipPlane(const LLPlane& p, const glm::mat4& modelview, const glm::mat4& projection, bool apply)
{
	mApply = apply;

	if (mApply)
	{
		mModelview = modelview;
		mProjection = projection;

		setPlane(p[0], p[1], p[2], p[3]);
	}
}

void LLGLUserClipPlane::setPlane(F32 a, F32 b, F32 c, F32 d)
{
	const glm::mat4& P = mProjection;
	const glm::mat4& M = mModelview;
    
	glm::mat4 invtrans_MVP = glm::transpose(glm::inverse(P*M));
    glm::vec4 oplane(a, b, c, d);
    glm::vec4 cplane = invtrans_MVP * oplane;

    cplane /= fabs(cplane[2]); // normalize such that depth is not scaled
    cplane[3] -= 1;

    if (cplane[2] < 0)
        cplane *= -1;

    glm::mat4 suffix;
    suffix = glm::row(suffix, 2, cplane);
    glm::mat4 newP = suffix * P;
    gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
    gGL.loadMatrix(glm::value_ptr(newP));
	gGLObliqueProjectionInverse = LLMatrix4(glm::value_ptr(glm::transpose(glm::inverse(newP))));
    gGL.matrixMode(LLRender::MM_MODELVIEW);
}

LLGLUserClipPlane::~LLGLUserClipPlane()
{
	if (mApply)
	{
		gGL.matrixMode(LLRender::MM_PROJECTION);
		gGL.popMatrix();
		gGL.matrixMode(LLRender::MM_MODELVIEW);
	}
}

LLGLDepthTest::LLGLDepthTest(GLboolean depth_enabled, GLboolean write_enabled, GLenum depth_func)
: mPrevDepthFunc(sDepthFunc), mPrevDepthEnabled(sDepthEnabled), mPrevWriteEnabled(sWriteEnabled)
{
	stop_glerror();
	
	checkState();

	if (!depth_enabled)
	{ // always disable depth writes if depth testing is disabled
	  // GL spec defines this as a requirement, but some implementations allow depth writes with testing disabled
	  // The proper way to write to depth buffer with testing disabled is to enable testing and use a depth_func of GL_ALWAYS
		write_enabled = FALSE;
	}

	if (depth_enabled != sDepthEnabled)
	{
		gGL.flush();
		if (depth_enabled) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		sDepthEnabled = depth_enabled;
	}
	if (depth_func != sDepthFunc)
	{
		gGL.flush();
		glDepthFunc(depth_func);
		sDepthFunc = depth_func;
	}
	if (write_enabled != sWriteEnabled)
	{
		gGL.flush();
		glDepthMask(write_enabled);
		sWriteEnabled = write_enabled;
	}
}

LLGLDepthTest::~LLGLDepthTest()
{
	checkState();
	if (sDepthEnabled != mPrevDepthEnabled )
	{
		gGL.flush();
		if (mPrevDepthEnabled) glEnable(GL_DEPTH_TEST);
		else glDisable(GL_DEPTH_TEST);
		sDepthEnabled = mPrevDepthEnabled;
	}
	if (sDepthFunc != mPrevDepthFunc)
	{
		gGL.flush();
		glDepthFunc(mPrevDepthFunc);
		sDepthFunc = mPrevDepthFunc;
	}
	if (sWriteEnabled != mPrevWriteEnabled )
	{
		gGL.flush();
		glDepthMask(mPrevWriteEnabled);
		sWriteEnabled = mPrevWriteEnabled;
	}
}

void LLGLDepthTest::checkState()
{
	if (gDebugGL)
	{
		GLint func = 0;
		GLboolean mask = FALSE;

		glGetIntegerv(GL_DEPTH_FUNC, &func);
		glGetBooleanv(GL_DEPTH_WRITEMASK, &mask);

		if (glIsEnabled(GL_DEPTH_TEST) != sDepthEnabled ||
			sWriteEnabled != mask ||
			sDepthFunc != func)
		{
			if (gDebugSession)
			{
				gFailLog << "Unexpected depth testing state." << std::endl;
			}
			else
			{
				LL_GL_ERRS << "Unexpected depth testing state." << LL_ENDL;
			}
		}
	}
}

LLGLSquashToFarClip::LLGLSquashToFarClip(glm::mat4 P, U32 layer)
{

	F32 depth = 0.99999f - 0.0001f * layer;
	glm::vec4 P_row_3 = glm::row(P, 3) * depth;
	P = glm::row(P, 2, P_row_3);

	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.pushMatrix();
	gGL.loadMatrix(glm::value_ptr(P));
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}

LLGLSquashToFarClip::~LLGLSquashToFarClip()
{
	gGL.matrixMode(LLRender::MM_PROJECTION);
	gGL.popMatrix();
	gGL.matrixMode(LLRender::MM_MODELVIEW);
}


	
LLGLSyncFence::LLGLSyncFence()
{
#ifdef GL_ARB_sync
	mSync = nullptr;
#endif
}

LLGLSyncFence::~LLGLSyncFence()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		glDeleteSync(mSync);
	}
#endif
}

void LLGLSyncFence::placeFence()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		glDeleteSync(mSync);
	}
	mSync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
#endif
}

bool LLGLSyncFence::isCompleted()
{
	bool ret = true;
#ifdef GL_ARB_sync
	if (mSync)
	{
		GLenum status = glClientWaitSync(mSync, 0, 1);
		if (status == GL_TIMEOUT_EXPIRED)
		{
			ret = false;
		}
	}
#endif
	return ret;
}

void LLGLSyncFence::wait()
{
#ifdef GL_ARB_sync
	if (mSync)
	{
		while (glClientWaitSync(mSync, 0, FENCE_WAIT_TIME_NANOSECONDS) == GL_TIMEOUT_EXPIRED)
		{ //track the number of times we've waited here
			static S32 waits = 0;
			waits++;
		}
	}
#endif
}



