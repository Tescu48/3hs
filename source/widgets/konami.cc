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

#include "widgets/konami.hh"
#include "i18n.hh"


#define KONCODE_SIZE (sizeof(konKeys) / sizeof(u32))
static const u32 konKeys[] = {
	KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN,
	KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT,
	KEY_B, KEY_A,
};


bool ui::KonamiListner::render(const ui::Keys& keys)
{
	if(this->currKey == KONCODE_SIZE)
	{
		ui::RenderQueue::global()->render_and_then([this]() -> void {
			this->show_bunny();
		});
		this->currKey = 0;
	}
	else if(keys.kDown & konKeys[this->currKey])
		++this->currKey;
	else if(keys.kDown != 0)
		this->currKey = 0;

	return true;
}

void ui::KonamiListner::show_bunny()
{
	ui::RenderQueue queue;

	ui::builder<ui::Sprite>(ui::Screen::top, ui::Sprite::spritesheet, (u32) ui::sprite::bun)
		.x(ui::layout::center_x).y(ui::layout::center_y)
		.add_to(queue);

	ui::builder<ui::Text>(ui::Screen::bottom, STRING(hs_bunny_found))
		.x(ui::layout::center_x).y(ui::layout::center_y)
		.wrap().add_to(queue);

	queue.render_finite_button(KEY_A | KEY_START | KEY_B);
}

