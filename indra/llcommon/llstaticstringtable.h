/** 
 * @file llstringtable.h
 * @brief The LLStringTable class provides a _fast_ method for finding
 * unique copies of strings.
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

#ifndef LL_STATIC_STRING_TABLE_H
#define LL_STATIC_STRING_TABLE_H

#include "lldefs.h"
#include "absl/hash/hash.h"
#include "absl/container/flat_hash_map.h"
#include "llstl.h"

class LLStaticHashedString
{
public:

	LLStaticHashedString(const std::string& s)
	{
		string_hash = makehash(s);
		string		= s;
	}

	const std::string&	String() const { return string;		}
	size_t				Hash()	 const { return string_hash;  }

	bool operator==(const LLStaticHashedString& b) const { return Hash() == b.Hash(); }

protected:

	size_t makehash(const std::string_view s)
	{
		return absl::Hash<std::string_view>{}(s);
	}

	std::string string;
	size_t		string_hash;
};

struct LLStaticStringHasher
{
	size_t operator()(const LLStaticHashedString& key_value) const { return key_value.Hash(); }
};

template< typename MappedObject >
using LLStaticStringTable = absl::flat_hash_map<LLStaticHashedString, MappedObject, LLStaticStringHasher>;

#endif

