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

#include <ui/swkbd.hh>
#include <panic.hh>


void ui::AppletSwkbd::setup(std::string *ret, int maxLen, SwkbdType type,
	int numBtns)
{
	swkbdInit(&this->state, type, numBtns, maxLen);
	this->len = maxLen;
	this->ret = ret;
}

float ui::AppletSwkbd::width()
{ return 0.0f; } /* fullscreen */

float ui::AppletSwkbd::height()
{ return 0.0f; } /* fullscreen */

void ui::AppletSwkbd::hint(const std::string& h)
{ swkbdSetHintText(&this->state, h.c_str()); }

void ui::AppletSwkbd::passmode(SwkbdPasswordMode mode)
{ swkbdSetPasswordMode(&this->state, mode); }

void ui::AppletSwkbd::init_text(const std::string& t)
{ swkbdSetInitialText(&this->state, t.c_str()); }

void ui::AppletSwkbd::valid(SwkbdValidInput mode, u32 filterFlags, u32 maxDigits)
{ swkbdSetValidation(&this->state, mode, filterFlags, maxDigits); }

bool ui::AppletSwkbd::render(const ui::Keys& keys)
{
	((void) keys);

	/* why is this cast necessairy? */
	ui::RenderQueue::global()->render_and_then((std::function<bool()>) [this]() -> bool {
		char *buf = new char[this->len + 1];
		SwkbdButton btn = swkbdInputText(&this->state, buf, this->len + 1);
		*this->ret = buf;
		delete [] buf;

		if(this->resPtr != nullptr) *this->resPtr = swkbdGetResult(&this->state);
		if(this->buttonPtr != nullptr) *this->buttonPtr = btn;

		return false;
	});

	return true;
}

/* connect */

void ui::AppletSwkbd::connect(ui::AppletSwkbd::connect_type t, SwkbdButton *b)
{
	panic_assert(t == ui::AppletSwkbd::button, "EINVAL");
	this->buttonPtr = b;
}

void ui::AppletSwkbd::connect(ui::AppletSwkbd::connect_type t, SwkbdResult *r)
{
	panic_assert(t == ui::AppletSwkbd::result, "EINVAL");
	this->resPtr = r;
}

/* keyboard */

std::string ui::keyboard(std::function<void(ui::AppletSwkbd *)> configure,
	SwkbdButton *btn, SwkbdResult *res, size_t length)
{
	ui::RenderQueue queue;
	std::string ret;

	ui::AppletSwkbd *swkbd;
	ui::builder<ui::AppletSwkbd>(ui::Screen::top, &ret, length)
		.connect(ui::AppletSwkbd::button, btn)
		.connect(ui::AppletSwkbd::result, res)
		.add_to(&swkbd, queue);
	configure(swkbd);

	queue.render_finite();
	return ret;
}

/* numpad */

uint64_t ui::numpad(std::function<void(ui::AppletSwkbd *)> configure,
	SwkbdButton *btn, SwkbdResult *res, size_t length)
{
	ui::RenderQueue queue;
	std::string ret;

	ui::AppletSwkbd *swkbd;
	ui::builder<ui::AppletSwkbd>(ui::Screen::top, &ret, length, SWKBD_TYPE_NUMPAD)
		.connect(ui::AppletSwkbd::button, btn)
		.connect(ui::AppletSwkbd::result, res)
		.add_to(&swkbd, queue);
	configure(swkbd);

	queue.render_finite();
	return strtoull(ret.c_str(), nullptr, 10);
}

