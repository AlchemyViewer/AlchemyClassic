// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
/**
 * @file llwlparamset.cpp
 * @brief Implementation for the LLWLParamSet class.
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
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

#include "llviewerprecompiledheaders.h"

#include "llwlparamset.h"
#include "llwlanimator.h"

#include "llwlparammanager.h"
#include "llglslshader.h"
#include "llsliderctrl.h"
#include "pipeline.h"

#include <llgl.h>

#include <sstream>

static LLStaticHashedString sStarBrightness("star_brightness");
static LLStaticHashedString sPresetNum("preset_num");
static LLStaticHashedString sSunAngle("sun_angle");
static LLStaticHashedString sEastAngle("east_angle");
static LLStaticHashedString sEnableCloudScroll("enable_cloud_scroll");
static LLStaticHashedString sCloudScrollRate("cloud_scroll_rate");
static LLStaticHashedString sLightNorm("lightnorm");
static LLStaticHashedString sCloudDensity("cloud_pos_density1");
static LLStaticHashedString sCloudScale("cloud_scale");
static LLStaticHashedString sCloudShadow("cloud_shadow");
static LLStaticHashedString sDensityMultiplier("density_multiplier");
static LLStaticHashedString sDistanceMultiplier("distance_multiplier");
static LLStaticHashedString sHazeDensity("haze_density");
static LLStaticHashedString sHazeHorizon("haze_horizon");
static LLStaticHashedString sMaxY("max_y");

LLWLParamSet::LLWLParamSet(void) :
	mName("Unnamed Preset"),
	mCloudScrollXOffset(0.f), mCloudScrollYOffset(0.f)	
{}

static LLTrace::BlockTimerStatHandle FTM_WL_PARAM_UPDATE("WL Param Update");

void LLWLParamSet::update(LLGLSLShader * shader) const 
{	
	LL_RECORD_BLOCK_TIME(FTM_WL_PARAM_UPDATE);
	LLSD::map_const_iterator i = mParamValues.beginMap();
	std::vector<LLStaticHashedString>::const_iterator n = mParamHashedNames.begin();
	for(;(i != mParamValues.endMap()) && (n != mParamHashedNames.end());++i, n++)
	{
		const LLStaticHashedString& param = *n;
		
		// check that our pre-hashed names are still tracking the mParamValues map correctly
		//
		llassert(param.String() == i->first);

		if (param == sStarBrightness || param == sPresetNum || param == sSunAngle ||
			param == sEastAngle || param == sEnableCloudScroll ||
			param == sCloudScrollRate || param == sLightNorm ) 
		{
			continue;
		}
		
		if (param == sCloudDensity)
		{
			LLVector4 val;
			val.mV[0] = F32(i->second[0].asReal()) + mCloudScrollXOffset;
			val.mV[1] = F32(i->second[1].asReal()) + mCloudScrollYOffset;
			val.mV[2] = (F32) i->second[2].asReal();
			val.mV[3] = (F32) i->second[3].asReal();

			stop_glerror();
			shader->uniform4fv(param, 1, val.mV);
			stop_glerror();
		}
		else if (param == sCloudScale || param == sCloudShadow ||
				 param == sDensityMultiplier || param == sDistanceMultiplier ||
				 param == sHazeDensity || param == sHazeHorizon ||
				 param == sMaxY )
		{
			F32 val = (F32) i->second[0].asReal();

			stop_glerror();
			shader->uniform1f(param, val);
			stop_glerror();
		}
		else // param is the uniform name
		{
			// handle all the different cases
			if (i->second.isArray() && i->second.size() == 4)
			{
				LLVector4 val;

				val.mV[0] = (F32) i->second[0].asReal();
				val.mV[1] = (F32) i->second[1].asReal();
				val.mV[2] = (F32) i->second[2].asReal();
				val.mV[3] = (F32) i->second[3].asReal();															

				stop_glerror();
				shader->uniform4fv(param, 1, val.mV);
				stop_glerror();
			} 
			else if (i->second.isReal())
			{
				F32 val = (F32) i->second.asReal();

				stop_glerror();
				shader->uniform1f(param, val);
				stop_glerror();
			} 
			else if (i->second.isInteger())
			{
				S32 val = (S32) i->second.asInteger();

				stop_glerror();
				shader->uniform1i(param, val);
				stop_glerror();
			} 
			else if (i->second.isBoolean())
			{
				S32 val = (i->second.asBoolean() ? 1 : 0);

				stop_glerror();
				shader->uniform1i(param, val);
				stop_glerror();
			}
		}
	}
	
	if (LLPipeline::sRenderDeferred && !LLPipeline::sReflectionRender && !LLPipeline::sUnderWaterRender)
	{
		shader->uniform1f(LLShaderMgr::GLOBAL_GAMMA, 2.2f); // <alchemy/>
	} else {
		shader->uniform1f(LLShaderMgr::GLOBAL_GAMMA, 1.0f); // <alchemy/>
	}
}

void LLWLParamSet::set(const std::string& paramName, float x) 
{	
	// handle case where no array
	if(mParamValues[paramName].isReal()) 
	{
		mParamValues[paramName] = x;
	} 
	
	// handle array
	else if(mParamValues[paramName].isArray() &&
			mParamValues[paramName][0].isReal())
	{
		mParamValues[paramName][0] = x;
	}
}

void LLWLParamSet::set(const std::string& paramName, float x, float y)
{
	mParamValues[paramName][0] = x;
	mParamValues[paramName][1] = y;
}

void LLWLParamSet::set(const std::string& paramName, float x, float y, float z) 
{
	mParamValues[paramName][0] = x;
	mParamValues[paramName][1] = y;
	mParamValues[paramName][2] = z;
}

void LLWLParamSet::set(const std::string& paramName, float x, float y, float z, float w) 
{
	mParamValues[paramName][0] = x;
	mParamValues[paramName][1] = y;
	mParamValues[paramName][2] = z;
	mParamValues[paramName][3] = w;
}

void LLWLParamSet::set(const std::string& paramName, const float * val) 
{
	mParamValues[paramName][0] = val[0];
	mParamValues[paramName][1] = val[1];
	mParamValues[paramName][2] = val[2];
	mParamValues[paramName][3] = val[3];
}

void LLWLParamSet::set(const std::string& paramName, const LLVector4 & val) 
{
	mParamValues[paramName][0] = val.mV[0];
	mParamValues[paramName][1] = val.mV[1];
	mParamValues[paramName][2] = val.mV[2];
	mParamValues[paramName][3] = val.mV[3];
}

void LLWLParamSet::set(const std::string& paramName, const LLColor4 & val) 
{
	mParamValues[paramName][0] = val.mV[0];
	mParamValues[paramName][1] = val.mV[1];
	mParamValues[paramName][2] = val.mV[2];
	mParamValues[paramName][3] = val.mV[3];
}

LLVector4 LLWLParamSet::getVector(const std::string& paramName, bool& error) 
{
	// test to see if right type
	LLSD cur_val = mParamValues.get(paramName);
	if (!cur_val.isArray()) 
	{
		error = true;
		return LLVector4(0,0,0,0);
	}
	
	LLVector4 val;
	val.mV[0] = (F32) cur_val[0].asReal();
	val.mV[1] = (F32) cur_val[1].asReal();
	val.mV[2] = (F32) cur_val[2].asReal();
	val.mV[3] = (F32) cur_val[3].asReal();
	
	error = false;
	return val;
}

F32 LLWLParamSet::getFloat(const std::string& paramName, bool& error) 
{
	// test to see if right type
	LLSD cur_val = mParamValues.get(paramName);
	if (cur_val.isArray() && cur_val.size() != 0) 
	{
		error = false;
		return (F32) cur_val[0].asReal();	
	}
	
	if(cur_val.isReal())
	{
		error = false;
		return (F32) cur_val.asReal();
	}
	
	error = true;
	return 0;
}

void LLWLParamSet::setSunAngle(float val) 
{
	// keep range 0 - 2pi
	if(val > F_TWO_PI || val < 0)
	{
		F32 num = val / F_TWO_PI;
		num -= floor(num);
		val = F_TWO_PI * num;
	}

	mParamValues["sun_angle"] = val;
}


void LLWLParamSet::setEastAngle(float val) 
{
	// keep range 0 - 2pi
	if(val > F_TWO_PI || val < 0)
	{
		F32 num = val / F_TWO_PI;
		num -= floor(num);
		val = F_TWO_PI * num;
	}

	mParamValues["east_angle"] = val;
}

void LLWLParamSet::mix(LLWLParamSet& src, LLWLParamSet& dest, F32 weight)
{
	// set up the iterators

	LLSD srcVal;
	LLSD destVal;

	// Iterate through values
	for(LLSD::map_iterator iter = mParamValues.beginMap(); iter != mParamValues.endMap(); ++iter)
	{
		// If param exists in both src and dest, set the holder variables, otherwise skip
		if(src.mParamValues.has(iter->first) && dest.mParamValues.has(iter->first))
		{
			srcVal = src.mParamValues[iter->first];
			destVal = dest.mParamValues[iter->first];
		}
		else
		{
			continue;
		}
		
		if(iter->second.isReal())									// If it's a real, interpolate directly
		{
			iter->second = srcVal.asReal() + ((destVal.asReal() - srcVal.asReal()) * weight);
		}
		else if(iter->second.isArray() && iter->second[0].isReal()	// If it's an array of reals, loop through the reals and interpolate on those
				&& iter->second.size() == srcVal.size() && iter->second.size() == destVal.size())
		{
			// Actually do interpolation: old value + (difference in values * factor)
			for(int i=0; i < iter->second.size(); ++i) 
			{
				// iter->second[i] = (1.f-weight)*(F32)srcVal[i].asReal() + weight*(F32)destVal[i].asReal();	// old way of doing it -- equivalent but one more operation
				iter->second[i] = srcVal[i].asReal() + ((destVal[i].asReal() - srcVal[i].asReal()) * weight);
			}
		}
		else														// Else, skip
		{
			continue;
		}		
	}

	// now mix the extra parameters
	setStarBrightness((1 - weight) * (F32) src.getStarBrightness()
		+ weight * (F32) dest.getStarBrightness());

	llassert(src.getSunAngle() >= - F_PI && 
					src.getSunAngle() <= 3 * F_PI);
	llassert(dest.getSunAngle() >= - F_PI && 
					dest.getSunAngle() <= 3 * F_PI);
	llassert(src.getEastAngle() >= 0 && 
					src.getEastAngle() <= 4 * F_PI);
	llassert(dest.getEastAngle() >= 0 && 
					dest.getEastAngle() <= 4 * F_PI);

	// sun angle and east angle require some handling to make sure
	// they go in circles.  Yes quaternions would work better.
	F32 srcSunAngle = src.getSunAngle();
	F32 destSunAngle = dest.getSunAngle();
	F32 srcEastAngle = src.getEastAngle();
	F32 destEastAngle = dest.getEastAngle();
	
	if(fabsf(srcSunAngle - destSunAngle) > F_PI) 
	{
		if(srcSunAngle > destSunAngle) 
		{
			destSunAngle += 2 * F_PI;
		} 
		else 
		{
			srcSunAngle += 2 * F_PI;
		}
	}

	if(fabsf(srcEastAngle - destEastAngle) > F_PI) 
	{
		if(srcEastAngle > destEastAngle) 
		{
			destEastAngle += 2 * F_PI;
		} 
		else 
		{
			srcEastAngle += 2 * F_PI;
		}
	}

	// now setup the sun properly
	setSunAngle((1 - weight) * srcSunAngle + weight * destSunAngle);
	setEastAngle((1 - weight) * srcEastAngle + weight * destEastAngle);
}

void LLWLParamSet::updateCloudScrolling(void) 
{
	static LLTimer s_cloud_timer;

	F64 delta_t = s_cloud_timer.getElapsedTimeAndResetF64();

	if(getEnableCloudScrollX())
	{
		mCloudScrollXOffset += F32(delta_t * (getCloudScrollX() - 10.f) / 100.f);
	}
	if(getEnableCloudScrollY())
	{
		mCloudScrollYOffset += F32(delta_t * (getCloudScrollY() - 10.f) / 100.f);
	}
}

void LLWLParamSet::updateHashedNames()
{
	mParamHashedNames.clear();
	// Iterate through values
	for(LLSD::map_iterator iter = mParamValues.beginMap(); iter != mParamValues.endMap(); ++iter)
	{
		mParamHashedNames.push_back(LLStaticHashedString(iter->first));
	}
}

