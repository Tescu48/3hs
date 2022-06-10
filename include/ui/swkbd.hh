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

#ifndef inc_ui_swkbd_hh
#define inc_ui_swkbd_hh

#include <ui/base.hh>
#include <3ds.h>

#include <functional>
#include <string>


namespace ui
{
	class AppletSwkbd : public ui::BaseWidget
	{ UI_WIDGET("AppletSwkbd")
	public:
		void setup(std::string *ret, int maxLen = 200, SwkbdType type = SWKBD_TYPE_NORMAL,
			int numBtns = 2);

		bool render(const ui::Keys&) override;
		float height() override;
		float width() override;

		enum connect_type { button, result };

		void connect(connect_type, SwkbdButton *b); /* button */
		void connect(connect_type, SwkbdResult *r); /* result */

		/* set the hint text, the text displayed transparantly
		 * if you don't have any text entered */
		void hint(const std::string& h);
		/* Sets the password mode, see enum SwkbdPasswordMode for possible values */
		void passmode(SwkbdPasswordMode mode);
		/* Sets the initial search text */
		void init_text(const std::string& t);
		/* Sets the valid input mode,
		 * filterflags is a bitfield of SWKBD_FILTER_* */
		void valid(SwkbdValidInput mode, u32 filterFlags = 0, u32 maxDigits = 0);


	private:
		SwkbdButton *buttonPtr = nullptr;
		SwkbdResult *resPtr = nullptr;
		SwkbdState state;
		std::string *ret;
		size_t len;


	};

	/* Gets string input with ui::AppletSwkbd */
	std::string keyboard(std::function<void(ui::AppletSwkbd *)> configure,
		SwkbdButton *btn = nullptr, SwkbdResult *res = nullptr, size_t length = 200);
	/* Gets a number with ui::AppletSwkbd */
	uint64_t numpad(std::function<void(ui::AppletSwkbd *)> configure,
		SwkbdButton *btn = nullptr, SwkbdResult *res = nullptr, size_t length = 10);

/*	class Swkbd
	{

	};*/
}

#endif

