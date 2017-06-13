// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llxfermanager.cpp
 * @brief implementation of LLXferManager class for a collection of xfers
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

#include "llxfermanager.h"

#include "llxfer.h"
#include "llxfer_file.h"
#include "llxfer_mem.h"
#include "llxfer_vfile.h"

#include "llerror.h"
#include "lluuid.h"
#include "u64.h"

const F32 LL_XFER_REGISTRATION_TIMEOUT = 60.0f;  // timeout if a registered transfer hasn't been requested in 60 seconds
const F32 LL_PACKET_TIMEOUT = 3.0f;             // packet timeout at 3 s
const S32 LL_PACKET_RETRY_LIMIT = 10;            // packet retransmission limit

const S32 LL_DEFAULT_MAX_SIMULTANEOUS_XFERS = 10;
const S32 LL_DEFAULT_MAX_REQUEST_FIFO_XFERS = 1000;

#define LL_XFER_PROGRESS_MESSAGES 0
#define LL_XFER_TEST_REXMIT       0


///////////////////////////////////////////////////////////

LLXferManager::LLXferManager (LLVFS *vfs)
{
	init(vfs);
}

///////////////////////////////////////////////////////////

LLXferManager::~LLXferManager ()
{
	cleanup();
}

///////////////////////////////////////////////////////////

void LLXferManager::init (LLVFS *vfs)
{
	mSendList = nullptr;
	mReceiveList = nullptr;

	setMaxOutgoingXfersPerCircuit(LL_DEFAULT_MAX_SIMULTANEOUS_XFERS);
	setMaxIncomingXfers(LL_DEFAULT_MAX_REQUEST_FIFO_XFERS);

	mVFS = vfs;

	// Turn on or off ack throttling
	mUseAckThrottling = FALSE;
	setAckThrottleBPS(100000);
}
	
///////////////////////////////////////////////////////////

void LLXferManager::cleanup ()
{
	LLXfer *xferp;
	LLXfer *delp;

	for_each(mOutgoingHosts.begin(), mOutgoingHosts.end(), DeletePointer());
	mOutgoingHosts.clear();

	delp = mSendList;
	while (delp)
	{
		xferp = delp->mNext;
		delete delp;
		delp = xferp;
	}
	mSendList = nullptr;

	delp = mReceiveList;
	while (delp)
	{
		xferp = delp->mNext;
		delete delp;
		delp = xferp;
	}
	mReceiveList = nullptr;
}

///////////////////////////////////////////////////////////

void LLXferManager::setMaxIncomingXfers(S32 max_num)
{
	mMaxIncomingXfers = max_num;
}

///////////////////////////////////////////////////////////

void LLXferManager::setMaxOutgoingXfersPerCircuit(S32 max_num)
{
	mMaxOutgoingXfersPerCircuit = max_num;
}

void LLXferManager::setUseAckThrottling(const BOOL use)
{
	mUseAckThrottling = use;
}

void LLXferManager::setAckThrottleBPS(const F32 bps)
{
	// Let's figure out the min we can set based on the ack retry rate
	// and number of simultaneous.

	// Assuming we're running as slow as possible, this is the lowest ack
	// rate we can use.
	F32 min_bps = (1000.f * 8.f* mMaxIncomingXfers) / LL_PACKET_TIMEOUT;

	// Set
	F32 actual_rate = llmax(min_bps*1.1f, bps);
	LL_DEBUGS("AppInit") << "LLXferManager ack throttle min rate: " << min_bps << LL_ENDL;
	LL_DEBUGS("AppInit") << "LLXferManager ack throttle actual rate: " << actual_rate << LL_ENDL;
	mAckThrottle.setRate(actual_rate);
}


///////////////////////////////////////////////////////////

void LLXferManager::updateHostStatus()
{
    LLXfer *xferp;
	LLHostStatus *host_statusp = nullptr;

	for_each(mOutgoingHosts.begin(), mOutgoingHosts.end(), DeletePointer());
	mOutgoingHosts.clear();

	for (xferp = mSendList; xferp; xferp = xferp->mNext)
	{
		for (status_list_t::iterator iter = mOutgoingHosts.begin();
			 iter != mOutgoingHosts.end(); ++iter)
		{
			host_statusp = *iter;
			if (host_statusp->mHost == xferp->mRemoteHost)
			{
				break;
			}
		}
		if (!host_statusp)
		{
			host_statusp = new LLHostStatus();
			if (host_statusp)
			{
				host_statusp->mHost = xferp->mRemoteHost;
				mOutgoingHosts.push_front(host_statusp);
			}
		}
		if (host_statusp)
		{
			if (xferp->mStatus == e_LL_XFER_PENDING)
			{
				host_statusp->mNumPending++;
			}
			else if (xferp->mStatus == e_LL_XFER_IN_PROGRESS)
			{
				host_statusp->mNumActive++;
			}
		}
		
	}	
}

///////////////////////////////////////////////////////////

void LLXferManager::printHostStatus()
{
	LLHostStatus *host_statusp = nullptr;
	if (!mOutgoingHosts.empty())
	{
		LL_INFOS() << "Outgoing Xfers:" << LL_ENDL;

		for (status_list_t::iterator iter = mOutgoingHosts.begin();
			 iter != mOutgoingHosts.end(); ++iter)
		{
			host_statusp = *iter;
			LL_INFOS() << "    " << host_statusp->mHost << "  active: " << host_statusp->mNumActive << "  pending: " << host_statusp->mNumPending << LL_ENDL;
		}
	}	
}

///////////////////////////////////////////////////////////

LLXfer *LLXferManager::findXfer (U64 id, LLXfer *list_head)
{
    LLXfer *xferp;
	for (xferp = list_head; xferp; xferp = xferp->mNext)
	{
		if (xferp->mID == id)
		{
			return(xferp);
		}
	}
	return(nullptr);
}


///////////////////////////////////////////////////////////

void LLXferManager::removeXfer (LLXfer *delp, LLXfer **list_head)
{
	// This function assumes that delp will only occur in the list
	// zero or one times.
	if (delp)
	{
		if (*list_head == delp)
		{
			*list_head = delp->mNext;
			delete (delp);
		}
		else
		{
			LLXfer *xferp = *list_head;
			while (xferp->mNext)
			{
				if (xferp->mNext == delp)
				{
					xferp->mNext = delp->mNext;
					delete (delp);
					break;
				}
				xferp = xferp->mNext;
			}
		}
	}
}

///////////////////////////////////////////////////////////

U32 LLXferManager::numActiveListEntries(LLXfer *list_head)
{
	U32 num_entries = 0;

	while (list_head)
	{
		if (list_head->mStatus == e_LL_XFER_IN_PROGRESS) 
		{
			num_entries++;
		}
		list_head = list_head->mNext;
	}
	return(num_entries);
}

///////////////////////////////////////////////////////////

S32 LLXferManager::numPendingXfers(const LLHost &host)
{
	LLHostStatus *host_statusp = nullptr;

	for (status_list_t::iterator iter = mOutgoingHosts.begin();
		 iter != mOutgoingHosts.end(); ++iter)
	{
		host_statusp = *iter;
		if (host_statusp->mHost == host)
		{
			return (host_statusp->mNumPending);
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////

S32 LLXferManager::numActiveXfers(const LLHost &host)
{
	LLHostStatus *host_statusp = nullptr;

	for (status_list_t::iterator iter = mOutgoingHosts.begin();
		 iter != mOutgoingHosts.end(); ++iter)
	{
		host_statusp = *iter;
		if (host_statusp->mHost == host)
		{
			return (host_statusp->mNumActive);
		}
	}
	return 0;
}

///////////////////////////////////////////////////////////

void LLXferManager::changeNumActiveXfers(const LLHost &host, S32 delta)
{
	LLHostStatus *host_statusp = nullptr;

	for (status_list_t::iterator iter = mOutgoingHosts.begin();
		 iter != mOutgoingHosts.end(); ++iter)
	{
		host_statusp = *iter;
		if (host_statusp->mHost == host)
		{
			host_statusp->mNumActive += delta;
		}
	}
}

///////////////////////////////////////////////////////////

void LLXferManager::registerCallbacks(LLMessageSystem *msgsystem)
{
	msgsystem->setHandlerFuncFast(_PREHASH_ConfirmXferPacket,  process_confirm_packet, nullptr);
	msgsystem->setHandlerFuncFast(_PREHASH_RequestXfer,        process_request_xfer, nullptr);
	msgsystem->setHandlerFuncFast(_PREHASH_SendXferPacket,	   	continue_file_receive, nullptr);
	msgsystem->setHandlerFuncFast(_PREHASH_AbortXfer, 	   	process_abort_xfer, nullptr);
}

///////////////////////////////////////////////////////////

U64 LLXferManager::getNextID ()
{
	LLUUID a_guid;

	a_guid.generate();

	U64 out_tmp;
	memcpy(&out_tmp, a_guid.mData, sizeof(out_tmp));

	return out_tmp;
}

///////////////////////////////////////////////////////////

S32 LLXferManager::encodePacketNum(S32 packet_num, BOOL is_EOF)
{
	if (is_EOF)
	{
		packet_num |= 0x80000000;
	}
	return packet_num;
}

///////////////////////////////////////////////////////////

S32 LLXferManager::decodePacketNum(S32 packet_num)
{
	return(packet_num & 0x0FFFFFFF);
}

///////////////////////////////////////////////////////////

BOOL LLXferManager::isLastPacket(S32 packet_num)
{
	return(packet_num & 0x80000000);
}

///////////////////////////////////////////////////////////

U64 LLXferManager::registerXfer(const void *datap, const S32 length)
{
	LLXfer *xferp = nullptr;
	U64 xfer_id = getNextID();

	try
	{
		xferp = (LLXfer *) new LLXfer_Mem();
	}
	catch (const std::bad_alloc& e)
	{
		LL_ERRS() << "Xfer allcoation error: " << e.what() << LL_ENDL;
		xfer_id = 0;
		return(xfer_id);
	}

	xferp->mNext = mSendList;
	mSendList = xferp;

	xfer_id = ((LLXfer_Mem *) xferp)->registerXfer(xfer_id, datap, length);

	if (!xfer_id)
	{
		removeXfer(xferp, &mSendList);
	}

    return(xfer_id);
}

///////////////////////////////////////////////////////////

U64 LLXferManager::requestFile(const std::string& local_filename,
								const std::string& remote_filename,
								ELLPath remote_path,
								const LLHost& remote_host,
								BOOL delete_remote_on_completion,
								void (*callback)(void**,S32,LLExtStat),
								void** user_data,
								BOOL is_priority,
								BOOL use_big_packets)
{
	LLXfer *xferp = nullptr;

	for (xferp = mReceiveList; xferp ; xferp = xferp->mNext)
	{
		if (xferp->getXferTypeTag() == LLXfer::XFER_FILE
			&& (((LLXfer_File*)xferp)->matchesLocalFilename(local_filename))
			&& (((LLXfer_File*)xferp)->matchesRemoteFilename(remote_filename, remote_path))
			&& (remote_host == xferp->mRemoteHost)
			&& (callback == xferp->mCallback)
			&& (user_data == xferp->mCallbackDataHandle))

		{
			// cout << "requested a xfer already in progress" << endl;
			return xferp->mID;
		}
	}

	U64 xfer_id = 0;

	S32 chunk_size = use_big_packets ? LL_XFER_LARGE_PAYLOAD : -1;
	try
	{
		xferp = (LLXfer *) new LLXfer_File(chunk_size);
	}
	catch (const std::bad_alloc& e)
	{
		LL_ERRS() << "Xfer allcoation error: " << e.what() << LL_ENDL;
		return 0;
	}

	addToList(xferp, mReceiveList, is_priority);

	// Remove any file by the same name that happens to be lying
	// around.
	// Note: according to AaronB, this is here to deal with locks on files that were
	// in transit during a crash,
	if (delete_remote_on_completion
		&& (remote_filename.substr(remote_filename.length() - 4) == ".tmp")
		&& gDirUtilp->fileExists(local_filename))
	{
		LLFile::remove(local_filename, ENOENT);
	}
	xfer_id = getNextID();
	((LLXfer_File *) xferp)->initializeRequest(
		xfer_id,
		local_filename,
		remote_filename,
		remote_path,
		remote_host,
		delete_remote_on_completion,
		callback, user_data);
	startPendingDownloads();

	return xfer_id;
}

void LLXferManager::requestFile(const std::string& remote_filename,
								ELLPath remote_path,
								const LLHost& remote_host,
								BOOL delete_remote_on_completion,
								void (*callback)(void*,S32,void**,S32,LLExtStat),
								void** user_data,
								BOOL is_priority)
{
	LLXfer *xferp = nullptr;

	try
	{
		xferp = (LLXfer *) new LLXfer_Mem();
	}
	catch (const std::bad_alloc& e)
	{
		LL_ERRS() << "Xfer allcoation error: " << e.what() << LL_ENDL;
		return;
	}

	addToList(xferp, mReceiveList, is_priority);
	((LLXfer_Mem *) xferp)->initializeRequest(getNextID(),
		remote_filename,
		remote_path,
		remote_host,
		delete_remote_on_completion,
		callback, user_data);
	startPendingDownloads();
}

void LLXferManager::requestVFile(const LLUUID& local_id,
								 const LLUUID& remote_id,
								 LLAssetType::EType type, LLVFS* vfs,
								 const LLHost& remote_host,
								 void (*callback)(void**,S32,LLExtStat),
								 void** user_data,
								 BOOL is_priority)
{
	LLXfer *xferp = nullptr;

	for (xferp = mReceiveList; xferp ; xferp = xferp->mNext)
	{
		if (xferp->getXferTypeTag() == LLXfer::XFER_VFILE
			&& (((LLXfer_VFile*)xferp)->matchesLocalFile(local_id, type))
			&& (((LLXfer_VFile*)xferp)->matchesRemoteFile(remote_id, type))
			&& (remote_host == xferp->mRemoteHost)
			&& (callback == xferp->mCallback)
			&& (user_data == xferp->mCallbackDataHandle))

		{
			// cout << "requested a xfer already in progress" << endl;
			return;
		}
	}

	try
	{
		xferp = (LLXfer *) new LLXfer_VFile();
	}
	catch (const std::bad_alloc& e)
	{
		LL_ERRS() << "Xfer allcoation error: " << e.what() << LL_ENDL;
		return;
	}

	addToList(xferp, mReceiveList, is_priority);
	((LLXfer_VFile *) xferp)->initializeRequest(getNextID(),
		vfs,
		local_id,
		remote_id,
		type,
		remote_host,
		callback,
		user_data);
		startPendingDownloads();
}

/*
void LLXferManager::requestXfer(
								const std::string& local_filename, 
								BOOL delete_remote_on_completion,
								U64 xfer_id, 
								const LLHost &remote_host, 
								void (*callback)(void **,S32),
								void **user_data)
{
	LLXfer *xferp;

	for (xferp = mReceiveList; xferp ; xferp = xferp->mNext)
	{
		if (xferp->getXferTypeTag() == LLXfer::XFER_FILE
			&& (((LLXfer_File*)xferp)->matchesLocalFilename(local_filename))
			&& (xfer_id == xferp->mID)
			&& (remote_host == xferp->mRemoteHost)
			&& (callback == xferp->mCallback)
			&& (user_data == xferp->mCallbackDataHandle))

		{
			// cout << "requested a xfer already in progress" << endl;
			return;
		}
	}

	xferp = (LLXfer *) new LLXfer_File();
	if (xferp)
	{
		xferp->mNext = mReceiveList;
		mReceiveList = xferp;

		((LLXfer_File *)xferp)->initializeRequest(xfer_id,local_filename,"",LL_PATH_NONE,remote_host,delete_remote_on_completion,callback,user_data);
		startPendingDownloads();
	}
	else
	{
		LL_ERRS() << "Xfer allcoation error" << LL_ENDL;
	}
}

void LLXferManager::requestXfer(U64 xfer_id, const LLHost &remote_host, BOOL delete_remote_on_completion, void (*callback)(void *,S32,void **,S32),void **user_data)
{
	LLXfer *xferp;

	xferp = (LLXfer *) new LLXfer_Mem();
	if (xferp)
	{
		xferp->mNext = mReceiveList;
		mReceiveList = xferp;

		((LLXfer_Mem *)xferp)->initializeRequest(xfer_id,"",LL_PATH_NONE,remote_host,delete_remote_on_completion,callback,user_data);
		startPendingDownloads();
	}
	else
	{
		LL_ERRS() << "Xfer allcoation error" << LL_ENDL;
	}
}
*/
///////////////////////////////////////////////////////////

void LLXferManager::processReceiveData (LLMessageSystem *mesgsys, void ** /*user_data*/)
{
	// there's sometimes an extra 4 bytes added to an xfer payload
	const S32 BUF_SIZE = LL_XFER_LARGE_PAYLOAD + 4;
	char fdata_buf[LL_XFER_LARGE_PAYLOAD + 4];		/* Flawfinder : ignore */
	S32 fdata_size;
	U64 id;
	S32 packetnum;
	LLXfer * xferp;
	
	mesgsys->getU64Fast(_PREHASH_XferID, _PREHASH_ID, id);
	mesgsys->getS32Fast(_PREHASH_XferID, _PREHASH_Packet, packetnum);

	fdata_size = mesgsys->getSizeFast(_PREHASH_DataPacket,_PREHASH_Data);
	mesgsys->getBinaryDataFast(_PREHASH_DataPacket, _PREHASH_Data, fdata_buf, 0, 0, BUF_SIZE);

	xferp = findXfer(id, mReceiveList);

	if (!xferp) 
	{
		LL_INFOS() << "received xfer data from " << mesgsys->getSender()
			<< " for non-existent xfer id: "
			<< std::to_string(id) << LL_ENDL;
		return;
	}

	S32 xfer_size;

	if (decodePacketNum(packetnum) != xferp->mPacketNum) // is the packet different from what we were expecting?
	{
		// confirm it if it was a resend of the last one, since the confirmation might have gotten dropped
		if (decodePacketNum(packetnum) == (xferp->mPacketNum - 1))
		{
			LL_INFOS() << "Reconfirming xfer " << xferp->mRemoteHost << ":" << xferp->getFileName() << " packet " << packetnum << LL_ENDL; 			sendConfirmPacket(mesgsys, id, decodePacketNum(packetnum), mesgsys->getSender());
		}
		else
		{
			LL_INFOS() << "Ignoring xfer " << xferp->mRemoteHost << ":" << xferp->getFileName() << " recv'd packet " << packetnum << "; expecting " << xferp->mPacketNum << LL_ENDL;
		}
		return;		
	}

	S32 result = 0;

	if (xferp->mPacketNum == 0) // first packet has size encoded as additional S32 at beginning of data
	{
		ntohmemcpy(&xfer_size,fdata_buf,MVT_S32,sizeof(S32));
		
// do any necessary things on first packet ie. allocate memory
		xferp->setXferSize(xfer_size);

		// adjust buffer start and size
		result = xferp->receiveData(&(fdata_buf[sizeof(S32)]),fdata_size-(sizeof(S32)));
	}
	else
	{
		result = xferp->receiveData(fdata_buf,fdata_size);
	}
	
	if (result == LL_ERR_CANNOT_OPEN_FILE)
	{
			xferp->abort(LL_ERR_CANNOT_OPEN_FILE);
			removeXfer(xferp,&mReceiveList);
			startPendingDownloads();
			return;		
	}

	xferp->mPacketNum++;  // expect next packet

	if (!mUseAckThrottling)
	{
		// No throttling, confirm right away
		sendConfirmPacket(mesgsys, id, decodePacketNum(packetnum), mesgsys->getSender());
	}
	else
	{
		// Throttling, put on queue to be confirmed later.
		LLXferAckInfo ack_info;
		ack_info.mID = id;
		ack_info.mPacketNum = decodePacketNum(packetnum);
		ack_info.mRemoteHost = mesgsys->getSender();
		mXferAckQueue.push_back(ack_info);
	}

	if (isLastPacket(packetnum))
	{
		xferp->processEOF();
		removeXfer(xferp,&mReceiveList);
		startPendingDownloads();
	}
}

///////////////////////////////////////////////////////////

void LLXferManager::sendConfirmPacket (LLMessageSystem *mesgsys, U64 id, S32 packetnum, const LLHost &remote_host)
{
#if LL_XFER_PROGRESS_MESSAGES
	if (!(packetnum % 50))
	{
		cout << "confirming xfer packet #" << packetnum << endl;
	}
#endif
	mesgsys->newMessageFast(_PREHASH_ConfirmXferPacket);
	mesgsys->nextBlockFast(_PREHASH_XferID);
	mesgsys->addU64Fast(_PREHASH_ID, id);
	mesgsys->addU32Fast(_PREHASH_Packet, packetnum);

	mesgsys->sendMessage(remote_host);
}

///////////////////////////////////////////////////////////

static bool find_and_remove(std::multiset<std::string>& files,
		const std::string& filename)
{
	std::multiset<std::string>::iterator ptr;
	if ( (ptr = files.find(filename)) != files.end())
	{
		//erase(filename) erases *all* entries with that key
		files.erase(ptr);
		return true;
	}
	return false;
}

void LLXferManager::expectFileForRequest(const std::string& filename)
{
	mExpectedRequests.insert(filename);
}

bool LLXferManager::validateFileForRequest(const std::string& filename)
{
	return find_and_remove(mExpectedRequests, filename);
}

void LLXferManager::expectFileForTransfer(const std::string& filename)
{
	mExpectedTransfers.insert(filename);
}

bool LLXferManager::validateFileForTransfer(const std::string& filename)
{
	return find_and_remove(mExpectedTransfers, filename);
}

static bool remove_prefix(std::string& filename, const std::string& prefix)
{
	if (std::equal(prefix.begin(), prefix.end(), filename.begin()))
	{
		filename = filename.substr(prefix.length());
		return true;
	}
	return false;
}

static bool verify_cache_filename(const std::string& filename)
{
	//NOTE: This routine is only used to check file names that our own
	// code places in the cache directory.	As such, it can be limited
	// to this very restrictive file name pattern.	It does not need to
	// handle other characters. The only known uses of this are (with examples):
	//	sim to sim object pass:			fc0b72d8-9456-63d9-a802-a557ef847313.tmp
	//	sim to viewer mute list:		mute_b78eacd0-1244-448e-93ca-28ede242f647.tmp
	//	sim to viewer task inventory:	inventory_d8ab59d2-baf0-0e79-c4c2-a3f99b9fcf45.tmp
	
	//IMPORTANT: Do not broaden the filenames accepted by this routine
	// without careful analysis. Anything allowed by this function can
	// be downloaded by the viewer.
	
	size_t len = filename.size();
	//const boost::regex expr("[0-9a-zA-Z_-]<1,46>\.tmp");
	if (len < 5 || len > 50)
	{	
		return false;
	}
	for(size_t i=0; i<(len-4); ++i)
	{	
		char c = filename[i];
		bool ok = isalnum(c) || '_'==c || '-'==c;
		if (!ok)
		{
			return false;
		}
	}
	return filename[len-4] == '.'
		&& filename[len-3] == 't'
		&& filename[len-2] == 'm'
		&& filename[len-1] == 'p';
}

void LLXferManager::processFileRequest (LLMessageSystem *mesgsys, void ** /*user_data*/)
{
		
	U64 id;
	std::string local_filename;
	ELLPath local_path = LL_PATH_NONE;
	S32 result = LL_ERR_NOERR;
	LLUUID	uuid;
	LLAssetType::EType type;
	S16 type_s16;
	BOOL b_use_big_packets;

	mesgsys->getBOOL("XferID", "UseBigPackets", b_use_big_packets);
	
	mesgsys->getU64Fast(_PREHASH_XferID, _PREHASH_ID, id);
	LL_INFOS() << "xfer request id: " << std::to_string(id)
		   << " to " << mesgsys->getSender() << LL_ENDL;

	mesgsys->getStringFast(_PREHASH_XferID, _PREHASH_Filename, local_filename);
	
	{
		U8 local_path_u8;
		mesgsys->getU8("XferID", "FilePath", local_path_u8);
		local_path = (ELLPath)local_path_u8;
	}

	mesgsys->getUUIDFast(_PREHASH_XferID, _PREHASH_VFileID, uuid);
	mesgsys->getS16Fast(_PREHASH_XferID, _PREHASH_VFileType, type_s16);
	type = (LLAssetType::EType)type_s16;

	LLXfer *xferp = nullptr;

	if (uuid != LLUUID::null)
	{
		if(nullptr == LLAssetType::lookup(type))
		{
			LL_WARNS() << "Invalid type for xfer request: " << uuid << ":"
					<< type_s16 << " to " << mesgsys->getSender() << LL_ENDL;
			return;
		}
			
		LL_INFOS() << "starting vfile transfer: " << uuid << "," << LLAssetType::lookup(type) << " to " << mesgsys->getSender() << LL_ENDL;

		if (! mVFS)
		{
			LL_WARNS() << "Attempt to send VFile w/o available VFS" << LL_ENDL;
			return;
		}

		try
		{
			xferp = (LLXfer *)new LLXfer_VFile(mVFS, uuid, type);
		}
		catch (const std::bad_alloc& e)
		{
			LL_ERRS() << "Xfer allcoation error: " << e.what() << LL_ENDL;
			return;
		}

		xferp->mNext = mSendList;
		mSendList = xferp;	
		result = xferp->startSend(id,mesgsys->getSender());
	}
	else if (!local_filename.empty())
	{
		// See DEV-21775 for detailed security issues

		if (local_path == LL_PATH_NONE)
		{
			// this handles legacy simulators that are passing objects
			// by giving a filename that explicitly names the cache directory
			static const std::string legacy_cache_prefix = "data/";
			if (remove_prefix(local_filename, legacy_cache_prefix))
			{
				local_path = LL_PATH_CACHE;
			}
		}

		switch (local_path)
		{
			case LL_PATH_NONE:
				if(!validateFileForTransfer(local_filename))
				{
					LL_WARNS() << "SECURITY: Unapproved filename '" << local_filename << LL_ENDL;
					return;
				}
				break;

			case LL_PATH_CACHE:
				if(!verify_cache_filename(local_filename))
				{
					LL_WARNS() << "SECURITY: Illegal cache filename '" << local_filename << LL_ENDL;
					return;
				}
				break;

			default:
				LL_WARNS() << "SECURITY: Restricted file dir enum: " << (U32)local_path << LL_ENDL;
				return;
		}

		// If we want to use a special path (e.g. LL_PATH_CACHE), we want to make sure we create the
		// proper expanded filename.
		std::string expanded_filename;
		if (local_path != LL_PATH_NONE)
		{
			expanded_filename = gDirUtilp->getExpandedFilename( local_path, local_filename );
		}
		else
		{
			expanded_filename = local_filename;
		}
		LL_INFOS() << "starting file transfer: " <<  expanded_filename << " to " << mesgsys->getSender() << LL_ENDL;

		BOOL delete_local_on_completion = FALSE;
		mesgsys->getBOOL("XferID", "DeleteOnCompletion", delete_local_on_completion);

		// -1 chunk_size causes it to use the default
		try
		{
			xferp = (LLXfer *)new LLXfer_File(expanded_filename, delete_local_on_completion, b_use_big_packets ? LL_XFER_LARGE_PAYLOAD : -1);
		}
		catch (const std::bad_alloc& e)
		{
			LL_ERRS() << "Xfer allcoation error: " << e.what() << LL_ENDL;
			return;
		}

		xferp->mNext = mSendList;
		mSendList = xferp;	
		result = xferp->startSend(id,mesgsys->getSender());
	}
	else
	{
		char U64_BUF[MAX_STRING];		/* Flawfinder : ignore */
		LL_INFOS() << "starting memory transfer: "
			<< std::to_string(id) << " to "
			<< mesgsys->getSender() << LL_ENDL;

		xferp = findXfer(id, mSendList);
		
		if (xferp)
		{
			result = xferp->startSend(id,mesgsys->getSender());
		}
		else
		{
			LL_INFOS() << "Warning: " << U64_BUF << " not found." << LL_ENDL;
			result = LL_ERR_FILE_NOT_FOUND;
		}
	}

	if (result)
	{
		if (xferp)
		{
			xferp->abort(result);
			removeXfer(xferp,&mSendList);
		}
		else // can happen with a memory transfer not found
		{
			LL_INFOS() << "Aborting xfer to " << mesgsys->getSender() << " with error: " << result << LL_ENDL;

			mesgsys->newMessageFast(_PREHASH_AbortXfer);
			mesgsys->nextBlockFast(_PREHASH_XferID);
			mesgsys->addU64Fast(_PREHASH_ID, id);
			mesgsys->addS32Fast(_PREHASH_Result, result);
	
			mesgsys->sendMessage(mesgsys->getSender());		
		}
	}
	else if(xferp && (numActiveXfers(xferp->mRemoteHost) < mMaxOutgoingXfersPerCircuit))
	{
		xferp->sendNextPacket();
		changeNumActiveXfers(xferp->mRemoteHost,1);
//		LL_INFOS() << "***STARTING XFER IMMEDIATELY***" << LL_ENDL;
	}
	else
	{
		if(xferp)
		{
			LL_INFOS() << "  queueing xfer request, " << numPendingXfers(xferp->mRemoteHost) << " ahead of this one" << LL_ENDL;
		}
		else
		{
			LL_WARNS() << "LLXferManager::processFileRequest() - no xfer found!"
					<< LL_ENDL;
		}
	}
}


///////////////////////////////////////////////////////////

void LLXferManager::processConfirmation (LLMessageSystem *mesgsys, void ** /*user_data*/)
{
	U64 id = 0;
	S32 packetNum = 0;

	mesgsys->getU64Fast(_PREHASH_XferID, _PREHASH_ID, id);
	mesgsys->getS32Fast(_PREHASH_XferID, _PREHASH_Packet, packetNum);

	LLXfer* xferp = findXfer(id, mSendList);
	if (xferp)
	{
//		cout << "confirmed packet #" << packetNum << " ping: "<< xferp->ACKTimer.getElapsedTimeF32() <<  endl;
		xferp->mWaitingForACK = FALSE;
		if (xferp->mStatus == e_LL_XFER_IN_PROGRESS)
		{
			xferp->sendNextPacket();
		}
		else
		{
			removeXfer(xferp, &mSendList);
		}
	}
}

///////////////////////////////////////////////////////////

void LLXferManager::retransmitUnackedPackets ()
{
	LLXfer *xferp;
	LLXfer *delp;
	xferp = mReceiveList;
	while(xferp)
	{
		if (xferp->mStatus == e_LL_XFER_IN_PROGRESS)
		{
			// if the circuit dies, abort
			if (! gMessageSystem->mCircuitInfo.isCircuitAlive( xferp->mRemoteHost ))
			{
				LL_INFOS() << "Xfer found in progress on dead circuit, aborting" << LL_ENDL;
				xferp->mCallbackResult = LL_ERR_CIRCUIT_GONE;
				xferp->processEOF();
				delp = xferp;
				xferp = xferp->mNext;
				removeXfer(delp,&mReceiveList);
				continue;
 			}
				
		}
		xferp = xferp->mNext;
	}

	xferp = mSendList; 
	updateHostStatus();
	F32 et = 0.f;
	while (xferp)
	{
		if (xferp->mWaitingForACK && ( (et = xferp->ACKTimer.getElapsedTimeF32()) > LL_PACKET_TIMEOUT))
		{
			if (xferp->mRetries > LL_PACKET_RETRY_LIMIT)
			{
				LL_INFOS() << "dropping xfer " << xferp->mRemoteHost << ":" << xferp->getFileName() << " packet retransmit limit exceeded, xfer dropped" << LL_ENDL;
				xferp->abort(LL_ERR_TCP_TIMEOUT);
				delp = xferp;
				xferp = xferp->mNext;
				removeXfer(delp,&mSendList);
			}
			else
			{
				LL_INFOS() << "resending xfer " << xferp->mRemoteHost << ":" << xferp->getFileName() << " packet unconfirmed after: "<< et << " sec, packet " << xferp->mPacketNum << LL_ENDL;
				xferp->resendLastPacket();
				xferp = xferp->mNext;
			}
		}
		else if ((xferp->mStatus == e_LL_XFER_REGISTERED) && ( (et = xferp->ACKTimer.getElapsedTimeF32()) > LL_XFER_REGISTRATION_TIMEOUT))
		{
			LL_INFOS() << "registered xfer never requested, xfer dropped" << LL_ENDL;
			xferp->abort(LL_ERR_TCP_TIMEOUT);
			delp = xferp;
			xferp = xferp->mNext;
			removeXfer(delp,&mSendList);
		}
		else if (xferp->mStatus == e_LL_XFER_ABORTED)
		{
			LL_WARNS() << "Removing aborted xfer " << xferp->mRemoteHost << ":" << xferp->getFileName() << LL_ENDL;
			delp = xferp;
			xferp = xferp->mNext;
			removeXfer(delp,&mSendList);
		}
		else if (xferp->mStatus == e_LL_XFER_PENDING)
		{
//			LL_INFOS() << "*** numActiveXfers = " << numActiveXfers(xferp->mRemoteHost) << "        mMaxOutgoingXfersPerCircuit = " << mMaxOutgoingXfersPerCircuit << LL_ENDL;   
			if (numActiveXfers(xferp->mRemoteHost) < mMaxOutgoingXfersPerCircuit)
			{
//			    LL_INFOS() << "bumping pending xfer to active" << LL_ENDL;
				xferp->sendNextPacket();
				changeNumActiveXfers(xferp->mRemoteHost,1);
			}			
			xferp = xferp->mNext;
		}
		else
		{
			xferp = xferp->mNext;
		}
	}

	//
	// HACK - if we're using xfer confirm throttling, throttle our xfer confirms here
	// so we don't blow through bandwidth.
	//

	while (mXferAckQueue.size())
	{
		if (mAckThrottle.checkOverflow(1000.0f*8.0f))
		{
			break;
		}
		//LL_INFOS() << "Confirm packet queue length:" << mXferAckQueue.size() << LL_ENDL;
		LLXferAckInfo ack_info = mXferAckQueue.front();
		mXferAckQueue.pop_front();
		//LL_INFOS() << "Sending confirm packet" << LL_ENDL;
		sendConfirmPacket(gMessageSystem, ack_info.mID, ack_info.mPacketNum, ack_info.mRemoteHost);
		mAckThrottle.throttleOverflow(1000.f*8.f); // Assume 1000 bytes/packet
	}
}

///////////////////////////////////////////////////////////

void LLXferManager::abortRequestById(U64 xfer_id, S32 result_code)
{
	LLXfer * xferp = findXfer(xfer_id, mReceiveList);
	if (xferp)
	{
		if (xferp->mStatus == e_LL_XFER_IN_PROGRESS)
		{
			// causes processAbort();
			xferp->abort(result_code);
		}
		else
		{
			xferp->mCallbackResult = result_code;
			xferp->processEOF(); //should notify requester
			removeXfer(xferp, &mReceiveList);
		}
		// Since already removed or marked as aborted no need
		// to wait for processAbort() to start new download
		startPendingDownloads();
	}
}

///////////////////////////////////////////////////////////

void LLXferManager::processAbort (LLMessageSystem *mesgsys, void ** /*user_data*/)
{
	U64 id = 0;
	S32 result_code = 0;
	LLXfer * xferp;

	mesgsys->getU64Fast(_PREHASH_XferID, _PREHASH_ID, id);
	mesgsys->getS32Fast(_PREHASH_XferID, _PREHASH_Result, result_code);

	xferp = findXfer(id, mReceiveList);
	if (xferp)
	{
		xferp->mCallbackResult = result_code;
		xferp->processEOF();
		removeXfer(xferp, &mReceiveList);
		startPendingDownloads();
	}
}

///////////////////////////////////////////////////////////

void LLXferManager::startPendingDownloads()
{
	// This method goes through the list, and starts pending
	// operations until active downloads == mMaxIncomingXfers. I copy
	// the pending xfers into a temporary data structure because the
	// xfers are stored as an intrusive linked list where older
	// requests get pushed toward the back. Thus, if we didn't do a
	// stateful iteration, it would be possible for old requests to
	// never start.
	LLXfer* xferp = mReceiveList;
	std::list<LLXfer*> pending_downloads;
	S32 download_count = 0;
	S32 pending_count = 0;
	while(xferp)
	{
		if(xferp->mStatus == e_LL_XFER_PENDING)
		{
			++pending_count;
			pending_downloads.push_front(xferp);
		}
		else if(xferp->mStatus == e_LL_XFER_IN_PROGRESS)
		{
			++download_count;
		}
		xferp = xferp->mNext;
	}

	S32 start_count = mMaxIncomingXfers - download_count;

	LL_DEBUGS() << "LLXferManager::startPendingDownloads() - XFER_IN_PROGRESS: "
			 << download_count << " XFER_PENDING: " << pending_count
			 << " startring " << llmin(start_count, pending_count) << LL_ENDL;

	if((start_count > 0) && (pending_count > 0))
	{
		S32 result;
		for (std::list<LLXfer*>::iterator iter = pending_downloads.begin();
			 iter != pending_downloads.end(); ++iter)
		{
			xferp = *iter;
			if (start_count-- <= 0)
				break;
			result = xferp->startDownload();
			if(result)
			{
				xferp->abort(result);
				++start_count;
			}
		}
	}
}

///////////////////////////////////////////////////////////

void LLXferManager::addToList(LLXfer* xferp, LLXfer*& head, BOOL is_priority)
{
	if(is_priority)
	{
		xferp->mNext = nullptr;
		LLXfer* next = head;
		if(next)
		{
			while(next->mNext)
			{
				next = next->mNext;
			}
			next->mNext = xferp;
		}
		else
		{
			head = xferp;
		}
	}
	else
	{
		xferp->mNext = head;
		head = xferp;
	}
}

///////////////////////////////////////////////////////////
//  Globals and C routines
///////////////////////////////////////////////////////////

LLXferManager *gXferManager = nullptr;


void start_xfer_manager(LLVFS *vfs)
{
	gXferManager = new LLXferManager(vfs);
}

void cleanup_xfer_manager()
{
	if (gXferManager)
	{
		delete(gXferManager);
		gXferManager = nullptr;
	}
}

void process_confirm_packet (LLMessageSystem *mesgsys, void **user_data)
{
	gXferManager->processConfirmation(mesgsys,user_data);
}

void process_request_xfer(LLMessageSystem *mesgsys, void **user_data)
{
	gXferManager->processFileRequest(mesgsys,user_data);
}

void continue_file_receive(LLMessageSystem *mesgsys, void **user_data)
{
#if LL_TEST_XFER_REXMIT
	if (ll_frand() > 0.05f)
	{
#endif
		gXferManager->processReceiveData(mesgsys,user_data);
#if LL_TEST_XFER_REXMIT
	}
	else
	{
		cout << "oops! dropped a xfer packet" << endl;
	}
#endif
}

void process_abort_xfer(LLMessageSystem *mesgsys, void **user_data)
{
	gXferManager->processAbort(mesgsys,user_data);
}


























