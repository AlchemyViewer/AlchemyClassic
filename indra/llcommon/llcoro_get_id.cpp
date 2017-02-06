// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file   llcoro_get_id.cpp
 * @author Nat Goodspeed
 * @date   2016-09-03
 * @brief  Implementation for llcoro_get_id.
 * 
 * $LicenseInfo:firstyear=2016&license=viewerlgpl$
 * Copyright (c) 2016, Linden Research, Inc.
 * $/LicenseInfo$
 */

// Precompiled header
#include "linden_common.h"
// associated header
#include "llcoro_get_id.h"
// STL headers
// std headers
// external library headers
// other Linden headers
#include "llcoros.h"

namespace llcoro
{

id get_id()
{
    // An instance of Current can convert to LLCoros::CoroData*, which can
    // implicitly convert to void*, which is an llcoro::id.
    return LLCoros::Current();
}

} // llcoro
