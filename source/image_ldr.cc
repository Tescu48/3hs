/* This file is part of 3hs
 * Copyright (C) 2021-2022 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

// To load C2D_Image's

#include "image_ldr.hh"
#include "panic.hh"
#include "i18n.hh"

#include <citro3d.h>
#include <citro2d.h>
#include <3ds.h>


static u32 next_pow2(u32 i)
{
	--i;
	i |= i >> 1;
	i |= i >> 2;
	i |= i >> 4;
	i |= i >> 8;
	i |= i >> 16;
	++i;

	return i;
}

void load_smdh_icon(C2D_Image *ret, const ctr::TitleSMDH& smdh, SMDHIconType type,
	unsigned int *chosenDimensions)
{
	unsigned int dim; // x = y
	u16 *src;

	switch(type)
	{
	case SMDHIconType::large:
		src = (u16 *) smdh.iconLarge;
		dim = 48;
		break;
	case SMDHIconType::small:
		src = (u16 *) smdh.iconSmall;
		dim = 24;
		break;

	default:
		panic("EINVAL");
		return;
	}

	unsigned int dim2 = next_pow2(dim);
	if(chosenDimensions != nullptr)
		*chosenDimensions = dim2;

	// subtex
	Tex3DS_SubTexture *subtex = new Tex3DS_SubTexture;
	subtex->width = subtex->height = dim;
	subtex->right = (float) dim / dim2;
	subtex->bottom = subtex->left = 0;
	subtex->top = (float) dim / dim2;

	// tex
	C3D_Tex *tex = new C3D_Tex;
	if(!C3D_TexInit(tex, dim2, dim2, GPU_RGB565))
		panic("failed to create a C3D texture");

	u16 *dst = (u16 *) tex->data + (dim2 - dim) * dim2;

	for(size_t i = 0; i < dim; i += 8)
	{
		memcpy(dst, src, dim * 8 * sizeof(u16));
		dst += dim2 * 8;
		src += dim * 8;
	}

	C3D_TexFlush(tex);

	// fill in data
	ret->subtex = subtex;
	ret->tex = tex;
}

void rgba_to_abgr(u32 *data, u16 w, u16 h)
{
	u32 size = w * h;
	for(u32 i = 0; i < size; ++i)
		data[i] = __builtin_bswap32(data[i]);
}

void load_rgba8(C2D_Image *image, u32 *data, u16 w, u16 h, bool allocStructs)
{
	Tex3DS_SubTexture *subtex;
	C3D_Tex *tex;
	if(allocStructs)
	{
		subtex = new Tex3DS_SubTexture;
		tex = new C3D_Tex;
	}
	else
	{
		/* we want to modify not have a const ptr... */
		subtex = (Tex3DS_SubTexture *) image->subtex;
		tex = image->tex;
	}

	u32 w_pow2 = next_pow2(w);
	u32 h_pow2 = next_pow2(h);

	subtex->width = w;
	subtex->height = h;
	subtex->left = 0.0f;
	subtex->top = 1.0f;
	subtex->right = (w / (float) w_pow2);
	subtex->bottom = 1.0 - (h / (float) h_pow2);

	panic_assert(C3D_TexInit(tex, w_pow2, h_pow2, GPU_RGBA8), "failed to load C3D texture");
	u32 *dst = (u32 *) tex->data;

	for(u16 x = 0; x < w; x++)
		for(u16 y = 0; y < h; y++)
		{
			u32 dst_pos = ((((y >> 3) * (w_pow2 >> 3) + (x >> 3)) << 6) + ((x & 1) | ((y & 1) << 1) | ((x & 2) << 1) | ((y & 2) << 2) | ((x & 4) << 2) | ((y & 4) << 3)));
			memcpy(&dst[dst_pos], &data[y * w + x], 4);
		}

	C3D_TexFlush(tex);

	image->subtex = subtex;
	image->tex = tex;
}

void delete_image(C2D_Image icon)
{
	C3D_TexDelete(icon.tex);
	delete icon.subtex;
	delete icon.tex;
}

void delete_image_data(C2D_Image icon)
{
	C3D_TexDelete(icon.tex);
}

