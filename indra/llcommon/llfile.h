/** 
 * @file llfile.h
 * @author Michael Schlachter
 * @date 2006-03-23
 * @brief Declaration of cross-platform POSIX file buffer and c++
 * stream classes.
 *
 * $LicenseInfo:firstyear=2006&license=viewerlgpl$
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

#ifndef LL_LLFILE_H
#define LL_LLFILE_H

/**
 * This class provides a cross platform interface to the filesystem.
 * Attempts to mostly mirror the POSIX style IO functions.
 */

typedef FILE	LLFILE;

#include <fstream>
#include <sys/stat.h>

#if LL_WINDOWS
// windows version of stat function and stat data structure are called _stat
typedef struct _stat	llstat;
#else
typedef struct stat		llstat;
#endif

#ifndef S_ISREG
# define S_ISREG(x) (((x) & S_IFMT) == S_IFREG)
#endif

#ifndef S_ISDIR
# define S_ISDIR(x) (((x) & S_IFMT) == S_IFDIR)
#endif

#include "llstring.h" // safe char* -> std::string conversion

class LL_COMMON_API LLFile
{
public:
	// All these functions take UTF8 path/filenames.
	static	LLFILE*	fopen(const std::string& filename,const char* accessmode);	/* Flawfinder: ignore */
	static	LLFILE*	_fsopen(const std::string& filename,const char* accessmode,int	sharingFlag);

	static	int		close(LLFILE * file);

	// perms is a permissions mask like 0777 or 0700.  In most cases it will
	// be overridden by the user's umask.  It is ignored on Windows.
	static	int		mkdir(const std::string& filename, int perms = 0700);

	static	int		rmdir(const std::string& filename);
	static	int		remove(const std::string& filename, int supress_error = 0);
	static	int		rename(const std::string& filename,const std::string&	newname);
	static  bool	copy(const std::string from, const std::string to);

	static	int		stat(const std::string&	filename,llstat*	file_status);
	static	bool	isdir(const std::string&	filename);
	static	bool	isfile(const std::string&	filename);
	static	LLFILE *	_Fiopen(const std::string& filename, 
			std::ios::openmode mode);

	static  const char * tmpdir();
};

#if LL_WINDOWS
/**
*  @brief  Wrapper for UTF16 path compatibility on windows operating systems
*/
template< typename BaseType, std::ios_base::openmode DEFAULT_MODE>
class LL_COMMON_API stream_wrapper : public BaseType {
public:

	stream_wrapper()
		: BaseType()
	{}

	explicit stream_wrapper(char const * _Filename, std::ios_base::open_mode _Mode = DEFAULT_MODE)
		: BaseType(utf8str_to_utf16str(_Filename).c_str(), _Mode)
	{
	}

	explicit stream_wrapper(const std::string& _Filename, std::ios_base::open_mode _Mode = DEFAULT_MODE)
		: BaseType(utf8str_to_utf16str(_Filename), _Mode)
	{
	}

	void open(char const* _Filename, std::ios_base::open_mode _Mode = DEFAULT_MODE) {
		BaseType::open(utf8str_to_utf16str(_Filename).c_str(), _Mode);
	}

	void open(const std::string& _Filename, std::ios_base::open_mode _Mode = DEFAULT_MODE) {
		BaseType::open(utf8str_to_utf16str(_Filename), _Mode);
	}
};

typedef stream_wrapper<std::fstream, std::ios_base::in | std::ios_base::out > llfstream;
typedef stream_wrapper<std::ifstream, std::ios_base::in > llifstream;
typedef stream_wrapper<std::ofstream, std::ios_base::out > llofstream;


#else // ! LL_WINDOWS

// on non-windows, llifstream and llofstream are just mapped directly to the std:: equivalents
typedef std::fstream  llfstream;
typedef std::ifstream llifstream;
typedef std::ofstream llofstream;

#endif // LL_WINDOWS or !LL_WINDOWS

#endif // not LL_LLFILE_H
