/** 
 * @file alphaNonIndexedNoColorF.glsl
 *
 * $LicenseInfo:firstyear=2005&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2005, Linden Research, Inc.
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
 
#extension GL_ARB_texture_rectangle : enable

#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform float minimum_alpha;

uniform sampler2DShadow shadowMap0;
uniform sampler2DShadow shadowMap1;
uniform sampler2DShadow shadowMap2;
uniform sampler2DShadow shadowMap3;
uniform sampler2DRect depthMap;
uniform sampler2D diffuseMap;

uniform mat4 shadow_matrix[6];
uniform vec4 shadow_clip;
uniform vec2 screen_res;

vec3 atmosLighting(vec3 light);
vec3 scaleSoftClip(vec3 light);

VARYING vec3 vary_ambient;
VARYING vec3 vary_directional;
VARYING vec3 vary_fragcoord;
VARYING vec3 vary_position;
VARYING vec3 vary_pointlight_col;
VARYING vec2 vary_texcoord0;
VARYING vec3 vary_norm;

uniform vec2 shadow_res;

uniform float shadow_bias;

uniform mat4 inv_proj;

uniform vec4 light_position[8];
uniform vec3 light_direction[8];
uniform vec3 light_attenuation[8]; 
uniform vec3 light_diffuse[8];

vec3 calcDirectionalLight(vec3 n, vec3 l)
{
        float a = pow(max(dot(n,l),0.0), 0.7);
        return vec3(a, a, a);
}

vec3 calcPointLightOrSpotLight(vec3 v, vec3 n, vec4 lp, vec3 ln, float la, float fa, float is_pointlight)
{
	//get light vector
	vec3 lv = lp.xyz-v;
	
	//get distance
	float d = dot(lv,lv);
	
	float da = 0.0;

	if (d > 0.0 && la > 0.0 && fa > 0.0)
	{
		//normalize light vector
		lv = normalize(lv);
	
		//distance attenuation
		float dist2 = d/la;
		da = clamp(1.0-(dist2-1.0*(1.0-fa))/fa, 0.0, 1.0);

		// spotlight coefficient.
		float spot = max(dot(-ln, lv), is_pointlight);
		da *= spot*spot; // GL_SPOT_EXPONENT=2

		//angular attenuation
		da *= max(pow(dot(n, lv), 0.7), 0.0);		
	}

	return vec3(da,da,da);	
}

vec4 getPosition(vec2 pos_screen)
{
	float depth = texture2DRect(depthMap, pos_screen.xy).a;
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos.xyz /= pos.w;
	pos.w = 1.0;
	return pos;
}

float pcfShadow(sampler2DShadow shadowMap, vec4 stc)
{
	stc.xyz /= stc.w;
	stc.z += shadow_bias;

	stc.x = floor(stc.x*shadow_res.x + fract(stc.y*12345))/shadow_res.x; // add some chaotic jitter to X sample pos according to Y to disguise the snapping going on here
	float cs = shadow2D(shadowMap, stc.xyz).x;
	
	float shadow = cs;

        shadow += shadow2D(shadowMap, stc.xyz+vec3(2.0/shadow_res.x, 1.5/shadow_res.y, 0.0)).x;
        shadow += shadow2D(shadowMap, stc.xyz+vec3(1.0/shadow_res.x, -1.5/shadow_res.y, 0.0)).x;
        shadow += shadow2D(shadowMap, stc.xyz+vec3(-1.0/shadow_res.x, 1.5/shadow_res.y, 0.0)).x;
        shadow += shadow2D(shadowMap, stc.xyz+vec3(-2.0/shadow_res.x, -1.5/shadow_res.y, 0.0)).x;
                        
        return shadow*0.2;
}


void main() 
{
	vec2 frag = vary_fragcoord.xy/vary_fragcoord.z*0.5+0.5;
	frag *= screen_res;
	
	float shadow = 0.0;
	vec4 pos = vec4(vary_position, 1.0);
	
	vec4 diff = texture2D(diffuseMap,vary_texcoord0.xy);

	if (diff.a < minimum_alpha)
	{
		discard;
	}
	
	vec4 spos = pos;
		
	if (spos.z > -shadow_clip.w)
	{	
		vec4 lpos;
		
		vec4 near_split = shadow_clip*-0.75;
		vec4 far_split = shadow_clip*-1.25;
		vec4 transition_domain = near_split-far_split;
		float weight = 0.0;

		if (spos.z < near_split.z)
		{
			lpos = shadow_matrix[3]*spos;
			
			float w = 1.0;
			w -= max(spos.z-far_split.z, 0.0)/transition_domain.z;
			shadow += pcfShadow(shadowMap3, lpos)*w;
			weight += w;
			shadow += max((pos.z+shadow_clip.z)/(shadow_clip.z-shadow_clip.w)*2.0-1.0, 0.0);
		}

		if (spos.z < near_split.y && spos.z > far_split.z)
		{
			lpos = shadow_matrix[2]*spos;
			
			float w = 1.0;
			w -= max(spos.z-far_split.y, 0.0)/transition_domain.y;
			w -= max(near_split.z-spos.z, 0.0)/transition_domain.z;
			shadow += pcfShadow(shadowMap2, lpos)*w;
			weight += w;
		}

		if (spos.z < near_split.x && spos.z > far_split.y)
		{
			lpos = shadow_matrix[1]*spos;
			
			float w = 1.0;
			w -= max(spos.z-far_split.x, 0.0)/transition_domain.x;
			w -= max(near_split.y-spos.z, 0.0)/transition_domain.y;
			shadow += pcfShadow(shadowMap1, lpos)*w;
			weight += w;
		}

		if (spos.z > far_split.x)
		{
			lpos = shadow_matrix[0]*spos;
							
			float w = 1.0;
			w -= max(near_split.x-spos.z, 0.0)/transition_domain.x;
				
			shadow += pcfShadow(shadowMap0, lpos)*w;
			weight += w;
		}
		

		shadow /= weight;
	}
	else
	{
		shadow = 1.0;
	}
	vec3 n = vary_norm;
	vec3 l = light_position[0].xyz;
	vec3 dlight = calcDirectionalLight(n, l);
	dlight = dlight * vary_directional.rgb * vary_pointlight_col;
	
	vec4 col = vec4(vary_ambient + dlight*shadow, 1.0);
	vec4 color = diff * col;
	
	color.rgb = atmosLighting(color.rgb);

	color.rgb = scaleSoftClip(color.rgb);
	vec3 light_col = vec3(0,0,0);

  #define LIGHT_LOOP(i) \
		light_col += light_diffuse[i].rgb * calcPointLightOrSpotLight(pos.xyz, vary_norm, light_position[i], light_direction[i], light_attenuation[i].x, light_attenuation[i].y, light_attenuation[i].z);

	LIGHT_LOOP(1)
	LIGHT_LOOP(2)
	LIGHT_LOOP(3)
	LIGHT_LOOP(4)
	LIGHT_LOOP(5)
	LIGHT_LOOP(6)
	LIGHT_LOOP(7)

	color.rgb += diff.rgb * vary_pointlight_col * light_col;

	frag_color = color;
}

