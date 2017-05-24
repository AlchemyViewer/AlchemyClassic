// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file   llheteromap.cpp
 * @author Nat Goodspeed
 * @date   2016-10-12
 * @brief  Implementation for llheteromap.
 * 
 * $LicenseInfo:firstyear=2016&license=viewerlgpl$
 * Copyright (c) 2016, Linden Research, Inc.
 * $/LicenseInfo$
 */

// Precompiled header
#include "linden_common.h"
// associated header
#include "llheteromap.h"
// STL headers
// std headers
// external library headers
// other Linden headers

LLHeteroMap::~LLHeteroMap()
{
    // For each entry in our map, we must call its deleter, which is the only
    // record we have of its original type.
    for (TypeMap::iterator mi(mMap.begin()), me(mMap.end()); mi != me; ++mi)
    {
        // mi->second is the std::pair; mi->second.first is the void*;
        // mi->second.second points to the deleter function
        (mi->second.second)(mi->second.first);
        mi->second.first = nullptr;
    }
}
