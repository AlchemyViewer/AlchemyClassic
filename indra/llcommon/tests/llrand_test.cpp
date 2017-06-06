// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file llrandom_test.cpp
 * @author Drake Arconis, Phoenix
 * @date 2017-06-06
 *
 * $LicenseInfo:firstyear=2017&license=viewerlgpl$
 * Second Life Viewer Source Code
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

#include "../llrand.h"

#include <gtest/gtest.h>

TEST(LLRand, FloatRandomDefaultRange)
{
	F32 number = 0.0f;
	for (S32 ii = 0; ii < 100000; ++ii)
	{
		number = ll_frand();
		ASSERT_TRUE(number >= 0.0f);
		ASSERT_TRUE(number < 1.0f);
	}
}

TEST(LLRand, DoubleRandomDefaultRange)
{
	F64 number = 0.0f;
	for (S32 ii = 0; ii < 100000; ++ii)
	{
		number = ll_drand();
		ASSERT_TRUE(number >= 0.0);
		ASSERT_TRUE(number < 1.0);
	}
}

TEST(LLRand, NormalizedOneNegPos)
{
	F32 number = 0.0f;
	for (S32 ii = 0; ii < 100000; ++ii)
	{
		number = ll_frand(2.0f) - 1.0f;
		ASSERT_TRUE(number >= -1.0f);
		ASSERT_TRUE(number <= 1.0f);
	}
}

TEST(LLRand, NegativeFloat)
{
	F32 number = 0.0f;
	for (S32 ii = 0; ii < 100000; ++ii)
	{
		number = ll_frand(-7.0);
		ASSERT_TRUE(number <= 0.0);
		ASSERT_TRUE(number > -7.0);
	}
}

TEST(LLRand, NegativeDouble)
{
	F64 number = 0.0f;
	for (S32 ii = 0; ii < 100000; ++ii)
	{
		number = ll_drand(-2.0);
		ASSERT_TRUE(number <= 0.0);
		ASSERT_TRUE(number > -2.0);
	}
}

TEST(LLRand, PositiveInteger)
{
	S32 number = 0;
	for (S32 ii = 0; ii < 100000; ++ii)
	{
		number = ll_rand(100);
		ASSERT_TRUE(number >= 0);
		ASSERT_TRUE(number < 100);
	}
}

TEST(LLRand, NegativeInteger)
{
	S32 number = 0;
	for (S32 ii = 0; ii < 100000; ++ii)
	{
		number = ll_rand(-127);
		ASSERT_TRUE(number <= 0);
		ASSERT_TRUE(number > -127);
	}
}
