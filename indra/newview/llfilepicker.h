/** 
 * @file llfilepicker.h
 * @brief OS-specific file picker
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

// OS specific file selection dialog. This is implemented as a
// singleton class, so call the instance() method to get the working
// instance. When you call getMultipleOpenFile(), it locks the picker
// until you iterate to the end of the list of selected files with
// getNextFile() or call reset().

#ifndef LL_LLFILEPICKER_H
#define LL_LLFILEPICKER_H

#if LL_DARWIN
// AssertMacros.h does bad things.
#undef verify
#undef check
#undef require

#endif

// Need commdlg.h for OPENFILENAMEA
#ifdef LL_WINDOWS
#include "llwin32headers.h"
#include <commdlg.h>
#endif

extern "C" {
// mostly for Linux, possible on others
#if LL_GTK
# include "gtk/gtk.h"
#endif // LL_GTK
}

class LLFilePicker
{
#ifdef LL_GTK
	friend class LLDirPicker;
	friend void chooser_responder(GtkWidget *, gint, gpointer);
#endif // LL_GTK
public:
	// calling this before main() is undefined
	static LLFilePicker& instance( void ) { return sInstance; }

	enum ELoadFilter
	{
		FFLOAD_ALL = 1,
		FFLOAD_WAV = 2,
		FFLOAD_IMAGE = 3,
		FFLOAD_ANIM = 4,
		FFLOAD_XML = 5,
		FFLOAD_RAW = 6,
		FFLOAD_MODEL = 7,
		FFLOAD_COLLADA = 8,
		FFLOAD_FBX = 9,
		FFLOAD_SCRIPT = 10,
		FFLOAD_DICTIONARY = 11,
		FFLOAD_DIRECTORY = 12,   //To call from lldirpicker.
		FFLOAD_EXE = 13,
		FFLOAD_ZIP = 14
	};

	enum ESaveFilter
	{
		FFSAVE_ALL = 1,
		FFSAVE_WAV = 3,
		FFSAVE_TGA = 4,
		FFSAVE_BMP = 5,
		FFSAVE_AVI = 6,
		FFSAVE_ANIM = 7,
		FFSAVE_BVH = 8,
		FFSAVE_XML = 9,
		FFSAVE_CSV = 10,
		FFSAVE_COLLADA = 11,
		FFSAVE_FBX = 12,
		FFSAVE_RAW = 13,
		FFSAVE_J2C = 14,
		FFSAVE_PNG = 15,
		FFSAVE_JPEG = 16,
		FFSAVE_SCRIPT = 17,
		FFSAVE_TGAPNG = 18
	};

	// open the dialog. This is a modal operation
	BOOL getSaveFile( ESaveFilter filter = FFSAVE_ALL, const std::string& filename = LLStringUtil::null );
	BOOL getOpenFile( ELoadFilter filter = FFLOAD_ALL, bool blocking = true  );
	BOOL getMultipleOpenFiles( ELoadFilter filter = FFLOAD_ALL );

	// Get the filename(s) found. getFirstFile() sets the pointer to
	// the start of the structure and allows the start of iteration.
	const std::string getFirstFile();

	// getNextFile() increments the internal representation and
	// returns the next file specified by the user. Returns NULL when
	// no more files are left. Further calls to getNextFile() are
	// undefined.
	const std::string getNextFile();

	// This utility function extracts the current file name without
	// doing any incrementing.
	const std::string getCurFile();

	// Returns the index of the current file.
	S32 getCurFileNum() const { return mCurrentFile; }

	S32 getFileCount() const { return (S32)mFiles.size(); }

	// See llvfs/lldir.h : getBaseFileName and getDirName to extract base or directory names
	
	// clear any lists of buffers or whatever, and make sure the file
	// picker isn't locked.
	void reset();

private:
	enum
	{
		SINGLE_FILENAME_BUFFER_SIZE = 1024,
		//FILENAME_BUFFER_SIZE = 65536
		FILENAME_BUFFER_SIZE = 65000
	};

	// utility function to check if access to local file system via file browser 
	// is enabled and if not, tidy up and indicate we're not allowed to do this.
	bool check_local_file_access_enabled();
	
#if LL_WINDOWS
	OPENFILENAMEW mOFN;				// for open and save dialogs
	WCHAR mFilesW[FILENAME_BUFFER_SIZE];

	BOOL setupFilter(ELoadFilter filter);
#endif

#if LL_DARWIN
    S32 mPickOptions;
	std::vector<std::string> mFileVector;
	
	bool doNavChooseDialog(ELoadFilter filter);
	bool doNavSaveDialog(ESaveFilter filter, const std::string& filename);
    std::vector<std::string> navOpenFilterProc(ELoadFilter filter);
#endif

#if LL_GTK
	static void add_to_selectedfiles(gpointer data, gpointer user_data);
	static void chooser_responder(GtkWidget *widget, gint response, gpointer user_data);
	// we remember the last path that was accessed for a particular usage
	std::map <std::string, std::string> mContextToPathMap;
	std::string mCurContextName;
	// we also remember the extension of the last added file.
	std::string mCurrentExtension;
#endif

	std::vector<std::string> mFiles;
	S32 mCurrentFile;
	bool mLocked;

	static LLFilePicker sInstance;
	
protected:
#if LL_GTK
        GtkWindow* buildFilePicker(bool is_save, bool is_folder,
				   std::string context = "generic");
#endif

public:
	// don't call these directly please.
	LLFilePicker();
	~LLFilePicker();
};

const std::string upload_pick(void* data);

#endif
