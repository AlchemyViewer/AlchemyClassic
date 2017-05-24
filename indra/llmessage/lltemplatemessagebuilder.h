/** 
 * @file lltemplatemessagebuilder.h
 * @brief Declaration of LLTemplateMessageBuilder class.
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
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

#ifndef LL_LLTEMPLATEMESSAGEBUILDER_H
#define LL_LLTEMPLATEMESSAGEBUILDER_H

#include "llmessagebuilder.h"
#include "llmsgvariabletype.h"

class LLMsgData;
class LLMessageTemplate;
class LLMsgBlkData;
class LLMessageTemplate;

class LLTemplateMessageBuilder : public LLMessageBuilder
{
public:
	
	typedef std::map<const char* , LLMessageTemplate*> message_template_name_map_t;

	LLTemplateMessageBuilder(const message_template_name_map_t&);
	virtual ~LLTemplateMessageBuilder();

	void newMessage(const char* name) override;

	void nextBlock(const char* blockname) override;
	BOOL removeLastBlock() override; // TODO: babbage: remove this horror...

	/** All add* methods expect pointers to canonical varname strings. */
	void addBinaryData(const char *varname, const void *data, 
							   S32 size) override;
	void addBOOL(const char* varname, BOOL b) override;
	void addS8(const char* varname, S8 s) override;
	void addU8(const char* varname, U8 u) override;
	void addS16(const char* varname, S16 i) override;
	void addU16(const char* varname, U16 i) override;
	void addF32(const char* varname, F32 f) override;
	void addS32(const char* varname, S32 s) override;
	void addU32(const char* varname, U32 u) override;
	void addU64(const char* varname, U64 lu) override;
	void addF64(const char* varname, F64 d) override;
	void addVector3(const char* varname, const LLVector3& vec) override;
	void addVector4(const char* varname, const LLVector4& vec) override;
	void addVector3d(const char* varname, const LLVector3d& vec) override;
	void addQuat(const char* varname, const LLQuaternion& quat) override;
	void addUUID(const char* varname, const LLUUID& uuid) override;
	void addIPAddr(const char* varname, const U32 ip) override;
	void addIPPort(const char* varname, const U16 port) override;
	void addString(const char* varname, const char* s) override;
	void addString(const char* varname, const std::string& s) override;

	BOOL isMessageFull(const char* blockname) const override;
	void compressMessage(U8*& buf_ptr, U32& buffer_length) override;

	BOOL isBuilt() const override;
	BOOL isClear() const override;
	U32 buildMessage(U8* buffer, U32 buffer_size, U8 offset_to_data) override;
        /**< Return built message size */

	void clearMessage() override;

	// TODO: babbage: remove this horror.
	void setBuilt(BOOL b) override;

	S32 getMessageSize() override;
	const char* getMessageName() const override;

	void copyFromMessageData(const LLMsgData& data) override;
	void copyFromLLSD(const LLSD&) override;

	LLMsgData* getCurrentMessage() const { return mCurrentSMessageData; }
private:
	void addData(const char* varname, const void* data, 
					 EMsgVariableType type, S32 size);
	
	void addData(const char* varname, const void* data, 
						EMsgVariableType type);

	LLMsgData* mCurrentSMessageData;
	const LLMessageTemplate* mCurrentSMessageTemplate;
	LLMsgBlkData* mCurrentSDataBlock;
	char* mCurrentSMessageName;
	char* mCurrentSBlockName;
	BOOL mbSBuilt;
	BOOL mbSClear;
	S32	 mCurrentSendTotal;
	const message_template_name_map_t& mMessageTemplates;
};

#endif // LL_LLTEMPLATEMESSAGEBUILDER_H
