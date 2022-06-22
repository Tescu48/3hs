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

#include <ui/menuselect.hh>
#include "panic.hh"

#define MAX_PER_PAGE 8
#define MIN(a,b) ((a)>(b)?(b):(a))


void ui::MenuSelect::setup()
{
	this->w = ui::screen_width(this->screen) - 10.0f;
	this->h = 20.0f;

	constexpr float y = 20.0f + (20.0f + 5.0f) * MAX_PER_PAGE;
	this->hint.setup(this->screen, STRING(hint_navigate));
	this->hint->set_x(5.0f);
	this->hint->set_y(y);
	this->hint->resize(0.4f, 0.4f);
}

void ui::MenuSelect::push_button(const std::string& label)
{
	u32 myi = this->btns.size();
	ui::Button *b = ui::builder<ui::Button>(this->screen, label)
		.connect(ui::Button::click, [this, myi]() -> bool {
			if(this->cursor_move_callback) this->cursor_move_callback();
			this->btns[this->i]->set_border(false);
			this->btns[myi]->set_border(true);
			this->i = myi;
			this->call_current();
			return true;
		})
		.size(this->w, this->h)
		.x(ui::layout::center_x)
		.finalize();
	if(myi == 0) b->set_border(true);
	if((myi % MAX_PER_PAGE) == 0) b->set_y(20.0f);
	else                          b->set_y(ui::under(this->btns.back(), b, 5.0f));
	this->btns.push_back(b);
}

void ui::MenuSelect::add_row(const std::string& s, callback_t c)
{
	panic_assert(!this->main_callback, "attempt to add callback row with main callback enabled");
	this->funcs.push_back(c);
	this->push_button(s);
}

void ui::MenuSelect::add_row(const std::string& label)
{
	panic_assert(this->main_callback, "attempt to add callback-less row without main callback");
	this->push_button(label);
}

void ui::MenuSelect::connect(connect_type t, const std::string& s, callback_t c)
{
	panic_assert(t == ui::MenuSelect::add, "expected ::add");
	this->add_row(s, c);
}

void ui::MenuSelect::connect(connect_type t, callback_t c)
{
	switch(t)
	{
	case ui::MenuSelect::on_select:
		this->main_callback = c;
		break;
	case ui::MenuSelect::on_move:
		this->cursor_move_callback = c;
		break;
	default:
		panic("EINVAL");
	}
}

float ui::MenuSelect::height()
{
	size_t s = this->btns.size();
	if(s == 0) return 0.0f;
	return s * this->h + (s - 1) * 5.0f;
}

float ui::MenuSelect::width()
{
	return this->w;
}

bool ui::MenuSelect::render(const ui::Keys& k)
{
	panic_assert(this->btns.size() != 0, "Empty menuselect");

#define MOVE(with) do { this->btns[this->i]->set_border(false); with; if(this->cursor_move_callback) this->cursor_move_callback(); this->btns[this->i]->set_border(true); } while(0)
	if((k.kDown & KEY_UP) && this->i > 0) MOVE(--this->i);
	if((k.kDown & KEY_DOWN) && this->i < this->btns.size() - 1) MOVE(++this->i);
	if(k.kDown & KEY_LEFT)
	{
		if(this->i >= MAX_PER_PAGE) MOVE(this->i -= MAX_PER_PAGE);
		else                        MOVE(this->i = 0);
	}
	if(k.kDown & KEY_RIGHT)
	{
		if(this->i + MAX_PER_PAGE < this->btns.size()) MOVE(this->i += MAX_PER_PAGE);
		else if(this->i < this->btns.size())           MOVE(this->i = this->btns.size() - 1);
	}
#undef MOVE
	if(k.kDown & KEY_A) this->call_current();

	/* aka u32 i = start_of_page */
	u32 start = this->i - (this->i % MAX_PER_PAGE);
	u32 end = MIN(start + MAX_PER_PAGE, this->btns.size());
	for(u32 i = start; i < end; ++i)
		this->btns[i]->render(k);
	this->hint->render(k);

	return true;
}

void ui::MenuSelect::call_current()
{
	ui::RenderQueue::global()->render_and_then((std::function<bool()>) [this]() -> bool {
		if(this->main_callback) return this->main_callback();
		else                    return this->funcs[this->i]();
	});
}

