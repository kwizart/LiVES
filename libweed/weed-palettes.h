/* WEED is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   Weed is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this source code; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA

   Weed is developed by:
   Gabriel "Salsaman" Finch - http://lives-video.com

   partly based on LiViDO, which is developed by:
   Niels Elburg - http://veejay.sf.net
   Denis "Jaromil" Rojo - http://freej.dyne.org
   Tom Schouten - http://zwizwa.fartit.com
   Andraz Tori - http://cvs.cinelerra.org

   reviewed with suggestions and contributions from:
   Silvano "Kysucix" Galliani - http://freej.dyne.org
   Kentaro Fukuchi - http://megaui.net/fukuchi
   Jun Iio - http://www.malib.net
   Carlo Prelz - http://www2.fluido.as:8080/
*/

/* (C) G. Finch, 2005 - 2019 */

#ifndef __WEED_PALETTES_H__
#define __WEED_PALETTES_H__

/* Palette types */
/* RGB palettes */
#define WEED_PALETTE_NONE 0
#define WEED_PALETTE_END WEED_PALETTE_NONE
#define WEED_PALETTE_RGB888 1
#define WEED_PALETTE_RGB24 1
#define WEED_PALETTE_BGR888 2
#define WEED_PALETTE_BGR24 2
#define WEED_PALETTE_RGBA8888 3
#define WEED_PALETTE_RGBA32 3
#define WEED_PALETTE_ARGB8888 4
#define WEED_PALETTE_ARGB32 4
#define WEED_PALETTE_RGBFLOAT 5
#define WEED_PALETTE_RGBAFLOAT  6
#define WEED_PALETTE_BGRA8888 7
#define WEED_PALETTE_BGRA32 7

/* YUV palettes */
#define WEED_PALETTE_YUV422P 513
#define WEED_PALETTE_YV16 513
#define WEED_PALETTE_YUV420P 514
#define WEED_PALETTE_YV12 514
#define WEED_PALETTE_YVU420P 515
#define WEED_PALETTE_I420 515
#define WEED_PALETTE_IYUV 515
#define WEED_PALETTE_YUV444P 516
#define WEED_PALETTE_YUVA4444P 517
#define WEED_PALETTE_YUYV8888 518
#define WEED_PALETTE_YUYV 518
#define WEED_PALETTE_YUY2 518
#define WEED_PALETTE_UYVY8888 519
#define WEED_PALETTE_UYVY 519
#define WEED_PALETTE_YUV411 520
#define WEED_PALETTE_IYU1 520
#define WEED_PALETTE_YUV888 521
#define WEED_PALETTE_IYU2 521
#define WEED_PALETTE_YUVA8888 522

/* Alpha palettes */
#define WEED_PALETTE_A1 1025
#define WEED_PALETTE_A8 1026
#define WEED_PALETTE_AFLOAT 1027

/* YUV sampling types */
// see http://www.mir.com/DMG/chroma.html
#define WEED_YUV_SAMPLING_DEFAULT   0
#define WEED_YUV_SAMPLING_JPEG  0   ///< jpeg/mpeg1 - samples centered horizontally: 0.5, 2.5 etc.
#define WEED_YUV_SAMPLING_MPEG   1 ///< mpeg2 - samples aligned horizontally: left 0,2,4 etc;
#define WEED_YUV_SAMPLING_DVPAL  2 ///< separated Cb and Cr
#define WEED_YUV_SAMPLING_DVNTSC  3 ///< not used - only for 411 planar

/* YUV clamping types */
#define WEED_YUV_CLAMPING_CLAMPED 0
#define WEED_YUV_CLAMPING_MPEG WEED_YUV_CLAMPING_CLAMPED
#define WEED_YUV_CLAMPING_UNCLAMPED 1
#define WEED_YUV_CLAMPING_JPEG WEED_YUV_CLAMPING_UNCLAMPED

/* YUV subspace types */
#define WEED_YUV_SUBSPACE_YUV 0
#define WEED_YUV_SUBSPACE_YCBCR 1
#define WEED_YUV_SUBSPACE_BT709 2
#define WEED_YUV_SUBSPACE_ITU709 WEED_YUV_SUBSPACE_BT709

/* GAMMA Values */
/* API version 200 */
#define WEED_GAMMA_UNKNOWN 0
#define WEED_GAMMA_LINEAR -1
#define WEED_GAMMA_SRGB 1

#define WEED_GAMMA_BT709 512

#endif
