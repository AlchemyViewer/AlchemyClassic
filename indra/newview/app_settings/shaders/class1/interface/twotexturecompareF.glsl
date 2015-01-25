/** 
 * @file twotexturecompareF.glsl
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

uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D dither_tex;
uniform float dither_scale;
uniform float dither_scale_s;
uniform float dither_scale_t;

VARYING vec2 vary_texcoord0;
VARYING vec2 vary_texcoord1;

void main() 
{
	frag_color = abs((texture2D(tex0, vary_texcoord0.xy) - texture2D(tex1, vary_texcoord1.xy)));

	vec2 dither_coord = vary_texcoord0 * vec2(dither_scale_s, dither_scale_t);
	vec4 dither_vec = texture2D(dither_tex, dither_coord.xy) * dither_scale;
	if(frag_color.x < dither_vec.x) frag_color.x = 0.f;
	if(frag_color.y < dither_vec.y) frag_color.y = 0.f;
	if(frag_color.z < dither_vec.z) frag_color.z = 0.f;
	if(frag_color.w < dither_vec.w) frag_color.w = 0.f;
}
