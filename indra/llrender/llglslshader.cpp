/** 
 * @file llglslshader.cpp
 * @brief GLSL helper functions and state.
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

#include "linden_common.h"

#include "llglslshader.h"

#include "llshadermgr.h"
#include "llfile.h"
#include "llrender.h"
#include "llvertexbuffer.h"

#if LL_DARWIN
#include "OpenGL/OpenGL.h"
#endif

GLuint LLGLSLShader::sCurBoundShader = 0;
LLGLSLShader* LLGLSLShader::sCurBoundShaderPtr = nullptr;
S32 LLGLSLShader::sIndexedTextureChannels = 0;
bool LLGLSLShader::sNoFixedFunction = false;
bool LLGLSLShader::sProfileEnabled = false;
std::set<LLGLSLShader*> LLGLSLShader::sInstances;
U64 LLGLSLShader::sTotalTimeElapsed = 0;
U32 LLGLSLShader::sTotalTrianglesDrawn = 0;
U64 LLGLSLShader::sTotalSamplesDrawn = 0;
U32 LLGLSLShader::sTotalDrawCalls = 0;

//UI shader -- declared here so llui_libtest will link properly
LLGLSLShader    gUIProgram;
LLGLSLShader    gSolidColorProgram;

BOOL shouldChange(const LLVector4& v1, const LLVector4& v2)
{
    return v1 != v2;
}

LLShaderFeatures::LLShaderFeatures()
    : atmosphericHelpers(false)
    , calculatesLighting(false)
    , calculatesAtmospherics(false)
    , hasLighting(false)
    , isAlphaLighting(false)
    , isShiny(false)
    , isFullbright(false)
    , isSpecular(false)
    , hasWaterFog(false)
    , hasTransport(false)
    , hasSkinning(false)
    , hasObjectSkinning(false)
    , hasAtmospherics(false)
    , hasGamma(false)
    , mIndexedTextureChannels(0)
    , disableTextureIndex(false)
    , hasAlphaMask(false)
    , attachNothing(false)
{
}

//===============================
// LLGLSL Shader implementation
//===============================

//static
void LLGLSLShader::initProfile()
{
    sProfileEnabled = true;
    sTotalTimeElapsed = 0;
    sTotalTrianglesDrawn = 0;
    sTotalSamplesDrawn = 0;
    sTotalDrawCalls = 0;

    for (auto sInstance : sInstances)
    {
        sInstance->clearStats();
    }
}


struct LLGLSLShaderCompareTimeElapsed
{
        bool operator()(const LLGLSLShader* const& lhs, const LLGLSLShader* const& rhs)
        {
            return lhs->mTimeElapsed < rhs->mTimeElapsed;
        }
};

//static
void LLGLSLShader::finishProfile(bool emit_report)
{
    sProfileEnabled = false;

    if (emit_report)
    {
        std::vector<LLGLSLShader*> sorted(sInstances.size());
        for (auto sInstance : sInstances)
        {
            sorted.push_back(sInstance);
        }

        std::sort(sorted.begin(), sorted.end(), LLGLSLShaderCompareTimeElapsed());

        for (auto& iter : sorted)
        {
            iter->dumpStats();
        }
            
    LL_INFOS() << "-----------------------------------" << LL_ENDL;
    LL_INFOS() << "Total rendering time: " << llformat("%.4f ms", sTotalTimeElapsed/1000000.f) << LL_ENDL;
    LL_INFOS() << "Total samples drawn: " << llformat("%.4f million", sTotalSamplesDrawn/1000000.f) << LL_ENDL;
    LL_INFOS() << "Total triangles drawn: " << llformat("%.3f million", sTotalTrianglesDrawn/1000000.f) << LL_ENDL;
    }
}

void LLGLSLShader::clearStats()
{
    mTrianglesDrawn = 0;
    mTimeElapsed = 0;
    mSamplesDrawn = 0;
    mDrawCalls = 0;
    mTextureStateFetched = false;
    mTextureMagFilter.clear();
    mTextureMinFilter.clear();
}

void LLGLSLShader::dumpStats()
{
    if (mDrawCalls > 0)
    {
        LL_INFOS() << "=============================================" << LL_ENDL;
        LL_INFOS() << mName << LL_ENDL;
        for (auto& shader_file : mShaderFiles)
        {
            LL_INFOS() << shader_file.first << LL_ENDL;
        }
        for (U32 i = 0; i < mTexture.size(); ++i)
        {
            GLint idx = mTexture[i];
            
            if (idx >= 0)
            {
                GLint uniform_idx = getUniformLocation(i);
                LL_INFOS() << mUniformNameMap[uniform_idx] << " - " << std::hex << mTextureMagFilter[i] << "/" << mTextureMinFilter[i] << std::dec << LL_ENDL;
            }
        }
        LL_INFOS() << "=============================================" << LL_ENDL;
    
        F32 ms = mTimeElapsed/1000000.f;
        F32 seconds = ms/1000.f;

        F32 pct_tris = (F32) mTrianglesDrawn/(F32)sTotalTrianglesDrawn*100.f;
        F32 tris_sec = (F32) (mTrianglesDrawn/1000000.0);
        tris_sec /= seconds;

        F32 pct_samples = (F32) ((F64)mSamplesDrawn/(F64)sTotalSamplesDrawn)*100.f;
        F32 samples_sec = (F32) mSamplesDrawn/1000000000.f;
        samples_sec /= seconds;

        F32 pct_calls = (F32) mDrawCalls/(F32)sTotalDrawCalls*100.f;
        U32 avg_batch = mTrianglesDrawn/mDrawCalls;

        LL_INFOS() << "Triangles Drawn: " << mTrianglesDrawn <<  " " << llformat("(%.2f pct of total, %.3f million/sec)", pct_tris, tris_sec ) << LL_ENDL;
        LL_INFOS() << "Draw Calls: " << mDrawCalls << " " << llformat("(%.2f pct of total, avg %d tris/call)", pct_calls, avg_batch) << LL_ENDL;
        LL_INFOS() << "SamplesDrawn: " << mSamplesDrawn << " " << llformat("(%.2f pct of total, %.3f billion/sec)", pct_samples, samples_sec) << LL_ENDL;
        LL_INFOS() << "Time Elapsed: " << mTimeElapsed << " " << llformat("(%.2f pct of total, %.5f ms)\n", (F32) ((F64)mTimeElapsed/(F64)sTotalTimeElapsed)*100.f, ms) << LL_ENDL;
    }
}

//static
void LLGLSLShader::startProfile()
{
    if (sProfileEnabled && sCurBoundShaderPtr)
    {
        sCurBoundShaderPtr->placeProfileQuery();
    }

}

//static
void LLGLSLShader::stopProfile(U32 count, U32 mode)
{
    if (sProfileEnabled && sCurBoundShaderPtr)
    {
        sCurBoundShaderPtr->readProfileQuery(count, mode);
    }
}

void LLGLSLShader::placeProfileQuery()
{
#if !LL_DARWIN
    if (mTimerQuery == 0)
    {
        glGenQueriesARB(1, &mSamplesQuery);
        glGenQueriesARB(1, &mTimerQuery);
    }

    if (!mTextureStateFetched)
    {
        mTextureStateFetched = true;
        mTextureMagFilter.resize(mTexture.size());
        mTextureMinFilter.resize(mTexture.size());

        U32 cur_active = gGL.getCurrentTexUnitIndex();

        for (U32 i = 0; i < mTexture.size(); ++i)
        {
            GLint idx = mTexture[i];

            if (idx >= 0)
            {
                gGL.getTexUnit(idx)->activate();

                U32 mag = 0xFFFFFFFF;
                U32 min = 0xFFFFFFFF;

                U32 type = LLTexUnit::getInternalType(gGL.getTexUnit(idx)->getCurrType());

                glGetTexParameteriv(type, GL_TEXTURE_MAG_FILTER, (GLint*) &mag);
                glGetTexParameteriv(type, GL_TEXTURE_MIN_FILTER, (GLint*) &min);

                mTextureMagFilter[i] = mag;
                mTextureMinFilter[i] = min;
            }
        }

        gGL.getTexUnit(cur_active)->activate();
    }


    glBeginQueryARB(GL_SAMPLES_PASSED, mSamplesQuery);
    glBeginQueryARB(GL_TIME_ELAPSED, mTimerQuery);
#endif
}

void LLGLSLShader::readProfileQuery(U32 count, U32 mode)
{
#if !LL_DARWIN
    glEndQueryARB(GL_TIME_ELAPSED);
    glEndQueryARB(GL_SAMPLES_PASSED);
    
    GLuint64 time_elapsed = 0;
    glGetQueryObjectui64v(mTimerQuery, GL_QUERY_RESULT, &time_elapsed);

    GLuint64 samples_passed = 0;
    glGetQueryObjectui64v(mSamplesQuery, GL_QUERY_RESULT, &samples_passed);

    sTotalTimeElapsed += time_elapsed;
    mTimeElapsed += time_elapsed;

    sTotalSamplesDrawn += samples_passed;
    mSamplesDrawn += samples_passed;

    U32 tri_count = 0;
    switch (mode)
    {
        case LLRender::TRIANGLES: tri_count = count/3; break;
        case LLRender::TRIANGLE_FAN: tri_count = count-2; break;
        case LLRender::TRIANGLE_STRIP: tri_count = count-2; break;
        default: tri_count = count; break; //points lines etc just use primitive count
    }

    mTrianglesDrawn += tri_count;
    sTotalTrianglesDrawn += tri_count;

    sTotalDrawCalls++;
    mDrawCalls++;
#endif
}



LLGLSLShader::LLGLSLShader()
    : mLightHash(0), 
      mProgramObject(0),
      mAttributeMask(0),
      mTotalUniformSize(0), 
      mActiveTextureChannels(0), 
      mShaderLevel(0), 
      mShaderGroup(SG_DEFAULT),
      mUniformsDirty(FALSE),
      mTimerQuery(0),
      mSamplesQuery(0),
      mTimeElapsed(0),
      mTrianglesDrawn(0),
      mSamplesDrawn(0),
      mDrawCalls(0),
      mTextureStateFetched(false)

{
    
}

LLGLSLShader::~LLGLSLShader()
{
}

void LLGLSLShader::unload()
{
    mShaderFiles.clear();
    mDefines.clear();

    unloadInternal();
}

void LLGLSLShader::unloadInternal()
{
    sInstances.erase(this);

    stop_glerror();
    mAttribute.clear();
    mTexture.clear();
    mUniform.clear();

    if (mProgramObject)
    {
        glDeleteProgram(mProgramObject);
        mProgramObject = 0;
    }

    if (mTimerQuery)
    {
        glDeleteQueriesARB(1, &mTimerQuery);
        mTimerQuery = 0;
    }

    if (mSamplesQuery)
    {
        glDeleteQueriesARB(1, &mSamplesQuery);
        mSamplesQuery = 0;
    }

    //hack to make apple not complain
    glGetError();

    stop_glerror();
}

BOOL LLGLSLShader::createShader(std::vector<LLStaticHashedString> * attributes,
                                std::vector<LLStaticHashedString> * uniforms,
                                U32 varying_count,
                                const char** varyings)
{
    unloadInternal();

    sInstances.insert(this);

    //reloading, reset matrix hash values
    for (unsigned int& i : mMatHash)
    {
        i = 0xFFFFFFFF;
    }
    mLightHash = 0xFFFFFFFF;

    llassert_always(!mShaderFiles.empty());
    BOOL success = TRUE;

    // Purge the old program just in case
    if (mProgramObject)
        glDeleteProgram(mProgramObject);

    // Create program
    mProgramObject = glCreateProgram();
    
#if LL_DARWIN
    // work-around missing mix(vec3,vec3,bvec3)
    mDefines["OLD_SELECT"] = "1";
#endif
    
    //compile new source
    std::vector< std::pair<std::string,GLenum> >::iterator fileIter = mShaderFiles.begin();
    for ( ; fileIter != mShaderFiles.end(); ++fileIter )
    {
        GLuint shaderhandle = LLShaderMgr::instance()->loadShaderFile((*fileIter).first, mShaderLevel, (*fileIter).second, &mDefines, mFeatures.mIndexedTextureChannels);
        LL_DEBUGS("ShaderLoading") << "SHADER FILE: " << (*fileIter).first << " mShaderLevel=" << mShaderLevel << LL_ENDL;
        if (shaderhandle > 0)
        {
            attachShader(shaderhandle);
        }
        else
        {
            success = FALSE;
        }
    }

    // Attach existing objects
    if (!LLShaderMgr::instance()->attachShaderFeatures(this))
    {
        glDeleteProgram(mProgramObject);
        mProgramObject = 0;
        return FALSE;
    }

    if (gGLManager.mGLSLVersionMajor < 2 && gGLManager.mGLSLVersionMinor < 3)
    { //indexed texture rendering requires GLSL 1.3 or later
        //attachShaderFeatures may have set the number of indexed texture channels, so set to 1 again
        mFeatures.mIndexedTextureChannels = llmin(mFeatures.mIndexedTextureChannels, 1);
    }

#ifdef GL_INTERLEAVED_ATTRIBS
    if (varying_count > 0 && varyings)
    {
        glTransformFeedbackVaryings(mProgramObject, varying_count, varyings, GL_INTERLEAVED_ATTRIBS);
    }
#endif

    // Map attributes and uniforms
    if (success)
    {
        success = mapAttributes(attributes);
    }
    if (success)
    {
        success = mapUniforms(uniforms);
    }
    if( !success )
    {
        glDeleteProgram(mProgramObject);
        mProgramObject = 0;

        LL_WARNS("ShaderLoading") << "Failed to link shader: " << mName << LL_ENDL;

        // Try again using a lower shader level;
        if (mShaderLevel > 0)
        {
            LL_WARNS("ShaderLoading") << "Failed to link using shader level " << mShaderLevel << " trying again using shader level " << (mShaderLevel - 1) << LL_ENDL;
            mShaderLevel--;
            return createShader(attributes,uniforms);
        }
    }

    if (LLShaderMgr::instance()->mProgramObjects.find(mName) == LLShaderMgr::instance()->mProgramObjects.end())
    {
        LLShaderMgr::instance()->mProgramObjects.emplace(mName, mProgramObject);
    }
    else
    {
        LL_WARNS("ShaderLoading") << "Attempting to create shader program with duplicate name: " << mName << LL_ENDL;
    }

    return success;
}

BOOL LLGLSLShader::attachShader(const std::string& object)
{
	const auto& shader_objects = LLShaderMgr::instance()->mShaderObjects;
	for (const auto& shader_object : shader_objects)
    {
		if (shader_object.first == object)
		{
			if (glIsShader(shader_object.second.mHandle))
			{
				glAttachShader(mProgramObject, shader_object.second.mHandle);
				stop_glerror();
				return TRUE;
			}
		}
	}

    {
        LL_WARNS("ShaderLoading") << "Attempting to attach shader object that hasn't been compiled: " << object << LL_ENDL;
        return FALSE;
    }
}

void LLGLSLShader::attachShader(GLuint object)
{
    if (object != 0)
    {
		const auto& shader_objects = LLShaderMgr::instance()->mShaderObjects;
		auto it = shader_objects.begin();
		for (; it != shader_objects.end(); it++)
		{
			if (it->second.mHandle == object)
			{
				LL_DEBUGS("ShaderLoading") << "Attached: " << (*it).first << LL_ENDL;
				break;
			}
		}
		if (it == shader_objects.end())
		{
			LL_WARNS("ShaderLoading") << "Attached unknown shader!" << LL_ENDL;
		}
        stop_glerror();
        glAttachShader(mProgramObject, object);
        stop_glerror();
    }
    else
    {
        LL_WARNS("ShaderLoading") << "Attempting to attach non existing shader object. " << LL_ENDL;
    }
}

void LLGLSLShader::attachShaders(GLuint* objects, S32 count)
{
    for (S32 i = 0; i < count; i++)
    {
        attachShader(objects[i]);
    }
}

BOOL LLGLSLShader::mapAttributes(const std::vector<LLStaticHashedString> * attributes)
{
	const auto& shader_mgr = LLShaderMgr::instance();
    //before linking, make sure reserved attributes always have consistent locations
    for (U32 i = 0; i < shader_mgr->mReservedAttribs.size(); i++)
    {
        const char* name = shader_mgr->mReservedAttribs[i].c_str();
        glBindAttribLocation(mProgramObject, i, (const GLchar*) name);
    }
    
    //link the program
    BOOL res = link();

    mAttribute.clear();
    U32 numAttributes = (attributes == nullptr) ? 0 : attributes->size();
#if LL_RELEASE_WITH_DEBUG_INFO
    mAttribute.resize(LLShaderMgr::instance()->mReservedAttribs.size() + numAttributes, { -1, NULL });
#else
    mAttribute.resize(shader_mgr->mReservedAttribs.size() + numAttributes, -1);
#endif
    
    if (res)
    { //read back channel locations

        mAttributeMask = 0;

        //read back reserved channels first
        for (U32 i = 0; i < shader_mgr->mReservedAttribs.size(); i++)
        {
            const char* name = shader_mgr->mReservedAttribs[i].c_str();
            S32 index = glGetAttribLocation(mProgramObject, (const GLchar*)name);
            if (index != -1)
            {
#if LL_RELEASE_WITH_DEBUG_INFO
                mAttribute[i] = { index, name };
#else
                mAttribute[i] = index;
#endif
                mAttributeMask |= 1 << i;
                LL_DEBUGS("ShaderUniform") << "Attribute " << name << " assigned to channel " << index << LL_ENDL;
            }
        }
        if (attributes != nullptr)
        {
            for (U32 i = 0; i < numAttributes; i++)
            {
                const char* name = (*attributes)[i].String().c_str();
                S32 index = glGetAttribLocation(mProgramObject, (const GLchar*)name);
                if (index != -1)
                {
                    mAttribute[shader_mgr->mReservedAttribs.size() + i] = index;
                    LL_DEBUGS("ShaderUniform") << "Attribute " << name << " assigned to channel " << index << LL_ENDL;
                }
            }
        }

        return TRUE;
    }
    
    return FALSE;
}

void LLGLSLShader::mapUniform(const gl_uniform_data_t& gl_uniform, const std::vector<LLStaticHashedString> * uniforms)
{
	char* name = (char*)gl_uniform.name.c_str(); //blegh
#if !LL_DARWIN
	GLint size = gl_uniform.size;
	if (size > 0)
	{
		switch(gl_uniform.type)
		{
			case GL_FLOAT_VEC2: size *= 2; break;
			case GL_FLOAT_VEC3: size *= 3; break;
			case GL_FLOAT_VEC4: size *= 4; break;
			case GL_DOUBLE: size *= 2; break;
			case GL_DOUBLE_VEC2: size *= 2; break;
			case GL_DOUBLE_VEC3: size *= 6; break;
			case GL_DOUBLE_VEC4: size *= 8; break;
			case GL_INT_VEC2: size *= 2; break;
			case GL_INT_VEC3: size *= 3; break;
			case GL_INT_VEC4: size *= 4; break;
			case GL_UNSIGNED_INT_VEC2: size *= 2; break;
			case GL_UNSIGNED_INT_VEC3: size *= 3; break;
			case GL_UNSIGNED_INT_VEC4: size *= 4; break;
			case GL_BOOL_VEC2: size *= 2; break;
			case GL_BOOL_VEC3: size *= 3; break;
			case GL_BOOL_VEC4: size *= 4; break;
			case GL_FLOAT_MAT2: size *= 4; break;
			case GL_FLOAT_MAT3: size *= 9; break;
			case GL_FLOAT_MAT4: size *= 16; break;
			case GL_FLOAT_MAT2x3: size *= 6; break;
			case GL_FLOAT_MAT2x4: size *= 8; break;
			case GL_FLOAT_MAT3x2: size *= 6; break;
			case GL_FLOAT_MAT3x4: size *= 12; break;
			case GL_FLOAT_MAT4x2: size *= 8; break;
			case GL_FLOAT_MAT4x3: size *= 12; break;
			case GL_DOUBLE_MAT2: size *= 8; break;
			case GL_DOUBLE_MAT3: size *= 18; break;
			case GL_DOUBLE_MAT4: size *= 32; break;
			case GL_DOUBLE_MAT2x3: size *= 12; break;
			case GL_DOUBLE_MAT2x4: size *= 16; break;
			case GL_DOUBLE_MAT3x2: size *= 12; break;
			case GL_DOUBLE_MAT3x4: size *= 24; break;
			case GL_DOUBLE_MAT4x2: size *= 16; break;
			case GL_DOUBLE_MAT4x3: size *= 24; break;
		}
		mTotalUniformSize += size;
	}
#endif

	S32 location = glGetUniformLocation(mProgramObject, name);
	if (location != -1)
	{
		//chop off "[0]" so we can always access the first element
		//of an array by the array name
		char* is_array = strstr(name, "[0]");
		if (is_array)
		{
			is_array[0] = 0;
		}

		LLStaticHashedString hashedName(name);
        mUniformNameMap[location] = name;
		mUniformMap[hashedName] = location;

        LL_DEBUGS("ShaderUniform") << "Uniform " << name << " is at location " << location << LL_ENDL;

		// Indexed textures are referenced by hardcoded tex unit index. This is where that mapping happens.
		if (gl_uniform.texunit_priority < (U32)mFeatures.mIndexedTextureChannels)
		{
			// mUniform and mTexture are irrelivant for indexed textures, since there's no enum to look them up through.
			// Thus, only call mapUniformTextureChannel to create the texunit => uniform location mapping in opengl.
			mapUniformTextureChannel(location, gl_uniform.type);
			return;
		}
	
		//find the index of this uniform
		for (U32 i = 0; i < LLShaderMgr::instance()->mReservedUniforms.size(); i++)
		{
			if ( (mUniform[i] == -1)
				&& (LLShaderMgr::instance()->mReservedUniforms[i] == name))
			{
				//found it
				mUniform[i] = location;
				mTexture[i] = mapUniformTextureChannel(location, gl_uniform.type);
				return;
			}
		}

		if (uniforms != NULL)
		{
			U32 j = LLShaderMgr::instance()->mReservedUniforms.size();
			for (U32 i = 0; i < uniforms->size(); i++, j++)
			{
				if ( (mUniform[j] == -1)
					&& ((*uniforms)[i].String() == name))
				{
					//found it
					mUniform[j] = location;
					mTexture[j] = mapUniformTextureChannel(location, gl_uniform.type);
					return;
				}
			}
		}
	}
}

void LLGLSLShader::addPermutation(const std::string& name, const std::string& value)
{
    mDefines[name] = value;
}

void LLGLSLShader::removePermutation(const std::string& name)
{
    mDefines[name].erase();
}

GLint LLGLSLShader::mapUniformTextureChannel(GLint location, GLenum type)
{
    if ((type >= GL_SAMPLER_1D_ARB && type <= GL_SAMPLER_2D_RECT_SHADOW_ARB))
    {   //this here is a texture
        glUniform1i(location, mActiveTextureChannels);
        LL_DEBUGS("ShaderUniform") << "Assigned " << mUniformNameMap[location] << " to texture channel " << mActiveTextureChannels << LL_ENDL;
        return mActiveTextureChannels++;
    }
    return -1;
}

BOOL LLGLSLShader::mapUniforms(const std::vector<LLStaticHashedString> * uniforms)
{
	BOOL res = TRUE;

	const auto& reservedUniforms = LLShaderMgr::instance()->mReservedUniforms;

	mTotalUniformSize = 0;
	mActiveTextureChannels = 0;
	mUniform.clear();
	mUniformMap.clear();
	mUniformNameMap.clear();
	mTexture.clear();
	mValueVec4.clear();
	mValueMat3.clear();
	mValueMat4.clear();
	//initialize arrays
	U32 numUniforms = (uniforms == NULL) ? 0 : uniforms->size();
	mUniform.resize(numUniforms + reservedUniforms.size(), -1);
	mTexture.resize(numUniforms + reservedUniforms.size(), -1);
	
	bind();

	//get the number of active uniforms
	GLint activeCount;
	glGetProgramiv(mProgramObject, GL_ACTIVE_UNIFORMS, &activeCount);

	std::vector< gl_uniform_data_t > gl_uniforms;
	
	bool has_diffuse = false;
	U32 max_index = mFeatures.mIndexedTextureChannels;
	// Gather active uniforms.
	for (S32 i = 0; i < activeCount; i++)
	{
		// Fetch name and size from opengl
		char name[1024];
		gl_uniform_data_t gl_uniform;
		GLsizei length;
		glGetActiveUniform(mProgramObject, i, 1024, &length, &gl_uniform.size, &gl_uniform.type, (GLcharARB *)name);
		if (length && name[length - 1] == '\0')
		{
			--length; // Some drivers can't be trusted...
		}
		if (gl_uniform.size < 0 || length <= 0)
			continue;
		gl_uniform.name = std::string(name, length);

		// Track if diffuseMap uniform was detected. If so, flag as such to assert indexed textures aren't also used in this shader.
		has_diffuse |= gl_uniform.name == "diffuseMap";

		// Use mReservedUniforms to calculate texunit ordering. Reserve priority [0,max_index) for indexed textures if applicable.
		auto it = std::find(reservedUniforms.cbegin(), reservedUniforms.cend(), gl_uniform.name);
		gl_uniform.texunit_priority = it != reservedUniforms.cend() ? max_index + std::distance(reservedUniforms.cbegin(), it) : UINT_MAX;

		// Indexed texture uniforms must ALWAYS have highest texunit priority. Ensures [texunit[0],texunit[max_index]) map to [tex[0],tex[max_index]) uniforms.
		// Note that this logic will break if a tex# index is skipped over in the shader.
		if (gl_uniform.texunit_priority == UINT_MAX)
		{
			S32 idx;
			if (sscanf(gl_uniform.name.c_str(), "tex%d", &idx) && idx < (S32)max_index)
			{
				gl_uniform.texunit_priority = idx;
			}
		}
		
		gl_uniforms.push_back(gl_uniform);
	}

	// Sort uniforms by texunit_priority
	std::sort(gl_uniforms.begin(), gl_uniforms.end(), [](gl_uniform_data_t& lhs, gl_uniform_data_t& rhs)
	{
		return lhs.texunit_priority < rhs.texunit_priority;
	});

	// Sanity check
	if (!gl_uniforms.empty()
        && (gl_uniforms[0].name == "tex0" || gl_uniforms[0].name == "bumpMap"))
	{
		llassert_always_msg(!has_diffuse, "Indexed textures and diffuseMap are incompatible!");
	}

	for (auto& gl_uniform : gl_uniforms)
	{
		mapUniform(gl_uniform, uniforms);
	}

	unbind();

	LL_DEBUGS("ShaderUniform") << "Total Uniform Size: " << mTotalUniformSize << LL_ENDL;
	return res;
}


BOOL LLGLSLShader::link(BOOL suppress_errors)
{
    BOOL success = LLShaderMgr::instance()->linkProgram(mProgramObject, suppress_errors);

    if (!suppress_errors)
    {
        LLShaderMgr::instance()->dumpProgramLog(mProgramObject, !success, mName);
    }

    return success;
}

void LLGLSLShader::bind()
{
    gGL.flush();
    if (gGLManager.mHasShaderObjects)
    {
        LLVertexBuffer::unbind();
        glUseProgram(mProgramObject);
        sCurBoundShader = mProgramObject;
        sCurBoundShaderPtr = this;
        if (mUniformsDirty)
        {
            LLShaderMgr::instance()->updateShaderUniforms(this);
            mUniformsDirty = FALSE;
        }
    }
}

void LLGLSLShader::unbind()
{
    gGL.flush();
    if (gGLManager.mHasShaderObjects)
    {
        stop_glerror();
        if (gGLManager.mIsNVIDIA)
        {
            for (U32 i = 0; i < mAttribute.size(); ++i)
            {
                vertexAttrib4f(i, 0,0,0,1);
                stop_glerror();
            }
        }
        LLVertexBuffer::unbind();
        glUseProgram(0);
        sCurBoundShader = 0;
        sCurBoundShaderPtr = nullptr;
        stop_glerror();
    }
}

void LLGLSLShader::bindNoShader(void)
{
    LLVertexBuffer::unbind();
    if (gGLManager.mHasShaderObjects)
    {
        glUseProgram(0);
        sCurBoundShader = 0;
        sCurBoundShaderPtr = nullptr;
    }
}

S32 LLGLSLShader::bindTexture(const std::string &uniform, LLTexture *texture, LLTexUnit::eTextureType mode)
{
    S32 channel = getUniformLocation(uniform);
    
    return bindTexture(channel, texture, mode);
}

S32 LLGLSLShader::bindTexture(S32 uniform, LLTexture *texture, LLTexUnit::eTextureType mode)
{
    if (uniform < 0 || uniform >= (S32)mTexture.size())
    {
        UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
        return -1;
    }
    
    uniform = mTexture[uniform];
    
    if (uniform > -1)
    {
        gGL.getTexUnit(uniform)->bind(texture, mode);
    }
    
    return uniform;
}

S32 LLGLSLShader::unbindTexture(const std::string &uniform, LLTexUnit::eTextureType mode)
{
    S32 channel = 0;
    channel = getUniformLocation(uniform);
    
    return unbindTexture(channel);
}

S32 LLGLSLShader::unbindTexture(S32 uniform, LLTexUnit::eTextureType mode)
{
    if (uniform < 0 || uniform >= (S32)mTexture.size())
    {
        UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
        return -1;
    }
    
    uniform = mTexture[uniform];
    
    if (uniform > -1)
    {
        gGL.getTexUnit(uniform)->unbind(mode);
    }
    
    return uniform;
}

S32 LLGLSLShader::enableTexture(S32 uniform, LLTexUnit::eTextureType mode)
{
    if (uniform < 0 || uniform >= (S32)mTexture.size())
    {
        UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
        return -1;
    }
    S32 index = mTexture[uniform];
    if (index != -1)
    {
        gGL.getTexUnit(index)->activate();
        gGL.getTexUnit(index)->enable(mode);
    }
    return index;
}

S32 LLGLSLShader::disableTexture(S32 uniform, LLTexUnit::eTextureType mode)
{
    if (uniform < 0 || uniform >= (S32)mTexture.size())
    {
        UNIFORM_ERRS << "Uniform out of range: " << uniform << LL_ENDL;
        return -1;
    }
    S32 index = mTexture[uniform];
    if (index != -1 && gGL.getTexUnit(index)->getCurrType() != LLTexUnit::TT_NONE)
    {
        if (gDebugGL && gGL.getTexUnit(index)->getCurrType() != mode)
        {
            if (gDebugSession)
            {
                gFailLog << "Texture channel " << index << " texture type corrupted." << std::endl;
                ll_fail("LLGLSLShader::disableTexture failed");
            }
            else
            {
                LL_ERRS() << "Texture channel " << index << " texture type corrupted." << LL_ENDL;
            }
        }
        gGL.getTexUnit(index)->disable();
    }
    return index;
}

GLint LLGLSLShader::getAttribLocation(U32 attrib)
{
    if (attrib < mAttribute.size())
    {
        return mAttribute[attrib];
    }
    else
    {
        return -1;
    }
}

void LLGLSLShader::vertexAttrib4f(U32 index, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
    if (mAttribute[index] > 0)
    {
        glVertexAttrib4f(mAttribute[index], x, y, z, w);
    }
}

void LLGLSLShader::setMinimumAlpha(F32 minimum)
{
    gGL.flush();
    uniform1f(LLShaderMgr::MINIMUM_ALPHA, minimum);
}
