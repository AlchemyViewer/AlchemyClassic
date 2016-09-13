/**
* @file llglmhelpers.h
* @brief Helper functions for glm
*
* $LicenseInfo:firstyear=2016&license=viewerlgpl$
* Copyright (C) 2016 Drake Arconis
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
* $/LicenseInfo$
*/

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

namespace llglmhelpers
{
	static inline glm::vec3 perspectiveTransform(const glm::mat4& mat, const glm::vec3& vec)
	{
		const float w = vec[0] * mat[0][3] + vec[1] * mat[1][3] + vec[2] * mat[2][3] + mat[3][3];
		return glm::vec3(
			(vec[0] * mat[0][0] + vec[1] * mat[1][0] + vec[2] * mat[2][0] + mat[3][0]) / w,
			(vec[0] * mat[0][1] + vec[1] * mat[1][1] + vec[2] * mat[2][1] + mat[3][1]) / w,
			(vec[0] * mat[0][2] + vec[1] * mat[1][2] + vec[2] * mat[2][2] + mat[3][2]) / w
			);
	}
}