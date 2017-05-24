/** 
 * @file llxfer_vfile.h
 * @brief definition of LLXfer_VFile class for a single xfer_vfile.
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

#ifndef LL_LLXFER_VFILE_H
#define LL_LLXFER_VFILE_H

#include "llxfer.h"
#include "llassetstorage.h"

class LLVFS;
class LLVFile;

class LLXfer_VFile : public LLXfer
{
 protected:
	LLUUID mLocalID;
	LLUUID mRemoteID;
	LLUUID mTempID;
	LLAssetType::EType mType;
	
	LLVFile *mVFile;

	LLVFS *mVFS;

	std::string mName;

 public:
	LLXfer_VFile ();
	LLXfer_VFile (LLVFS *vfs, const LLUUID &local_id, LLAssetType::EType type);
	virtual ~LLXfer_VFile();

	virtual void init(LLVFS *vfs, const LLUUID &local_id, LLAssetType::EType type);
	void cleanup() override;

	virtual S32 initializeRequest(U64 xfer_id,
			LLVFS *vfs,
			const LLUUID &local_id,
			const LLUUID &remote_id,
			const LLAssetType::EType type,
			const LLHost &remote_host,
			void (*callback)(void **,S32,LLExtStat),
			void **user_data);
	S32 startDownload() override;

	S32 processEOF() override;

	S32 startSend (U64 xfer_id, const LLHost &remote_host) override;

	S32 suck(S32 start_position) override;
	S32 flush() override;

	virtual BOOL matchesLocalFile(const LLUUID &id, LLAssetType::EType type);
	virtual BOOL matchesRemoteFile(const LLUUID &id, LLAssetType::EType type);

	void setXferSize(S32 xfer_size) override;
	S32  getMaxBufferSize() override;

	U32 getXferTypeTag() override;

	std::string getFileName() override;
};

#endif





