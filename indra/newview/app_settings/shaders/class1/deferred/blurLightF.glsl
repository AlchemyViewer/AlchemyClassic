/** 
 * @file blurLightF.glsl
 *
 * $LicenseInfo:firstyear=2007&license=viewerlgpl$
 * Second Life Viewer Source Code
 * Copyright (C) 2007, Linden Research, Inc.
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



#ifdef DEFINE_GL_FRAGCOLOR
out vec4 frag_color;
#else
#define frag_color gl_FragColor
#endif

uniform sampler2DRect depthMap;
uniform sampler2DRect normalMap;
uniform sampler2DRect lightMap;

uniform float dist_factor;
uniform float blur_size;
uniform vec2 delta;
uniform vec3 kern[4];
uniform float kern_scale;

VARYING vec2 vary_fragcoord;

uniform mat4 inv_proj;
uniform vec2 screen_res;

vec4 getPosition(vec2 pos_screen)
{
	float depth = texture2DRect(depthMap, pos_screen.xy).r;
	vec2 sc = pos_screen.xy*2.0;
	sc /= screen_res;
	sc -= vec2(1.0,1.0);
	vec4 ndc = vec4(sc.x, sc.y, 2.0*depth-1.0, 1.0);
	vec4 pos = inv_proj * ndc;
	pos /= pos.w;
	pos.w = 1.0;
	return pos;
}

vec2 encode_normal(vec3 n)
{
	float f = sqrt(8 * n.z + 8);
	return n.xy / f + 0.5;
}

vec3 decode_normal (vec2 enc)
{
    vec2 fenc = enc*4-2;
    float f = dot(fenc,fenc);
    float g = sqrt(1-f/4);
    vec3 n;
    n.xy = fenc*g;
    n.z = 1-f/2;
    return n;
}

void main() 
{
    vec2 tc = vary_fragcoord.xy;
	vec3 norm = texture2DRect(normalMap, tc).xyz;
	norm = decode_normal(norm.xy); // unpack norm

	vec3 pos = getPosition(tc).xyz;
	vec4 ccol = texture2DRect(lightMap, tc).rgba;
	
	vec2 dlt = kern_scale * delta / (vec2(1.0)+norm.xy*norm.xy);
	dlt /= max(-pos.z*dist_factor, 1.0);
	
	vec2 defined_weight = kern[0].xy; // special case the first (centre) sample's weight in the blur; we have to sample it anyway so we get it for 'free'
	vec4 col = defined_weight.xyxx * ccol;

	// relax tolerance according to distance to avoid speckling artifacts, as angles and distances are a lot more abrupt within a small screen area at larger distances
	float pointplanedist_tolerance_pow2 = pos.z*-0.001;

	for (int i = 3; i > 0; i--)
	{
		vec2 samptc = tc + kern[i].z*dlt;
	    vec3 samppos = getPosition(samptc).xyz; 

		float d = dot(norm.xyz, samppos.xyz-pos.xyz);// dist from plane
		
		if (d*d <= pointplanedist_tolerance_pow2)
		{
			col += texture2DRect(lightMap, samptc)*kern[i].xyxx;
			defined_weight += kern[i].xy;
		}
	}

	for (int i = 1; i < 4; i++)
	{
		vec2 samptc = tc - kern[i].z*dlt;
	    vec3 samppos = getPosition(samptc).xyz; 

		float d = dot(norm.xyz, samppos.xyz-pos.xyz);// dist from plane
		
		if (d*d <= pointplanedist_tolerance_pow2)
		{
			col += texture2DRect(lightMap, samptc)*kern[i].xyxx;
			defined_weight += kern[i].xy;
		}
	}

	col /= defined_weight.xyxx;
	col.y *= col.y; // delinearize SSAO effect post-blur
	
	frag_color = col;
}

