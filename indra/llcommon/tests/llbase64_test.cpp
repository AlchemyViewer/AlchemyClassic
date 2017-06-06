// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file llbase64_test.cpp
 * @author Drake Arconis, James Cook
 * @date 2017-05-29
 *
 * $LicenseInfo:firstyear=2017&license=viewerlgpl$
 * Alchemy Viewer Source Code
 * Copyright (C) 2010, Linden Research, Inc.
 * Copyright (C) 2017, Drake Arconis
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

#include <string>

#include "llbase64.h"
#include "lluuid.h"

#include <gtest/gtest.h>


TEST(LLBase64, encode_nothing)
{
	std::string result = LLBase64::encode(NULL, 0);
	EXPECT_EQ(LLStringUtil::null, result);
}

TEST(LLBase64, encode_blank_uuid)
{
	LLUUID nothing;
	std::string result = LLBase64::encode(&nothing.mData[0], UUID_BYTES);
	EXPECT_EQ(std::string("AAAAAAAAAAAAAAAAAAAAAA=="), result);
}

TEST(LLBase64, encode_random_uuid)
{
	LLUUID id("526a1e07-a19d-baed-84c4-ff08a488d15e");
	std::string result = LLBase64::encode(&id.mData[0], UUID_BYTES);
	EXPECT_EQ(std::string("UmoeB6Gduu2ExP8IpIjRXg=="), result);
}

TEST(LLBase64, encode_40_bytes)
{
	U8 blob[40] = { 115, 223, 172, 255, 140, 70, 49, 125, 236, 155, 45, 199, 101, 17, 164, 131, 230, 19, 80, 64, 112, 53, 135, 98, 237, 12, 26, 72, 126, 14, 145, 143, 118, 196, 11, 177, 132, 169, 195, 134 };
	auto result = LLBase64::encode(&blob[0], 40);
	EXPECT_EQ(std::string("c9+s/4xGMX3smy3HZRGkg+YTUEBwNYdi7QwaSH4OkY92xAuxhKnDhg=="), result);
}

TEST(LLBase64, encode_string)
{
	std::string result = LLBase64::encode(LLStringExplicit("Hands full of bananas."));
	EXPECT_EQ(std::string("SGFuZHMgZnVsbCBvZiBiYW5hbmFzLg=="), result);
}

TEST(LLBase64, decode_string)
{
	std::string result = LLBase64::decode(LLStringExplicit("SGFuZHMgZnVsbCBvZiBiYW5hbmFzLg=="));
	EXPECT_EQ(std::string("Hands full of bananas."), result);
}


