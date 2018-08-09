/* === S Y N F I G ========================================================= */
/*!    \file
**    \brief PixelFormat and conversions
**
**    $Id$
**
**    \legal
**    Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**    Copyright (c) 2007, 2008 Chris Moore
**    Copyright (c) 2012-2013 Carlos López
**    Copyright (c) 2015 Diego Barrios Romero
**
**    This package is free software; you can redistribute it and/or
**    modify it under the terms of the GNU General Public License as
**    published by the Free Software Foundation; either version 2 of
**    the License, or (at your option) any later version.
**
**    This package is distributed in the hope that it will be useful,
**    but WITHOUT ANY WARRANTY; without even the implied warranty of
**    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**    General Public License for more details.
**    \endlegal
*/
/* ========================================================================= */

#ifndef __SYNFIG_COLOR_PIXELFORMAT_H
#define __SYNFIG_COLOR_PIXELFORMAT_H

#include <synfig/color/color.h>

namespace synfig {


enum PixelFormat
{
/* Bit    Descriptions (ON/OFF)
** ----+-------------
** 0    Color Channels (Gray/RGB)
** 1    Alpha Channel (WITH/WITHOUT)
** 2    ZDepth    (WITH/WITHOUT)
** 3    Endian (BGR/RGB)
** 4    Alpha Location (Start/End)
** 5    ZDepth Location (Start/End)
** 6    Alpha/ZDepth Arrangement (ZA,AZ)
** 7    Alpha Range (Inverted,Normal)
** 8    Z Range (Inverted,Normal)
** 9    Raw Color (not conversion)
** 10   Premult Alpha
*/
    PF_RGB       = 0,
    PF_GRAY      = (1<<0), //!< If set, use one grayscale channel. If clear, use three channels for RGB
    PF_A         = (1<<1), //!< If set, include alpha channel
    PF_Z         = (1<<2), //!< If set, include ZDepth channel
    PF_BGR       = (1<<3), //!< If set, reverse the order of the RGB channels
    PF_A_START   = (1<<4), //!< If set, alpha channel is before the color data. If clear, it is after.
    PF_Z_START   = (1<<5), //!< If set, ZDepth channel is before the color data. If clear, it is after.
    PF_ZA        = (1<<6), //!< If set, the ZDepth channel will be in front of the alpha channel. If clear, they are reversed.
    PF_A_INV     = (1<<7), //!< If set, the alpha channel is stored as 1.0-a
    PF_Z_INV     = (1<<8), //!< If set, the ZDepth channel is stored as 1.0-z
    PF_RAW_COLOR = (1<<9)+(1<<1), //!< If set, the data represents a raw Color data structure, and all other bits are ignored.
    PF_A_PREMULT = (1<<10) //!< If set, the encoded color channels are alpha-premulted
};

inline PixelFormat operator|(PixelFormat lhs, PixelFormat rhs)
    { return static_cast<PixelFormat>((int)lhs|(int)rhs); }

inline PixelFormat operator&(PixelFormat lhs, PixelFormat rhs)
    { return static_cast<PixelFormat>((int)lhs&(int)rhs); }
#define FLAGS(x,y)        (((x)&(y))==(y))

//! Returns the number of channels that the given PixelFormat calls for
inline int
channels(const PixelFormat x)
{
    if (FLAGS(x, PF_RAW_COLOR))
    	return sizeof(Color);

    int chan = FLAGS(x, PF_GRAY) ? 1 : 3;
    if (FLAGS(x, PF_A)) ++chan;
    if (FLAGS(x, PF_Z)) ++chan;
    return chan;
}

inline unsigned char*
Color2PixelFormat(
	const Color &color,
	const PixelFormat &pf,
	unsigned char *out,
	const Gamma &gamma = Gamma::no_gamma )
{
	static int yuv_r = (int)(EncodeYUV[0][0]*256.f);
	static int yuv_g = (int)(EncodeYUV[0][1]*256.f);
	static int yuv_b = 256 - yuv_r - yuv_g;

	if(FLAGS(pf, PF_RAW_COLOR)) {
		// just copy raw color data
		*reinterpret_cast<Color*>(out) = color;
		return out + sizeof(color);
	}

	// get alpha value
	ColorReal a = std::max(ColorReal(0.0), std::min(ColorReal(1.0), color.get_a()));
	unsigned char ac = (unsigned char)(a*ColorReal(255.9));
	if (FLAGS(pf, PF_A_INV)) ac = 255 - ac;

	// get color values
	int ri = gamma.r_F32_to_U16(color.get_r());
	int gi = gamma.g_F32_to_U16(color.get_g());
	int bi = gamma.b_F32_to_U16(color.get_b());
	if (FLAGS(pf, PF_A_PREMULT)) {
		int ai = ((int)ac) + 1;
		ri = (ri*ai) >> 8;
		gi = (gi*ai) >> 8;
		bi = (bi*ai) >> 8;
	}

	// put alpha and z-depth before color channels if need
	if (FLAGS(pf, PF_ZA)) {
		if (FLAGS(pf, PF_Z_START)) ++out;
		if (FLAGS(pf, PF_A_START)) *out++ = ac;
	} else {
		if(FLAGS(pf, PF_A_START)) *out++ = ac;
		if(FLAGS(pf, PF_Z_START)) ++out;
	}

	// put color channels
	if (FLAGS(pf, PF_GRAY)) {
		*out++ = (unsigned char)((ri*yuv_r + gi*yuv_g + bi*yuv_b) >> 16);
	} else
	if (FLAGS(pf, PF_BGR)) {
		*out++ = (unsigned char)(bi >> 8);
		*out++ = (unsigned char)(gi >> 8);
		*out++ = (unsigned char)(ri >> 8);
	} else {
		*out++ = (unsigned char)(ri >> 8);
		*out++ = (unsigned char)(gi >> 8);
		*out++ = (unsigned char)(bi >> 8);
	}

	// put alpha and z-depth after color channels if need
	if (FLAGS(pf, PF_ZA)) {
		if (!FLAGS(pf, PF_Z_START) && FLAGS(pf, PF_Z)) ++out;
		if (!FLAGS(pf, PF_A_START) && FLAGS(pf, PF_A)) *out++ = ac;
	} else {
		if (!FLAGS(pf, PF_A_START) && FLAGS(pf, PF_A)) *out++ = ac;
		if (!FLAGS(pf, PF_Z_START) && FLAGS(pf, PF_Z)) ++out;
	}

	return out;
}

inline unsigned char*
convert_color_format(
	unsigned char *dest,
	const Color *src,
	int w,
	PixelFormat pf,
	const Gamma &gamma )
{
	assert(w >= 0);
	while (w--)
		dest = Color2PixelFormat((*(src++)).clamped(), pf, dest, gamma);
	return dest;
}

inline const unsigned char*
PixelFormat2Color(
	Color &color,
	const PixelFormat &pf,
	const unsigned char *in )
{
	const ColorReal k(1.0/255.0);

	if(FLAGS(pf, PF_RAW_COLOR)) {
		// just copy raw color data
		color = *reinterpret_cast<const Color*>(in);
		return in + sizeof(color);
	}

	// read alpha and z-depth at begin if need
	if (FLAGS(pf, PF_ZA)) {
		if (FLAGS(pf, PF_Z_START)) ++in;
		if (FLAGS(pf, PF_A_START)) color.set_a(k*ColorReal(*in++));
	} else {
		if (FLAGS(pf, PF_A_START)) color.set_a(k*ColorReal(*in++));
		if (FLAGS(pf, PF_Z_START)) ++in;
	}

	// read color channels
	if (FLAGS(pf, PF_GRAY)) {
		color.set_yuv(k*ColorReal(*in++), 0, 0);
	} else
	if(FLAGS(pf, PF_BGR)) {
		color.set_b(k*ColorReal(*in++));
		color.set_g(k*ColorReal(*in++));
		color.set_r(k*ColorReal(*in++));
	} else {
		color.set_r(k*ColorReal(*in++));
		color.set_g(k*ColorReal(*in++));
		color.set_b(k*ColorReal(*in++));
	}

	// read alpha and z-depth at end if need
	if (FLAGS(pf, PF_ZA)) {
		if (!FLAGS(pf, PF_Z_START) && FLAGS(pf, PF_Z)) ++in;
		if (!FLAGS(pf, PF_A_START) && FLAGS(pf, PF_A)) color.set_a(k*(float)(*in++));
	} else {
		if (!FLAGS(pf, PF_A_START) && FLAGS(pf, PF_A)) color.set_a(k*(float)(*in++));
		if (!FLAGS(pf, PF_Z_START) && FLAGS(pf, PF_Z)) ++in;
	}

	// invert alpha
	if (FLAGS(pf, PF_A_INV))
		color.set_a(ColorReal(1.0) - color.get_a());

	// demult alpha
	if (FLAGS(pf, PF_A_PREMULT))
		color = color.demult_alpha();

	return in;
}

} // synfig namespace

#endif // __SYNFIG_COLOR_PIXELFORMAT_H

