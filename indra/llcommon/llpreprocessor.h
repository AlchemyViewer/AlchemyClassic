/** 
 * @file llpreprocessor.h
 * @brief This file should be included in all Linden Lab files and
 * should only contain special preprocessor directives
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

#ifndef LLPREPROCESSOR_H
#define LLPREPROCESSOR_H

#include "llcompilerfeatures.h"

// Figure out endianness of platform
#ifdef LL_LINUX
#define __ENABLE_WSTRING
#include <endian.h>
#endif	//	LL_LINUX

#if (defined(LL_WINDOWS) || (defined(LL_LINUX) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || (defined(LL_DARWIN) && defined(__LITTLE_ENDIAN__)))
#define LL_LITTLE_ENDIAN 1
#else
#define LL_BIG_ENDIAN 1
#endif

// Per-compiler switches
#ifdef __GNUC__
#define LL_FORCE_INLINE inline __attribute__((always_inline))
#else
#define LL_FORCE_INLINE __forceinline
#endif

// Mark-up expressions with branch prediction hints.  Do NOT use
// this with reckless abandon - it's an obfuscating micro-optimization
// outside of inner loops or other places where you are OVERWHELMINGLY
// sure which way an expression almost-always evaluates.
#if __GNUC__ >= 3
# define LL_LIKELY(EXPR) __builtin_expect (!!(EXPR), true)
# define LL_UNLIKELY(EXPR) __builtin_expect (!!(EXPR), false)
#else
# define LL_LIKELY(EXPR) (EXPR)
# define LL_UNLIKELY(EXPR) (EXPR)
#endif


// Figure out differences between compilers
#if defined(__clang__)
	#define CLANG_VERSION (__clang_major__ * 10000 \
						  + __clang_minor__ * 100 \
						  + __clang_patchlevel__)
	#ifndef LL_CLANG
		#define LL_CLANG 1
	#endif
#elif defined (__ICC)
	#ifndef LL_INTELC
		#define LL_INTELC 1
	#endif
#elif defined(__GNUC__)
	#define GCC_VERSION (__GNUC__ * 10000 \
						+ __GNUC_MINOR__ * 100 \
						+ __GNUC_PATCHLEVEL__)
	#ifndef LL_GNUC
		#define LL_GNUC 1
	#endif
#elif defined(__MSVC_VER__) || defined(_MSC_VER)
	#ifndef LL_MSVC
		#define LL_MSVC 1
	#endif
#endif

// Deal with minor differences on Unixy OSes.
#if LL_DARWIN || LL_LINUX
	// Different name, same functionality.
	#define stricmp strcasecmp
	#define strnicmp strncasecmp

	// Not sure why this is different, but...
	#ifndef MAX_PATH
		#define MAX_PATH PATH_MAX
	#endif	//	not MAX_PATH

#endif


// Static linking with apr on windows needs to be declared.
#if LL_WINDOWS && !LL_COMMON_LINK_SHARED
#ifndef APR_DECLARE_STATIC
#define APR_DECLARE_STATIC // For APR on Windows
#endif
#ifndef APU_DECLARE_STATIC
#define APU_DECLARE_STATIC // For APR util on Windows
#endif
#endif

#if defined(LL_WINDOWS)
#define BOOST_REGEX_NO_LIB 1
#define CURL_STATICLIB 1
#ifndef XML_STATIC
#define XML_STATIC
#endif
#endif	//	LL_WINDOWS

// Deal with VC6 problems
#if LL_MSVC
#pragma warning( 3	     : 4701 )	// "local variable used without being initialized"  Treat this as level 3, not level 4.
#pragma warning( 3	     : 4702 )	// "unreachable code"  Treat this as level 3, not level 4.
#pragma warning( 3	     : 4189 )	// "local variable initialized but not referenced"  Treat this as level 3, not level 4.
//#pragma warning( 3	: 4018 )	// "signed/unsigned mismatch"  Treat this as level 3, not level 4.
#pragma warning( 3      :  4263 )	// 'function' : member function does not override any base class virtual member function
#pragma warning( 3      :  4264 )	// "'virtual_function' : no override available for virtual member function from base 'class'; function is hidden"
//#pragma warning( 3       : 4265 )	// "class has virtual functions, but destructor is not virtual"
#pragma warning( 3      :  4266 )	// 'function' : no override available for virtual member function from base 'type'; function is hidden
#pragma warning (disable : 4180)	// qualifier applied to function type has no meaning; ignored
#pragma warning( disable : 4800 )	// 'BOOL' : forcing value to bool 'true' or 'false' (performance warning)
//#pragma warning( disable : 4996 )	// warning: deprecated

// Linker optimization with "extern template" generates these warnings
#pragma warning( disable : 4506 )   // no definition for inline function

// level 4 warnings that we need to disable:
#pragma warning (disable : 4100) // unreferenced formal parameter
#pragma warning (disable : 4127) // conditional expression is constant (e.g. while(1) )
#pragma warning (disable : 4244) // possible loss of data on conversions
#pragma warning (disable : 4396) // the inline specifier cannot be used when a friend declaration refers to a specialization of a function template
#pragma warning (disable : 4512) // assignment operator could not be generated
#pragma warning (disable : 4706) // assignment within conditional (even if((x = y)) )

#pragma warning (disable : 4251) // member needs to have dll-interface to be used by clients of class
#pragma warning (disable : 4275) // non dll-interface class used as base for dll-interface class
#pragma warning (disable : 4018) // '<' : signed/unsigned mismatch	

#if _WIN64
#pragma warning (disable : 4267) // 'var' : conversion from 'size_t' to 'type', possible loss of data
#endif

#endif	//	LL_MSVC

#if LL_WINDOWS
#define LL_DLLEXPORT __declspec(dllexport)
#define LL_DLLIMPORT __declspec(dllimport)
#elif LL_LINUX
#define LL_DLLEXPORT __attribute__ ((visibility("default")))
#define LL_DLLIMPORT
#else
#define LL_DLLEXPORT
#define LL_DLLIMPORT
#endif // LL_WINDOWS

#if LL_COMMON_LINK_SHARED
// CMake automagically defines llcommon_EXPORTS only when building llcommon
// sources, and only when llcommon is a shared library (i.e. when
// LL_COMMON_LINK_SHARED). We must still test LL_COMMON_LINK_SHARED because
// otherwise we can't distinguish between (non-llcommon source) and (llcommon
// not shared).
# if defined(llcommon_EXPORTS)
#   define LL_COMMON_API LL_DLLEXPORT
# else //llcommon_EXPORTS
#   define LL_COMMON_API LL_DLLIMPORT
# endif //llcommon_EXPORTS
#else // LL_COMMON_LINK_SHARED
# define LL_COMMON_API
#endif // LL_COMMON_LINK_SHARED

#define LL_TYPEOF(expr) decltype(expr)

#define LL_TO_STRING_HELPER(x) #x
#define LL_TO_STRING(x) LL_TO_STRING_HELPER(x)
#define LL_FILE_LINENO_MSG(msg) __FILE__ "(" LL_TO_STRING(__LINE__) ") : " msg
#define LL_GLUE_IMPL(x, y) x##y
#define LL_GLUE_TOKENS(x, y) LL_GLUE_IMPL(x, y)

#if LL_WINDOWS
#define LL_COMPILE_TIME_MESSAGE(msg) __pragma(message(LL_FILE_LINENO_MSG(msg)))
#else
// no way to get gcc 4.2 to print a user-defined diagnostic message only when a macro is used
#define LL_COMPILE_TIME_MESSAGE(msg)
#endif

#endif	//	not LL_LINDEN_PREPROCESSOR_H
