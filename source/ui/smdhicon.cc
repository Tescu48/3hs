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

#include <ui/smdhicon.hh>

#include "install.hh"
#include "panic.hh"
#include "i18n.hh"

UI_CTHEME_GETTER(color_border, ui::theme::smdh_icon_border_color)
UI_SLOTS(ui::SMDHIcon_color, color_border)

void ui::SMDHIcon::setup(ctr::TitleSMDH *smdh, SMDHIconType type)
{
	unsigned int dim;
	load_smdh_icon(&this->img, *smdh, type, &dim);

	this->params.pos.h = this->params.pos.w = dim;
	this->params.center.x = this->params.center.y =
		this->params.depth = this->params.angle =
		this->params.pos.x = this->params.pos.y = 0;
}

void ui::SMDHIcon::setup(u64 tid, SMDHIconType type)
{
	ctr::TitleSMDH *smdh = ctr::smdh::get(tid);
	if(smdh == nullptr) panic("Failed to load smdh.");

	unsigned int dim;
	load_smdh_icon(&this->img, *smdh, type, &dim);
	delete smdh;

	this->params.pos.h = this->params.pos.w = dim;
	this->params.depth = this->z;

	this->params.center.x = this->params.center.y =
		this->params.angle = this->params.pos.x = this->params.pos.y = 0;
}

void ui::SMDHIcon::destroy()
{
	delete_smdh_icon(this->img);
}

void ui::SMDHIcon::set_x(float x)
{
	this->params.pos.x = this->x = ui::transform(this, x);
}

void ui::SMDHIcon::set_y(float y)
{
	this->params.pos.y = this->y = ui::transform(this, y);
}

void ui::SMDHIcon::set_z(float z)
{
	this->params.depth = this->z = z;
}

void ui::SMDHIcon::set_border(bool b)
{
	this->drawBorder = b;
}

float ui::SMDHIcon::width()
{
	return this->params.pos.w;
}

float ui::SMDHIcon::height()
{
	return this->params.pos.h;
}

bool ui::SMDHIcon::render(const ui::Keys& keys)
{
	((void) keys);
	if(this->drawBorder)
	{
		C2D_DrawRectSolid(this->x - 1.0f, this->y - 1.0f, 0.0f,
			this->params.pos.w + 2.0f, this->params.pos.h + 2.0f,
			this->slots.get(0));
	}
	C2D_DrawImage(this->img, &this->params, nullptr);
	return true;
}

