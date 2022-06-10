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

#ifndef inc_widgets_indicators_hh
#define inc_widgets_indicators_hh

#include <ui/base.hh>


namespace ui
{
	class FreeSpaceIndicator : public ui::BaseWidget
	{ UI_WIDGET("FreeSpaceIndicator")
	public:
		void setup();
		bool render(const ui::Keys& keys) override;
		float height() override { return 0.0f; }
		float width() override { return 0.0f; }
		void update();


	private:
		ui::RenderQueue queue;


	};

	class TimeIndicator : public ui::BaseWidget
	{ UI_WIDGET("TimeIndicator")
	public:
		void setup();
		bool render(const ui::Keys& keys) override;
		float height() override { return 0.0f; }
		float width() override { return 0.0f; }
		void update();

		static std::string time(time_t stamp);


	public:
		ui::ScopedWidget<ui::Text> text;
		time_t lastCheck;


	};

	UI_SLOTS_PROTO_EXTERN(BatteryIndicator_color)
	class BatteryIndicator : public ui::BaseWidget
	{ UI_WIDGET("BatteryIndicator")
	public:
		void setup();
		bool render(const ui::Keys& keys) override;
		float height() override { return 0.0f; }
		float width() override { return 0.0f; }
		void update();


	private:
		UI_SLOTS_PROTO(BatteryIndicator_color, 2)
		ui::RenderQueue queue;
		u8 level = 0;


	};

	class NetIndicator : public ui::BaseWidget
	{ UI_WIDGET("NetIndicator")
	public:
		void setup();
		bool render(const ui::Keys& keys) override;
		float height() override { return 0.0f; }
		float width() override { return 0.0f; }
		void update();


	private:
		ui::ScopedWidget<ui::Sprite> sprite;
		s8 status;


	};
}

#endif
