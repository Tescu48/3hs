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

#include "about.hh"

#include <ui/base.hh>

#include "update.hh" // VERSION
#include "i18n.hh"
#include "util.hh"


void show_about()
{
	ui::RenderQueue queue;

	ui::builder<ui::Text>(ui::Screen::top, STRING(credits_thanks))
		.x(ui::layout::center_x)
		.y(ui::layout::base)
		.wrap()
		.add_to(queue);
	ui::Sprite *sprite;
	ui::builder<ui::Sprite>(ui::Screen::top, ui::Sprite::spritesheet, (u32) ui::sprite::logo)
		.under(queue.back())
		.add_to(&sprite, queue);
	ui::builder<ui::Sprite>(ui::Screen::top, ui::Sprite::spritesheet, (u32) ui::sprite::gplv3)
		.align_y_center(queue.back())
		.next_center(queue.back(), 15.0f)
		.add_to(queue);
	ui::builder<ui::Text>(ui::Screen::top, PSTRING(this_version, VERSION " \"" VERSION_DESC "\"" ))
		.x(ui::layout::center_x)
		.under(sprite)
		.wrap()
		.add_to(queue);

	bool focus = set_focus(true);
	queue.render_finite_button(KEY_B | KEY_A | KEY_START);
	set_focus(focus);
}

