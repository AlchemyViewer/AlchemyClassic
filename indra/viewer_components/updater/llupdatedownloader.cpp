/**
 * @file llupdatedownloader.cpp
 *
 * $LicenseInfo:firstyear=2010&license=viewerlgpl$
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

#include "llupdatedownloader.h"
#include "httpcommon.h"
#include "llexception.h"
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <curl/curl.h>
#include "lldir.h"
#include "llevents.h"
#include "llfile.h"
#include "llmd5.h"
#include "llsd.h"
#include "llsdserialize.h"
#include "llthread.h"
#include "llupdaterservice.h"

class LLUpdateDownloader::Implementation:
	public LLThread
{
public:
	Implementation(LLUpdateDownloader::Client & client);
	~Implementation();
	void cancel(void);
	void download(LLURI const & uri,
				  std::string const & hash,
				  std::string const & updateChannel,
				  std::string const & updateVersion,
				  std::string const & info_url,
				  bool required);
	bool isDownloading(void);
	size_t onHeader(void * header, size_t size);
	size_t onBody(void * header, size_t size);
	int onProgress(curl_off_t downloadSize, curl_off_t bytesDownloaded);
	void resume(void);
	void setBandwidthLimit(U64 bytesPerSecond);

private:
	curl_off_t mBandwidthLimit;
	bool mCancelled;
	LLUpdateDownloader::Client & mClient;
	LLCore::LLHttp::CURL_ptr mCurl;
	LLSD mDownloadData;
	llofstream mDownloadStream;
	unsigned char mDownloadPercent;
	std::string mDownloadRecordPath;
	curl_slist * mHeaderList;

	void initializeCurlGet(std::string const & url, bool processHeader);
	void resumeDownloading(size_t startByte);
	void run(void);
	void startDownloading(LLURI const & uri, std::string const & hash);
	void throwOnCurlError(CURLcode code);
	bool validateDownload(const std::string& filePath);
	bool validateOrRemove(const std::string& filePath);

	LOG_CLASS(LLUpdateDownloader::Implementation);
};


namespace {
	class DownloadError:
		public LLException
	{
	public:
		DownloadError(const char * message):
			LLException(message)
		{
			; // No op.
		}
	};


	const char * gSecondLifeUpdateRecord = "AlchemyUpdateDownload.xml";
};



// LLUpdateDownloader
//-----------------------------------------------------------------------------



std::string LLUpdateDownloader::downloadMarkerPath(void)
{
	return gDirUtilp->getExpandedFilename(LL_PATH_LOGS, gSecondLifeUpdateRecord);
}


LLUpdateDownloader::LLUpdateDownloader(Client & client):
	mImplementation(new LLUpdateDownloader::Implementation(client))
{
	; // No op.
}


void LLUpdateDownloader::cancel(void)
{
	mImplementation->cancel();
}


void LLUpdateDownloader::download(LLURI const & uri,
								  std::string const & hash,
								  std::string const & updateChannel,
								  std::string const & updateVersion,
								  std::string const & info_url,
								  bool required)
{
	mImplementation->download(uri, hash, updateChannel, updateVersion, info_url, required);
}


bool LLUpdateDownloader::isDownloading(void)
{
	return mImplementation->isDownloading();
}


void LLUpdateDownloader::resume(void)
{
	mImplementation->resume();
}


void LLUpdateDownloader::setBandwidthLimit(U64 bytesPerSecond)
{
	mImplementation->setBandwidthLimit(bytesPerSecond);
}



// LLUpdateDownloader::Implementation
//-----------------------------------------------------------------------------


namespace {
	size_t write_function(void * data, size_t blockSize, size_t blocks, void * downloader)
	{
		size_t bytes = blockSize * blocks;
		return reinterpret_cast<LLUpdateDownloader::Implementation *>(downloader)->onBody(data, bytes);
	}


	size_t header_function(void * data, size_t blockSize, size_t blocks, void * downloader)
	{
		size_t bytes = blockSize * blocks;
		return reinterpret_cast<LLUpdateDownloader::Implementation *>(downloader)->onHeader(data, bytes);
	}


	int xferinfo_callback(void * downloader,
						  curl_off_t dowloadTotal,
						  curl_off_t downloadNow,
						  curl_off_t uploadTotal,
						  curl_off_t uploadNow)
	{
		return reinterpret_cast<LLUpdateDownloader::Implementation *>(downloader)->
			onProgress(dowloadTotal, downloadNow);
	}
}


LLUpdateDownloader::Implementation::Implementation(LLUpdateDownloader::Client & client):
	LLThread("LLUpdateDownloader"),
	mBandwidthLimit(0),
	mCancelled(false),
	mClient(client),
	mCurl(),
	mDownloadPercent(0),
	mHeaderList(0)
{
	CURLcode code = curl_global_init(CURL_GLOBAL_ALL); // Just in case.
	llverify(code == CURLE_OK); // TODO: real error handling here.
}


LLUpdateDownloader::Implementation::~Implementation()
{
	if(isDownloading())
	{
		cancel();
		shutdown();
	}
	else
	{
		; // No op.
	}
    mCurl.reset();
}


void LLUpdateDownloader::Implementation::cancel(void)
{
	mCancelled = true;
}


void LLUpdateDownloader::Implementation::download(LLURI const & uri,
												  std::string const & hash,
												  std::string const & updateChannel,
												  std::string const & updateVersion,
												  std::string const & info_url,
												  bool required)
{ 
	if(isDownloading()) mClient.downloadError("download in progress");

	mDownloadRecordPath = downloadMarkerPath();
	mDownloadData = LLSD();
	mDownloadData["required"] = required;
	mDownloadData["update_channel"] = updateChannel;
	mDownloadData["update_version"] = updateVersion;
	if (!info_url.empty())
	{
		mDownloadData["info_url"] = info_url;
	}
	try
	{
		startDownloading(uri, hash);
	}
	catch(DownloadError const & e)
	{
		mClient.downloadError(e.what());
	}
}


bool LLUpdateDownloader::Implementation::isDownloading(void)
{
	return !isStopped();
}


void LLUpdateDownloader::Implementation::resume(void)
{
	mCancelled = false;

	if(isDownloading())
	{
		mClient.downloadError("download in progress");
	}

	mDownloadRecordPath = downloadMarkerPath();
	llifstream dataStream(mDownloadRecordPath.c_str());
	if(!dataStream)
	{
		mClient.downloadError("no download marker");
		return;
	}

	LLSDSerialize::fromXMLDocument(mDownloadData, dataStream);

	if(!mDownloadData.asBoolean())
	{
		mClient.downloadError("no download information in marker");
		return;
	}

	std::string filePath = mDownloadData["path"].asString();
	try
	{
		if(LLFile::isfile(filePath))
		{
			llstat fileStatus;
			LLFile::stat(filePath, &fileStatus);
			if(fileStatus.st_size != mDownloadData["size"].asInteger())
			{
				resumeDownloading(fileStatus.st_size);
			}
			else if(!validateOrRemove(filePath))
			{
				download(LLURI(mDownloadData["url"].asString()),
						 mDownloadData["hash"].asString(),
						 mDownloadData["update_channel"].asString(),
						 mDownloadData["update_version"].asString(),
						 mDownloadData["info_url"].asString(),
						 mDownloadData["required"].asBoolean());
			}
			else
			{
				mClient.downloadComplete(mDownloadData);
			}
		}
		else
		{
			download(LLURI(mDownloadData["url"].asString()),
					 mDownloadData["hash"].asString(),
					 mDownloadData["update_channel"].asString(),
					 mDownloadData["update_version"].asString(),
					 mDownloadData["info_url"].asString(),
					 mDownloadData["required"].asBoolean());
		}
	}
	catch(DownloadError & e)
	{
		mClient.downloadError(e.what());
	}
}


void LLUpdateDownloader::Implementation::setBandwidthLimit(U64 bytesPerSecond)
{
	if((mBandwidthLimit != bytesPerSecond) && isDownloading() && !mDownloadData["required"].asBoolean())
	{
		llassert(static_cast<bool>(mCurl));
		mBandwidthLimit = bytesPerSecond;
		CURLcode code = curl_easy_setopt(mCurl.get(), CURLOPT_MAX_RECV_SPEED_LARGE, &mBandwidthLimit);
		if(code != CURLE_OK)
		{
			LL_WARNS("UpdaterService") << "unable to change dowload bandwidth" << LL_ENDL;
		}
	}
	else
	{
		mBandwidthLimit = bytesPerSecond;
	}
}


size_t LLUpdateDownloader::Implementation::onHeader(void * buffer, size_t size)
{
	char const * headerPtr = reinterpret_cast<const char *> (buffer);
	std::string header(headerPtr, headerPtr + size);
	size_t colonPosition = header.find(':');
	if(colonPosition == std::string::npos) return size; // HTML response; ignore.

	if(header.substr(0, colonPosition) == "Content-Length") {
		try {
			size_t firstDigitPos = header.find_first_of("0123456789", colonPosition);
			size_t lastDigitPos = header.find_last_of("0123456789");
			std::string contentLength = header.substr(firstDigitPos, lastDigitPos - firstDigitPos + 1);
			size_t size = boost::lexical_cast<size_t>(contentLength);
			LL_INFOS("UpdaterService") << "download size is " << size << LL_ENDL;

			mDownloadData["size"] = LLSD(LLSD::Integer(size));
			llofstream odataStream(mDownloadRecordPath.c_str());
			LLSDSerialize::toPrettyXML(mDownloadData, odataStream);
		} catch (std::exception const & e) {
			LL_WARNS("UpdaterService") << "unable to read content length ("
				<< e.what() << ")" << LL_ENDL;
		}
	} else {
		; // No op.
	}

	return size;
}


size_t LLUpdateDownloader::Implementation::onBody(void * buffer, size_t size)
{
	if(mCancelled) return 0; // Forces a write error which will halt curl thread.
	if((size == 0) || (buffer == 0)) return 0;

	mDownloadStream.write(static_cast<const char *>(buffer), size);
	if(mDownloadStream.bad()) {
		return 0;
	} else {
		return size;
	}
}


int LLUpdateDownloader::Implementation::onProgress(curl_off_t downloadSize, curl_off_t bytesDownloaded)
{
	int downloadPercent = static_cast<int>(100.0 * ((double) bytesDownloaded / (double) downloadSize));
	if(downloadPercent > mDownloadPercent) {
		mDownloadPercent = downloadPercent;

		LLSD event;
		event["pump"] = LLUpdaterService::pumpName();
		LLSD payload;
		payload["type"] = LLSD(LLUpdaterService::PROGRESS);
		payload["download_size"] = (LLSD::Integer) downloadSize;
		payload["bytes_downloaded"] = (LLSD::Integer) bytesDownloaded;
		event["payload"] = payload;
		LLEventPumps::instance().obtain("mainlooprepeater").post(event);

		LL_INFOS("UpdaterService") << "progress event " << payload << LL_ENDL;
	} else {
		; // Keep events to a reasonalbe number.
	}

	return 0;
}


void LLUpdateDownloader::Implementation::run(void)
{
    CURLcode code = curl_easy_perform(mCurl.get());
	mDownloadStream.close();
	if(code == CURLE_OK)
	{
		LLFile::remove(mDownloadRecordPath);
		if(validateOrRemove(mDownloadData["path"]))
		{
			LL_INFOS("UpdaterService") << "download successful" << LL_ENDL;
			mClient.downloadComplete(mDownloadData);
		}
		else
		{
			mClient.downloadError("failed hash check");
		}
	}
	else if(mCancelled && (code == CURLE_WRITE_ERROR))
	{
		LL_INFOS("UpdaterService") << "download canceled by user" << LL_ENDL;
		// Do not call back client.
	}
	else
	{
		LL_WARNS("UpdaterService") << "download failed with error '" <<
			curl_easy_strerror(code) << "'" << LL_ENDL;
		LLFile::remove(mDownloadRecordPath);
		if(mDownloadData.has("path"))
		{
			std::string filePath = mDownloadData["path"].asString();
			LL_INFOS("UpdaterService") << "removing " << filePath << LL_ENDL;
			LLFile::remove(filePath);
		}
		mClient.downloadError("curl error");
	}

	if(mHeaderList)
	{
		curl_slist_free_all(mHeaderList);
		mHeaderList = 0;
	}
}


void LLUpdateDownloader::Implementation::initializeCurlGet(std::string const & url, bool processHeader)
{
	if(!mCurl)
	{
		mCurl = LLCore::LLHttp::createEasyHandle();
	}
	else
	{
        curl_easy_reset(mCurl.get());
	}

	if(!mCurl)
	{
		LLTHROW(DownloadError("failed to initialize curl"));
	}
    throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_NOSIGNAL, true));
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_FOLLOWLOCATION, true));
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_WRITEFUNCTION, &write_function));
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_WRITEDATA, this));
	if(processHeader)
	{
	   throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_HEADERFUNCTION, &header_function));
	   throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_HEADERDATA, this));
	}
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_HTTPGET, true));
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_URL, url.c_str()));
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_XFERINFOFUNCTION, &xferinfo_callback));
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_XFERINFODATA, this));
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_NOPROGRESS, 0));
	// if it's a required update set the bandwidth limit to 0 (unlimited)
	curl_off_t limit = mDownloadData["required"].asBoolean() ? 0 : mBandwidthLimit;
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_MAX_RECV_SPEED_LARGE, limit));
    throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_CAINFO, gDirUtilp->getCAFile().c_str()));
    throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_SSL_VERIFYHOST, 2));
    throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_SSL_VERIFYPEER, 1));

	mDownloadPercent = 0;
}


void LLUpdateDownloader::Implementation::resumeDownloading(size_t startByte)
{
	LL_INFOS("UpdaterService") << "resuming download from " << mDownloadData["url"].asString()
		<< " at byte " << startByte << LL_ENDL;

	initializeCurlGet(mDownloadData["url"].asString(), false);

	// The header 'Range: bytes n-' will request the bytes remaining in the
	// source begining with byte n and ending with the last byte.
	boost::format rangeHeaderFormat("Range: bytes=%u-");
	rangeHeaderFormat % startByte;
	mHeaderList = curl_slist_append(mHeaderList, rangeHeaderFormat.str().c_str());
	if(mHeaderList == 0)
	{
		LLTHROW(DownloadError("cannot add Range header"));
	}
	throwOnCurlError(curl_easy_setopt(mCurl.get(), CURLOPT_HTTPHEADER, mHeaderList));

	mDownloadStream.open(mDownloadData["path"].asString().c_str(),
						 std::ios_base::out | std::ios_base::binary | std::ios_base::app);
	start();
}


void LLUpdateDownloader::Implementation::startDownloading(LLURI const & uri, std::string const & hash)
{
	mDownloadData["url"] = uri.asString();
	mDownloadData["hash"] = hash;
	mDownloadData["current_version"] = ll_get_version();
	LLSD path = uri.pathArray();
	if(path.size() == 0) LLTHROW(DownloadError("no file path"));
	std::string fileName = path[path.size() - 1].asString();
	std::string filePath = gDirUtilp->getExpandedFilename(LL_PATH_TEMP, fileName);
	mDownloadData["path"] = filePath;

	LL_INFOS("UpdaterService") << "downloading " << filePath
		<< " from " << uri.asString() << LL_ENDL;
	LL_INFOS("UpdaterService") << "hash of file is " << hash << LL_ENDL;

	llofstream dataStream(mDownloadRecordPath.c_str());
	LLSDSerialize::toPrettyXML(mDownloadData, dataStream);

	mDownloadStream.open(filePath.c_str(), std::ios_base::out | std::ios_base::binary);
	initializeCurlGet(uri.asString(), true);
	start();
}


void LLUpdateDownloader::Implementation::throwOnCurlError(CURLcode code)
{
	if(code != CURLE_OK) {
		const char * errorString = curl_easy_strerror(code);
		if(errorString != 0) {
			LLTHROW(DownloadError(curl_easy_strerror(code)));
		} else {
			LLTHROW(DownloadError("unknown curl error"));
		}
	} else {
		; // No op.
	}
}

bool LLUpdateDownloader::Implementation::validateOrRemove(const std::string& filePath)
{
	bool valid = validateDownload(filePath);
	if (! valid)
	{
		LL_INFOS("UpdaterService") << "removing " << filePath << LL_ENDL;
		LLFile::remove(filePath);
	}
	return valid;
}

bool LLUpdateDownloader::Implementation::validateDownload(const std::string& filePath)
{
	llifstream fileStream(filePath.c_str(), std::ios_base::in | std::ios_base::binary);
	if(!fileStream)
	{
		LL_INFOS("UpdaterService") << "can't open " << filePath << ", invalid" << LL_ENDL;
		return false;
	}

	std::string hash = mDownloadData["hash"].asString();
	if (! hash.empty())
	{
		char digest[33];
		LLMD5(fileStream).hex_digest(digest);
		if (hash == digest)
		{
			LL_INFOS("UpdaterService") << "verified hash " << hash
									   << " for downloaded " << filePath << LL_ENDL;
			return true;
		}
		else
		{
			LL_WARNS("UpdaterService") << "download hash mismatch for "
									   << filePath << ": expected " << hash
									   << " but computed " << digest << LL_ENDL;
			return false;
		}
	}
	else
	{
		LL_INFOS("UpdaterService") << "no hash specified for " << filePath
								   << ", unverified" << LL_ENDL;
		return true; // No hash check provided.
	}
}
