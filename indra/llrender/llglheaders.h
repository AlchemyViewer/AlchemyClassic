/**
 * @file llglheaders.h
 * @brief LLGL definitions
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

#ifndef LL_LLGLHEADERS_H
#define LL_LLGLHEADERS_H

#if LL_MESA
//----------------------------------------------------------------------------
// MESA headers
// quotes so we get libraries/.../GL/ version
#define GL_GLEXT_PROTOTYPES 1
#include "GL/gl.h"
#include "GL/glext.h"
#include "GL/glu.h"

// The __APPLE__ kludge is to make glh_extensions.h not symbol-clash horribly
# define __APPLE__
# include "GL/glh_extensions.h"
# undef __APPLE__

#elif LL_LINUX
//----------------------------------------------------------------------------
// LL_LINUX

#define GLEW_STATIC 1
#include <GL/glew.h>
#include <GL/glxew.h>

#elif LL_WINDOWS
//----------------------------------------------------------------------------
// LL_WINDOWS
#define GLEW_STATIC 1
#include <GL/glew.h>
#include <GL/wglew.h>

#elif LL_DARWIN
//----------------------------------------------------------------------------
// LL_DARWIN

#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>

//
// Define multisample headers on Mac
//
#ifndef GL_ARB_texture_multisample
#define GL_SAMPLE_POSITION                0x8E50
#define GL_SAMPLE_MASK                    0x8E51
#define GL_SAMPLE_MASK_VALUE              0x8E52
#define GL_MAX_SAMPLE_MASK_WORDS          0x8E59
#define GL_TEXTURE_2D_MULTISAMPLE         0x9100
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE   0x9101
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY   0x9102
#define GL_PROXY_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9103
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE 0x9104
#define GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY 0x9105
#define GL_TEXTURE_SAMPLES                0x9106
#define GL_TEXTURE_FIXED_SAMPLE_LOCATIONS 0x9107
#define GL_SAMPLER_2D_MULTISAMPLE         0x9108
#define GL_INT_SAMPLER_2D_MULTISAMPLE     0x9109
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE 0x910A
#define GL_SAMPLER_2D_MULTISAMPLE_ARRAY   0x910B
#define GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910C
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910D
#define GL_MAX_COLOR_TEXTURE_SAMPLES      0x910E
#define GL_MAX_DEPTH_TEXTURE_SAMPLES      0x910F
#define GL_MAX_INTEGER_SAMPLES            0x9110
#endif

#endif // LL_MESA / LL_WINDOWS / LL_DARWIN

// Even when GL_ARB_depth_clamp is available in the driver, the (correct)
// headers, and therefore GL_DEPTH_CLAMP might not be defined.
// In that case GL_DEPTH_CLAMP_NV should be defined, but why not just
// use the known numeric.
//
// To avoid #ifdef's in the code. Just define this here.
#ifndef GL_DEPTH_CLAMP
// Probably (still) called GL_DEPTH_CLAMP_NV.
#define GL_DEPTH_CLAMP 0x864F
#endif

//GL_NVX_gpu_memory_info constants
#ifndef GL_NVX_gpu_memory_info
#define GL_NVX_gpu_memory_info
#define	GL_GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX          0x9047
#define	GL_GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX    0x9048
#define	GL_GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX  0x9049
#define	GL_GPU_MEMORY_INFO_EVICTION_COUNT_NVX            0x904A
#define	GL_GPU_MEMORY_INFO_EVICTED_MEMORY_NVX            0x904B
#endif

//GL_ATI_meminfo constants
#ifndef GL_ATI_meminfo
#define GL_ATI_meminfo
#define GL_VBO_FREE_MEMORY_ATI                     0x87FB
#define GL_TEXTURE_FREE_MEMORY_ATI                 0x87FC
#define GL_RENDERBUFFER_FREE_MEMORY_ATI            0x87FD
#endif

#endif // LL_LLGLHEADERS_H
