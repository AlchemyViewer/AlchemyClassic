// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/** 
 * @file llsingleton_test.cpp
 * @date 2011-08-11
 * @brief Unit test for the LLSingleton class
 *
 * $LicenseInfo:firstyear=2011&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2011, Linden Research, Inc.
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

#include "llsingleton.h"
#include "../test/lltut.h"

namespace tut
{
	struct singleton
	{
		// We need a class created with the LLSingleton template to test with.
		class LLSingletonTest: public LLSingleton<LLSingletonTest>
		{

		};
	};

	typedef test_group<singleton> singleton_t;
	typedef singleton_t::object singleton_object_t;
	tut::singleton_t tut_singleton("LLSingleton");

	template<> template<>
	void singleton_object_t::test<1>()
	{

	}
	template<> template<>
	void singleton_object_t::test<2>()
	{
		LLSingletonTest* singleton_test = LLSingletonTest::getInstance();
		ensure(singleton_test);
	}
	template<> template<>
	void singleton_object_t::test<3>()
	{
		//Construct the instance
		LLSingletonTest::getInstance();
		ensure(LLSingletonTest::instanceExists());

		//Delete the instance
		LLSingletonTest::deleteSingleton();
		ensure(LLSingletonTest::destroyed());
		ensure(!LLSingletonTest::instanceExists());

		//Construct it again.
		LLSingletonTest* singleton_test = LLSingletonTest::getInstance();
		ensure(singleton_test);
		ensure(LLSingletonTest::instanceExists());
	}
}
