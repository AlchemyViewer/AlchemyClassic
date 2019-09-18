/**
 * @file llfile.cpp
 * @author Michael Schlachter
 * @date 2006-03-23
 * @brief Implementation of cross-platform POSIX file buffer and c++
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

#if LL_WINDOWS
#include "llwin32headerslean.h"
#include <cstdlib>                 // Windows errno
#include <vector>
#else
#include <cerrno>
#include <sys/file.h>
#endif

#include "linden_common.h"
#include "llfile.h"
#include "llstring.h"
#include "llerror.h"
#include "stringize.h"


static std::string empty;

// Many of the methods below use OS-level functions that mess with errno. Wrap
// variants of strerror() to report errors.

#if LL_WINDOWS
// On Windows, use strerror_s().
std::string strerr(int errn)
{
	char buffer[256];
	strerror_s(buffer, errn);       // infers sizeof(buffer) -- love it!
	return buffer;
}

#else
// On Posix we want to call strerror_r(), but alarmingly, there are two
// different variants. The one that returns int always populates the passed
// buffer (except in case of error), whereas the other one always returns a
// valid char* but might or might not populate the passed buffer. How do we
// know which one we're getting? Define adapters for each and let the compiler
// select the applicable adapter.

// strerror_r() returns char*
std::string message_from(int /*orig_errno*/, const char* /*buffer*/, size_t /*bufflen*/,
						 const char* strerror_ret)
{
	return strerror_ret;
}

// strerror_r() returns int
std::string message_from(int orig_errno, const char* buffer, size_t bufflen,
						 int strerror_ret)
{
	if (strerror_ret == 0)
	{
		return buffer;
	}
	// Here strerror_r() has set errno. Since strerror_r() has already failed,
	// seems like a poor bet to call it again to diagnose its own error...
	int stre_errno = errno;
	if (stre_errno == ERANGE)
	{
		return STRINGIZE("strerror_r() can't explain errno " << orig_errno
						 << " (" << bufflen << "-byte buffer too small)");
	}
	if (stre_errno == EINVAL)
	{
		return STRINGIZE("unknown errno " << orig_errno);
	}
	// Here we don't even understand the errno from strerror_r()!
	return STRINGIZE("strerror_r() can't explain errno " << orig_errno
					 << " (error " << stre_errno << ')');
}

std::string strerr(int errn)
{
	char buffer[256];
	// Select message_from() function matching the strerror_r() we have on hand.
	return message_from(errn, buffer, sizeof(buffer),
						strerror_r(errn, buffer, sizeof(buffer)));
}
#endif	// ! LL_WINDOWS

// On either system, shorthand call just infers global 'errno'.
std::string strerr()
{
	return strerr(errno);
}

int warnif(const std::string& desc, const std::string& filename, int rc, int accept=0)
{
	if (rc < 0)
	{
		// Capture errno before we start emitting output
		int errn = errno;
		// For certain operations, a particular errno value might be
		// acceptable -- e.g. stat() could permit ENOENT, mkdir() could permit
		// EEXIST. Don't warn if caller explicitly says this errno is okay.
		if (errn != accept)
		{
			LL_WARNS("LLFile") << "Couldn't " << desc << " '" << filename
							   << "' (errno " << errn << "): " << strerr(errn) << LL_ENDL;
		}
#if 0 && LL_WINDOWS                 // turn on to debug file-locking problems
		// If the problem is "Permission denied," maybe it's because another
		// process has the file open. Try to find out.
		if (errn == EACCES)         // *not* EPERM
		{
			// Only do any of this stuff (before LL_ENDL) if it will be logged.
			LL_DEBUGS("LLFile") << empty;
			// would be nice to use LLDir for this, but dependency goes the
			// wrong way
			const char* TEMP = LLFile::tmpdir();
			if (! (TEMP && *TEMP))
			{
				LL_CONT << "No $TEMP, not running 'handle'";
			}
			else
			{
				std::string tf(TEMP);
				tf += "\\handle.tmp";
				// http://technet.microsoft.com/en-us/sysinternals/bb896655
				std::string cmd(STRINGIZE("handle \"" << filename
										  // "openfiles /query /v | fgrep -i \"" << filename
										  << "\" > \"" << tf << '"'));
				LL_CONT << cmd;
				if (system(cmd.c_str()) != 0)
				{
					LL_CONT << "\nDownload 'handle.exe' from http://technet.microsoft.com/en-us/sysinternals/bb896655";
				}
				else
				{
					llifstream inf(tf);
					std::string line;
					while (std::getline(inf, line))
					{
						LL_CONT << '\n' << line;
					}
				}
				LLFile::remove(tf);
			}
			LL_CONT << LL_ENDL;
		}
#endif  // LL_WINDOWS hack to identify processes holding file open
	}
	return rc;
}

// static
int	LLFile::mkdir(const std::string& dirname, int perms)
{
#if LL_WINDOWS
	// permissions are ignored on Windows
	int rc = _wmkdir(ll_convert_string_to_wide(dirname).c_str());
#else
	int rc = ::mkdir(dirname.c_str(), (mode_t)perms);
#endif
	// We often use mkdir() to ensure the existence of a directory that might
	// already exist. There is no known case in which we want to call out as
	// an error the requested directory already existing.
	if (rc < 0 && errno == EEXIST)
	{
		// this is not the error you want, move along
		return 0;
	}
	// anything else might be a problem
	return warnif("mkdir", dirname, rc, EEXIST);
}

// static
int	LLFile::rmdir(const std::string& dirname)
{
#if LL_WINDOWS
	// permissions are ignored on Windows
	int rc = _wrmdir(ll_convert_string_to_wide(dirname).c_str());
#else
	int rc = ::rmdir(dirname.c_str());
#endif
	return warnif("rmdir", dirname, rc);
}

// static
LLFILE*	LLFile::fopen(const std::string& filename, const char* mode)	/* Flawfinder: ignore */
{
#if	LL_WINDOWS
	LLFILE* filep = nullptr;
	auto ret = _wfopen_s(&filep, ll_convert_string_to_wide(filename).c_str(), ll_convert_string_to_wide(std::string(mode)).c_str());
	if (ret != 0)
		return nullptr;
	return filep;
#else
	return ::fopen(filename.c_str(),mode);	/* Flawfinder: ignore */
#endif
}

LLFILE*	LLFile::_fsopen(const std::string& filename, const char* mode, int sharingFlag)
{
#if	LL_WINDOWS
	return _wfsopen(ll_convert_string_to_wide(filename).c_str(), ll_convert_string_to_wide(std::string(mode)).c_str(),sharingFlag);
#else
	llassert(0);//No corresponding function on non-windows
	return NULL;
#endif
}

int	LLFile::close(LLFILE * file)
{
	int ret_value = 0;
	if (file)
	{
		ret_value = fclose(file);
	}
	return ret_value;
}


int	LLFile::remove(const std::string& filename, int supress_error)
{
#if	LL_WINDOWS
	int rc = _wremove(ll_convert_string_to_wide(filename).c_str());
#else
	int rc = ::remove(filename.c_str());
#endif
	return warnif("remove", filename, rc, supress_error);
}

int	LLFile::rename(const std::string& filename, const std::string& newname, int supress_error)
{
#if	LL_WINDOWS
	int rc = _wrename(ll_convert_string_to_wide(filename).c_str(), ll_convert_string_to_wide(newname).c_str());
#else
	int rc = ::rename(filename.c_str(),newname.c_str());
#endif
	return warnif(STRINGIZE("rename to '" << newname << "' from"), filename, rc, supress_error);
}

bool LLFile::copy(const std::string& from, const std::string& to)
{
	bool copied = false;
	LLFILE* in = LLFile::fopen(from, "rb");		/* Flawfinder: ignore */	 	
	if (in)	 	
	{	 	
		LLFILE* out = LLFile::fopen(to, "wb");		/* Flawfinder: ignore */
		if (out)
		{
			char buf[16384];		/* Flawfinder: ignore */ 	
			size_t readbytes;
			bool write_ok = true;
			while(write_ok && (readbytes = fread(buf, 1, 16384 * sizeof(char), in))) /* Flawfinder: ignore */
			{
				if (fwrite(buf, 1, readbytes, out) != readbytes)
				{
					LL_WARNS("LLFile") << "Short write" << LL_ENDL; 
					write_ok = false;
				}
			}
			if ( write_ok )
			{
				copied = true;
			}
			fclose(out);
		}
		fclose(in);
	}
	return copied;
}

int	LLFile::stat(const std::string& filename, llstat* filestatus)
{
#if LL_WINDOWS
	int rc = _wstat(ll_convert_string_to_wide(filename).c_str(),filestatus);
#else
	int rc = ::stat(filename.c_str(),filestatus);
#endif
	// We use stat() to determine existence (see isfile(), isdir()).
	// Don't spam the log if the subject pathname doesn't exist.
	return warnif("stat", filename, rc, ENOENT);
}

bool LLFile::isdir(const std::string& filename)
{
	llstat st;

	return stat(filename, &st) == 0 && S_ISDIR(st.st_mode);
}

bool LLFile::isfile(const std::string& filename)
{
	llstat st;

	return stat(filename, &st) == 0 && S_ISREG(st.st_mode);
}

const char *LLFile::tmpdir()
{
	static std::string utf8path;

	if (utf8path.empty())
	{
		char sep;
#if LL_WINDOWS
		sep = '\\';

		std::vector<wchar_t> utf16path(MAX_PATH + 1);
		GetTempPathW(utf16path.size(), &utf16path[0]);
		utf8path = ll_convert_wide_to_string(&utf16path[0]);
#else
		sep = '/';

		utf8path = LLStringUtil::getenv("TMPDIR", "/tmp/");
#endif
		if (utf8path[utf8path.size() - 1] != sep)
		{
			utf8path += sep;
		}
	}
	return utf8path.c_str();
}


/***************** Modified file stream created to overcome the incorrect behaviour of posix fopen in windows *******************/

#if LL_WINDOWS

LLFILE *	LLFile::_Fiopen(const std::string& filename, 
		std::ios::openmode mode)
{	// open a file
	static const char *mods[] =
	{	// fopen mode strings corresponding to valid[i]
	"r", "w", "w", "a", "rb", "wb", "wb", "ab",
	"r+", "w+", "a+", "r+b", "w+b", "a+b",
	nullptr};
	static const int valid[] =
	{	// valid combinations of open flags
		std::ios_base::in,
		std::ios_base::out,
		std::ios_base::out | std::ios_base::trunc,
		std::ios_base::out | std::ios_base::app,
		std::ios_base::in  | std::ios_base::binary,
		std::ios_base::out | std::ios_base::binary,
		std::ios_base::out | std::ios_base::trunc | std::ios_base::binary,
		std::ios_base::out | std::ios_base::app | std::ios_base::binary,
		std::ios_base::in  | std::ios_base::out,
		std::ios_base::in  | std::ios_base::out | std::ios_base::trunc,
		std::ios_base::in  | std::ios_base::out | std::ios_base::app,
		std::ios_base::in  | std::ios_base::out | std::ios_base::binary,
		std::ios_base::in  | std::ios_base::out | std::ios_base::trunc
			| std::ios_base::binary,
		std::ios_base::in | std::ios_base::out | std::ios_base::app
			| std::ios_base::binary,
	0};

	LLFILE *fp = nullptr;
	int n;
	std::ios_base::openmode atendflag = mode & std::ios_base::ate;
	std::ios_base::openmode norepflag = mode & std::ios_base::_Noreplace;

	if (mode & std::ios_base::_Nocreate)
		mode |= std::ios_base::in;	// file must exist
	mode &= ~(std::ios_base::ate | std::ios_base::_Nocreate | std::ios_base::_Noreplace);
	for (n = 0; valid[n] != 0 && valid[n] != mode; ++n)
		;	// look for a valid mode

	if (valid[n] == 0)
		return (nullptr);	// no valid mode
	else if (norepflag && mode & (std::ios_base::out | std::ios_base::app)
		&& (fp = LLFile::fopen(filename, "r")) != nullptr)	/* Flawfinder: ignore */
		{	// file must not exist, close and fail
		fclose(fp);
		return (nullptr);
		}
	else if (fp != nullptr && fclose(fp) != 0)
		return (nullptr);	// can't close after test open
// should open with protection here, if other than default
	else if ((fp = LLFile::fopen(filename, mods[n])) == nullptr)	/* Flawfinder: ignore */
		return (nullptr);	// open failed

	if (!atendflag || fseek(fp, 0, SEEK_END) == 0)
		return (fp);	// no need to seek to end, or seek succeeded

	fclose(fp);	// can't position at end
	return (nullptr);
}

#endif /* LL_WINDOWS */

