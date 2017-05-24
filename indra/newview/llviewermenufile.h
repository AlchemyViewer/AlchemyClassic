/** 
 * @file llviewermenufile.h
 * @brief "File" menu in the main menu bar.
 *
 * $LicenseInfo:firstyear=2002&license=viewerlgpl$
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

#ifndef LLVIEWERMENUFILE_H
#define LLVIEWERMENUFILE_H

#include "llfoldertype.h"
#include "llassetstorage.h"
#include "llinventorytype.h"
#include "llfilepicker.h"
#include "llthread.h"
#include <queue>

#include "llviewerassetupload.h"

class LLTransactionID;

const std::string BVHSTATUS[] =
{
    "E_ST_OK",
    "E_ST_EOF",
    "E_ST_NO_CONSTRAINT",
    "E_ST_NO_FILE",
    "E_ST_NO_HIER",
    "E_ST_NO_JOINT",
    "E_ST_NO_NAME",
    "E_ST_NO_OFFSET",
    "E_ST_NO_CHANNELS",
    "E_ST_NO_ROTATION",
    "E_ST_NO_AXIS",
    "E_ST_NO_MOTION",
    "E_ST_NO_FRAMES",
    "E_ST_NO_FRAME_TIME",
    "E_ST_NO_POS",
    "E_ST_NO_ROT",
    "E_ST_NO_XLT_FILE",
    "E_ST_NO_XLT_HEADER",
    "E_ST_NO_XLT_NAME",
    "E_ST_NO_XLT_IGNORE",
    "E_ST_NO_XLT_RELATIVE",
    "E_ST_NO_XLT_OUTNAME",
    "E_ST_NO_XLT_MATRIX",
    "E_ST_NO_XLT_MERGECHILD",
    "E_ST_NO_XLT_MERGEPARENT",
    "E_ST_NO_XLT_PRIORITY",
    "E_ST_NO_XLT_LOOP",
    "E_ST_NO_XLT_EASEIN",
    "E_ST_NO_XLT_EASEOUT",
    "E_ST_NO_XLT_HAND",
    "E_ST_NO_XLT_EMOTE",
    "E_ST_BAD_ROOT"
};

void init_menu_file();


LLUUID upload_new_resource(
    const std::string& src_filename,
    std::string name,
    std::string desc,
    S32 compression_info,
    LLFolderType::EType destination_folder_type,
    LLInventoryType::EType inv_type,
    U32 next_owner_perms,
    U32 group_perms,
    U32 everyone_perms,
    const std::string& display_name,
    LLAssetStorage::LLStoreAssetCallback callback,
    S32 expected_upload_cost,
    void *userdata);

void upload_new_resource(
    LLResourceUploadInfo::ptr_t &uploadInfo,
    LLAssetStorage::LLStoreAssetCallback callback = nullptr,
    void *userdata = nullptr);


void assign_defaults_and_show_upload_message(
	LLAssetType::EType asset_type,
	LLInventoryType::EType& inventory_type,
	std::string& name,
	const std::string& display_name,
	std::string& description);

class LLFilePickerThread : public LLThread
{ //multi-threaded file picker (runs system specific file picker in background and calls "notify" from main thread)
public:

	static std::queue<LLFilePickerThread*> sDeadQ;
	static LLMutex* sMutex;

	static void initClass();
	static void cleanupClass();
	static void clearDead();

	std::string mFile; 

	LLFilePicker::ELoadFilter mFilter;

	LLFilePickerThread(LLFilePicker::ELoadFilter filter)
		: LLThread("file picker"), mFilter(filter)
	{

	}

	void getFile();

	void run() override;

	virtual void notify(const std::string& filename) = 0;
};


#endif
