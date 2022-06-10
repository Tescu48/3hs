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

#ifndef inc_ui_meta_hh
#define inc_ui_meta_hh

#include <ui/base.hh>
#include "hsapi.hh"


namespace ui
{
	class CatMeta : public ui::BaseWidget
	{ UI_WIDGET("CatMeta")
	public:
		void setup(const hsapi::Category& cat);

		void set_cat(const hsapi::Category& cat);

		float get_x() override;
		float get_y() override;

		bool render(const ui::Keys& keys) override;
		float height() override;
		float width() override;


	private:
		ui::RenderQueue queue;


	};

	class SubMeta : public ui::BaseWidget
	{ UI_WIDGET("SubMeta")
	public:
		void setup(const hsapi::Subcategory& sub);

		void set_sub(const hsapi::Subcategory& sub);

		float get_x() override;
		float get_y() override;

		bool render(const ui::Keys& keys) override;
		float height() override;
		float width() override;


	private:
		ui::RenderQueue queue;


	};

	class TitleMeta : public ui::BaseWidget
	{ UI_WIDGET("TitleMeta")
	public:
		void setup(const hsapi::Title& meta);

		void set_title(const hsapi::Title& meta);

		float get_x() override;
		float get_y() override;

		bool render(const ui::Keys& keys) override;
		float height() override;
		float width() override;


	private:
		ui::RenderQueue queue;


	};
}

#endif

