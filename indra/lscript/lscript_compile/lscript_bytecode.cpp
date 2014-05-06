/** 
 * @file lscript_bytecode.cpp
 * @brief classes to build actual bytecode
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

#include "linden_common.h"

#include "lscript_bytecode.h"
#include "lscript_error.h"

#if defined(_MSC_VER)
# pragma warning(disable: 4102) // 'yy_more' : unreferenced label
# pragma warning(disable: 4702) // unreachable code
#endif

LLScriptJumpTable::LLScriptJumpTable()
{
}

LLScriptJumpTable::~LLScriptJumpTable()
{
	delete_and_clear(mLabelMap);
	delete_and_clear(mJumpMap);
}

void LLScriptJumpTable::addLabel(char *name, S32 offset)
{
	char *temp = gScopeStringTable->addString(name);
	mLabelMap[temp] = new S32(offset);
}

void LLScriptJumpTable::addJump(char *name, S32 offset)
{
	char *temp = gScopeStringTable->addString(name);
	mJumpMap[temp] = new S32(offset);
}


LLScriptByteCodeChunk::LLScriptByteCodeChunk(BOOL b_need_jumps)
: mCodeChunk(NULL), mCurrentOffset(0), mJumpTable(NULL)
{
	if (b_need_jumps)
	{
		mJumpTable = new LLScriptJumpTable();
	}
}

LLScriptByteCodeChunk::~LLScriptByteCodeChunk()
{
	delete [] mCodeChunk;
	delete mJumpTable;
}

void LLScriptByteCodeChunk::addByte(U8 byte)
{
	if (mCodeChunk)
	{
		U8 *temp = new U8[mCurrentOffset + 1];
		memcpy(temp, mCodeChunk, mCurrentOffset);	/* Flawfinder: ignore */
		delete [] mCodeChunk;
		mCodeChunk = temp;
	}
	else
	{
		mCodeChunk = new U8[1];
	}
	*(mCodeChunk + mCurrentOffset++) = byte;
}

void LLScriptByteCodeChunk::addU16(U16 data)
{
	U8 temp[2];
	S32 offset = 0;
	u162bytestream(temp, offset, data);
	addBytes(temp, 2);
}

void LLScriptByteCodeChunk::addBytes(const U8 *bytes, S32 size)
{
	if (mCodeChunk)
	{
		U8 *temp = new U8[mCurrentOffset + size];
		memcpy(temp, mCodeChunk, mCurrentOffset);	/* Flawfinder: ignore */
		delete [] mCodeChunk;
		mCodeChunk = temp;
	}
	else
	{
		mCodeChunk = new U8[size];
	}
	memcpy(mCodeChunk + mCurrentOffset, bytes, size);/* Flawfinder: ignore */
	mCurrentOffset += size;
}

void LLScriptByteCodeChunk::addBytes(const char *bytes, S32 size)
{
	if (mCodeChunk)
	{
		U8 *temp = new U8[mCurrentOffset + size];
		memcpy(temp, mCodeChunk, mCurrentOffset);	 	/*Flawfinder: ignore*/
		delete [] mCodeChunk;
		mCodeChunk = temp;
	}
	else
	{
		mCodeChunk = new U8[size];
	}
	memcpy(mCodeChunk + mCurrentOffset, bytes, size);	/*Flawfinder: ignore*/
	mCurrentOffset += size;
}

void LLScriptByteCodeChunk::addBytes(S32 size)
{
	if (mCodeChunk)
	{
		U8 *temp = new U8[mCurrentOffset + size];
		memcpy(temp, mCodeChunk, mCurrentOffset);	/*Flawfinder: ignore*/
		delete [] mCodeChunk;
		mCodeChunk = temp;
	}
	else
	{
		mCodeChunk = new U8[size];
	}
	memset(mCodeChunk + mCurrentOffset, 0, size);
	mCurrentOffset += size;
}

void LLScriptByteCodeChunk::addBytesDontInc(S32 size)
{
	if (mCodeChunk)
	{
		U8 *temp = new U8[mCurrentOffset + size];
		memcpy(temp, mCodeChunk, mCurrentOffset);	 	/*Flawfinder: ignore*/
		delete [] mCodeChunk;
		mCodeChunk = temp;
	}
	else
	{
		mCodeChunk = new U8[size];
	}
	memset(mCodeChunk + mCurrentOffset, 0, size);
}

void LLScriptByteCodeChunk::addInteger(S32 value)
{
	U8 temp[4];
	S32 offset = 0;
	integer2bytestream(temp, offset, value);
	addBytes(temp, 4);
}

void LLScriptByteCodeChunk::addFloat(F32 value)
{
	U8 temp[4];
	S32 offset = 0;
	float2bytestream(temp, offset, value);
	addBytes(temp, 4);
}

void LLScriptByteCodeChunk::addLabel(char *name)
{
	if (mJumpTable)
	{
		mJumpTable->addLabel(name, mCurrentOffset);
	}
}

void LLScriptByteCodeChunk::addJump(char *name)
{
	if (mJumpTable)
	{
		mJumpTable->addJump(name, mCurrentOffset);
	}
}

// format is Byte 0: jump op code Byte 1 - 4: offset
// the jump position points to Byte 5, so we need to add the data at
//  offset - 4, offset - 3, offset - 2, and offset - 1

// offset is label - jump

void LLScriptByteCodeChunk::connectJumps()
{
	if (mJumpTable)
	{
		for(std::map<char *, S32 *>::iterator it = mJumpTable->mJumpMap.begin(), end_it = mJumpTable->mJumpMap.end();
			it != end_it;
			++it)
		{
			S32 jumppos = *it->second;
			S32 offset = *mJumpTable->mLabelMap[it->first] - jumppos;
			jumppos = jumppos - 4;
			integer2bytestream(mCodeChunk, jumppos, offset);
		}
	}
}

LLScriptScriptCodeChunk::LLScriptScriptCodeChunk(S32 total_size)
: mTotalSize(total_size), mCompleteCode(NULL)
{
	mRegisters = new LLScriptByteCodeChunk(FALSE);
	mGlobalVariables = new LLScriptByteCodeChunk(FALSE);
	mGlobalFunctions = new LLScriptByteCodeChunk(FALSE);
	mStates = new LLScriptByteCodeChunk(FALSE);
	mHeap = new LLScriptByteCodeChunk(FALSE);
}

LLScriptScriptCodeChunk::~LLScriptScriptCodeChunk()
{
	delete mRegisters;
	delete mGlobalVariables;
	delete mGlobalFunctions;
	delete mStates;
	delete mHeap;
	delete [] mCompleteCode;
}

void LLScriptScriptCodeChunk::build(LLFILE *efp, LLFILE *bcfp)
{
	S32 code_data_size = mRegisters->mCurrentOffset + 
					 mGlobalVariables->mCurrentOffset +
					 mGlobalFunctions->mCurrentOffset +
					 mStates->mCurrentOffset +
					 mHeap->mCurrentOffset;

	S32 offset = 0;

	if (code_data_size < mTotalSize)
	{
		mCompleteCode = new U8[mTotalSize];
		memset(mCompleteCode, 0, mTotalSize);
		
		memcpy(mCompleteCode, mRegisters->mCodeChunk, mRegisters->mCurrentOffset);	
		offset += mRegisters->mCurrentOffset;

		set_register(mCompleteCode, LREG_IP, 0);
		set_register(mCompleteCode, LREG_VN, LSL2_VERSION_NUMBER);
		set_event_register(mCompleteCode, LREG_IE, 0, LSL2_CURRENT_MAJOR_VERSION);
		set_register(mCompleteCode, LREG_BP, mTotalSize - 1);
		set_register(mCompleteCode, LREG_SP, mTotalSize - 1);

		set_register(mCompleteCode, LREG_GVR, offset);
		
		memcpy(mCompleteCode + offset, mGlobalVariables->mCodeChunk, mGlobalVariables->mCurrentOffset);	 	/*Flawfinder: ignore*/
		offset += mGlobalVariables->mCurrentOffset;

		set_register(mCompleteCode, LREG_GFR, offset);
		
		memcpy(mCompleteCode + offset, mGlobalFunctions->mCodeChunk, mGlobalFunctions->mCurrentOffset);	/*Flawfinder: ignore*/
		offset += mGlobalFunctions->mCurrentOffset;

		set_register(mCompleteCode, LREG_SR, offset);
		// zero is, by definition the default state
		set_register(mCompleteCode, LREG_CS, 0);
		set_register(mCompleteCode, LREG_NS, 0);
		set_event_register(mCompleteCode, LREG_CE, LSCRIPTStateBitField[LSTT_STATE_ENTRY], LSL2_CURRENT_MAJOR_VERSION);
		S32 default_state_offset = 0;
		if (LSL2_CURRENT_MAJOR_VERSION == LSL2_MAJOR_VERSION_TWO)
		{
			default_state_offset = 8;
		}
		else
		{
			default_state_offset = 4;
		}
		set_event_register(mCompleteCode, LREG_ER, bytestream2u64(mStates->mCodeChunk, default_state_offset), LSL2_CURRENT_MAJOR_VERSION);
		
		memcpy(mCompleteCode + offset, mStates->mCodeChunk, mStates->mCurrentOffset);	 	/*Flawfinder: ignore*/
		offset += mStates->mCurrentOffset;

		set_register(mCompleteCode, LREG_HR, offset);
		
		memcpy(mCompleteCode + offset, mHeap->mCodeChunk, mHeap->mCurrentOffset);	 	/*Flawfinder: ignore*/
		offset += mHeap->mCurrentOffset;
		
		set_register(mCompleteCode, LREG_HP, offset);
		set_register(mCompleteCode, LREG_FR, 0);
		set_register(mCompleteCode, LREG_SLR, 0);
		set_register(mCompleteCode, LREG_ESR, 0);
		set_register(mCompleteCode, LREG_PR, 0);
		set_register(mCompleteCode, LREG_TM, mTotalSize);


		if (fwrite(mCompleteCode, 1, mTotalSize, bcfp) != (size_t)mTotalSize)
		{
			LL_WARNS() << "Short write" << LL_ENDL;
		}
	}
	else
	{
		gErrorToText.writeError(efp, 0, 0, LSERROR_ASSEMBLE_OUT_OF_MEMORY);
	}
}

LLScriptScriptCodeChunk	*gScriptCodeChunk;
