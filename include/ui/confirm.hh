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

#ifndef inc_ui_confirm_hh
#define inc_ui_confirm_hh

#include <ui/base.hh>

#include <string>


namespace ui
{
	class Confirm : public ui::BaseWidget
	{ UI_WIDGET("Confirm")
	public:
		void setup(const std::string& label, bool& ret);

		bool render(const ui::Keys&) override;
		float height() override;
		float width() override;

		void set_y(float y) override;

		static bool exec(const std::string& label, const std::string& label_top = "",
			bool default_val = false);


	private:
		ui::RenderQueue queue;
		bool *ret;

		void adjust();


	};
}

#endif

