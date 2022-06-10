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

#ifndef inc_ui_menuselect_hh
#define inc_ui_menuselect_hh

#include <ui/base.hh>
#include <functional>
#include <string>
#include <vector>


namespace ui
{
	class MenuSelect : public ui::BaseWidget
	{ UI_WIDGET("MenuSelect")
	public:
		using callback_t = std::function<void()>;

		void setup();
		bool render(const ui::Keys&) override;
		float height() override;
		float width() override;

		enum connect_type { add };
		void connect(connect_type, const std::string&, callback_t);

		void add_row(const std::string& label, callback_t callback);


	private:
		void call_current();

		std::vector<ui::Button *> btns;
		std::vector<callback_t> funcs;
		ui::RenderQueue q;
		float w, h;
		u32 i = 0;


	};
}

#endif

