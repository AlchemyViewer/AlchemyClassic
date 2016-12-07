// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file lllfsthread.cpp
 * @brief LLLFSThread base class
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

#include "linden_common.h"
#include "lllfsthread.h"
#include "llstl.h"

//============================================================================

/*static*/ LLLFSThread* LLLFSThread::sLocal = NULL;

//============================================================================
// Run on MAIN thread
//static
void LLLFSThread::initClass(bool local_is_threaded)
{
	llassert(sLocal == NULL);
	sLocal = new LLLFSThread(local_is_threaded);
}

//static
S32 LLLFSThread::updateClass(U32 ms_elapsed)
{
	sLocal->update((F32)ms_elapsed);
	return sLocal->getPending();
}

//static
void LLLFSThread::cleanupClass()
{
	sLocal->setQuitting();
	while (sLocal->getPending())
	{
		sLocal->update(0);
	}
	delete sLocal;
	sLocal = 0;
}

//----------------------------------------------------------------------------

LLLFSThread::LLLFSThread(bool threaded) :
	LLQueuedThread("LFS", threaded),
	mPriorityCounter(PRIORITY_LOWBITS)
{
}

LLLFSThread::~LLLFSThread()
{
	// ~LLQueuedThread() will be called here
}

//----------------------------------------------------------------------------

LLLFSThread::handle_t LLLFSThread::read(const std::string& filename,	/* Flawfinder: ignore */ 
										U8* buffer, S32 offset, S32 numbytes,
										Responder* responder, U32 priority)
{
	handle_t handle = generateHandle();

	if (priority == 0) priority = PRIORITY_NORMAL | priorityCounter();
	else if (priority < PRIORITY_LOW) priority |= PRIORITY_LOW; // All reads are at least PRIORITY_LOW

	Request* req = new Request(this, handle, priority,
							   FILE_READ, filename,
							   buffer, offset, numbytes,
							   responder);

	bool res = addRequest(req);
	if (!res)
	{
		LL_ERRS() << "LLLFSThread::read called after LLLFSThread::cleanupClass()" << LL_ENDL;
	}

	return handle;
}

LLLFSThread::handle_t LLLFSThread::write(const std::string& filename,
										 U8* buffer, S32 offset, S32 numbytes,
										 Responder* responder, U32 priority)
{
	handle_t handle = generateHandle();

	if (priority == 0) priority = PRIORITY_LOW | priorityCounter();
	
	Request* req = new Request(this, handle, priority,
							   FILE_WRITE, filename,
							   buffer, offset, numbytes,
							   responder);

	bool res = addRequest(req);
	if (!res)
	{
		LL_ERRS() << "LLLFSThread::read called after LLLFSThread::cleanupClass()" << LL_ENDL;
	}
	
	return handle;
}

//============================================================================

LLLFSThread::Request::Request(LLLFSThread* thread,
							  handle_t handle, U32 priority,
							  operation_t op, const std::string& filename,
							  U8* buffer, S32 offset, S32 numbytes,
							  Responder* responder) :
	QueuedRequest(handle, priority, FLAG_AUTO_COMPLETE),
	//mThread(thread),
	mOperation(op),
	mFileName(filename),
	mBuffer(buffer),
	mOffset(offset),
	mBytes(numbytes),
	mBytesRead(0),
	mResponder(responder)
{
	if (numbytes <= 0)
	{
		LL_WARNS() << "LLLFSThread: Request with numbytes = " << numbytes << LL_ENDL;
	}
}

LLLFSThread::Request::~Request()
{
}

// virtual, called from own thread
void LLLFSThread::Request::finishRequest(bool completed)
{
	if (mResponder.notNull())
	{
		mResponder->completed(completed ? mBytesRead : 0);
		mResponder = NULL;
	}
}

void LLLFSThread::Request::deleteRequest()
{
	if (getStatus() == STATUS_QUEUED)
	{
		LL_ERRS() << "Attempt to delete a queued LLLFSThread::Request!" << LL_ENDL;
	}	
	if (mResponder.notNull())
	{
		mResponder->completed(0);
		mResponder = NULL;
	}
	LLQueuedThread::QueuedRequest::deleteRequest();
}

bool LLLFSThread::Request::processRequest()
{
	bool complete = false;
	if (mOperation ==  FILE_READ)
	{
		llassert(mOffset >= 0);
		llifstream instream(mFileName, std::ios::in | std::ios::binary);
		if (!instream.is_open())
		{
			LL_WARNS() << "LLLFS: Unable to read file: " << mFileName << LL_ENDL;
			mBytesRead = 0; // fail
			return true;
		}
		if (mOffset < 0)
			instream.seekg(0, std::ios::end);
		else
			instream.seekg(mOffset);
		llassert_always(!instream.good());
		instream.read((char*)mBuffer, mBytes );
		mBytesRead = instream.good() ? instream.gcount() : 0;
		complete = true;
// 		LL_INFOS() << "LLLFSThread::READ:" << mFileName << " Bytes: " << mBytesRead << LL_ENDL;
	}
	else if (mOperation ==  FILE_WRITE)
	{
		std::ios_base::openmode flags = std::ios::out | std::ios::binary;
		if (mOffset < 0)
			flags |= std::ios::app;
		else if (LLFile::isfile(mFileName))
			flags |= std::ios::in;

		llofstream outstream(mFileName, flags);
		if (!outstream.is_open())
		{
			LL_WARNS() << "LLLFS: Unable to write file: " << mFileName << LL_ENDL;
			mBytesRead = 0; // fail
			return true;
		}
		if (mOffset >= 0)
		{
			outstream.seekp(mOffset);
			if (!outstream.good())
			{
				LL_WARNS() << "LLLFS: Unable to write file (seek failed): " << mFileName << LL_ENDL;
				mBytesRead = 0; // fail
				return true;
			}
		}
		std::streampos before = outstream.tellp();
		outstream.write((char*) mBuffer, mBytes);
		mBytesRead = outstream.good() ? outstream.tellp() - before : 0;
		complete = true;
// 		LL_INFOS() << "LLLFSThread::WRITE:" << mFileName << " Bytes: " << mBytesRead << "/" << mBytes << " Offset:" << mOffset << LL_ENDL;
	}
	else
	{
		LL_ERRS() << "LLLFSThread::unknown operation: " << (S32)mOperation << LL_ENDL;
	}
	return complete;
}

//============================================================================

LLLFSThread::Responder::~Responder()
{
}

//============================================================================
