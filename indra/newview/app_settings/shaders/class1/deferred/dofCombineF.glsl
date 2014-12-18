/** 
 * @file dofCombineF.glsl
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

uniform sampler2DRect diffuseRect;
uniform sampler2DRect lightMap;

uniform mat4 inv_proj;
uniform vec2 screen_res;

uniform float max_cof;
uniform float res_scale;
uniform float dof_width;
uniform float dof_height;
uniform float seconds60;

VARYING vec2 vary_fragcoord;

vec4 dofSample(sampler2DRect tex, vec2 tc)
{
	tc.x = min(tc.x, dof_width);
	tc.y = min(tc.y, dof_height);

	return texture2DRect(tex, tc);
}

float rand(vec2 co)
{
    return fract(sin(dot(co.xy ,vec2(12.9898,78.233))) * 43758.5453);
}

void main() 
{
	vec2 tc = vary_fragcoord.xy;
	
	vec4 diff = texture2DRect(lightMap, tc.xy);
	vec4 dof = dofSample(diffuseRect, tc.xy*res_scale);
	dof.a = 0.0;

	float a = min(abs(diff.a*2.0-1.0) * max_cof*res_scale, 1.0);

	// help out the transition from low-res dof buffer to full-rez full-focus buffer
	if (a > 0.25 && a < 0.75)
	{ //help out the transition a bit
		float sc = a/res_scale;
		
		vec4 col;
		col = diff;
		col.rgb += texture2DRect(lightMap, tc.xy+vec2(sc,sc)).rgb;
		col.rgb += texture2DRect(lightMap, tc.xy+vec2(-sc,sc)).rgb;
		col.rgb += texture2DRect(lightMap, tc.xy+vec2(sc,-sc)).rgb;
		col.rgb += texture2DRect(lightMap, tc.xy+vec2(-sc,-sc)).rgb;
		
		diff = mix(diff, col*0.2, a);
	}

	diff = mix(diff, dof, a);
#if USE_FILM_GRAIN
	vec3 noise_strength = 1.0 - diff.rgb;
	noise_strength *= noise_strength;
	noise_strength *= noise_strength;
	noise_strength *= noise_strength;
	//noise_strength *= noise_strength;
	//noise_strength *= noise_strength;
	vec2 s60 = vec2(seconds60);
	float rndf = rand(tc.xy + s60);
	vec3 rand3 = vec3(rndf, fract(rndf + 0.33), fract(rndf + 0.67));
	vec3 dxrndf3 = dFdx(rand3);
	vec3 dyrndf3 = dFdy(rand3);
	rand3 = (dxrndf3 + dyrndf3) * 0.25 + vec3(0.5);
	diff.rgb = diff.rgb - vec3(0.25) * (rand3 - vec3(0.40)) * noise_strength;
#endif // USE_FILM_GRAIN
	frag_color = diff;
}
