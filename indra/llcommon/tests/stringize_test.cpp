// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file   stringize_test.cpp
 * @author Nat Goodspeed
 * @date   2008-09-12
 * @brief  Test of stringize.h
 * 
 * $LicenseInfo:firstyear=2008&license=viewerlgpl$
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

/*==========================================================================*|
#if LL_WINDOWS
#pragma warning (disable : 4675) // "resolved by ADL" -- just as I want!
#endif
|*==========================================================================*/

// STL headers
#include <iomanip>

// Precompiled header
#include "linden_common.h"

// associated header
#include "../stringize.h"

// std headers
// external library headers
// other Linden headers
#include "../llsd.h"

#include "../test/lltut.h"

namespace tut
{
    struct stringize_data
    {
        stringize_data():
            c('c'),
            s(17),
            i(34),
            l(68),
            f(3.14159265358979f),
            d(3.14159265358979),
            // Including a space differentiates this from
            // boost::lexical_cast<std::string>, which doesn't handle embedded
            // spaces so well.
            abc("abc def")
        {
            llsd["i"]   = i;
            llsd["d"]   = d;
            llsd["abc"] = abc;
            def = L"def ghi";

        }

        char        c;
        short       s;
        int         i;
        long        l;
        float       f;
        double      d;
        std::string abc;
		std::wstring def;
        LLSD        llsd;
    };
    typedef test_group<stringize_data> stringize_group;
    typedef stringize_group::object stringize_object;
    tut::stringize_group strzgrp("stringize_h");

    template<> template<>
    void stringize_object::test<1>()
    {
        ensure_equals(STRINGIZE("c is " << c), "c is c");
        ensure_equals(STRINGIZE(std::setprecision(4) << d), "3.142");
    }
} // namespace tut
