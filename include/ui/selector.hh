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

#ifndef inc_ui_selector_hh
#define inc_ui_selector_hh

#include <ui/base.hh>

#include <string>
#include <vector>


namespace ui
{
	namespace constants
	{
		constexpr float SEL_LABEL_HEIGHT = 30.0f;
		constexpr float SEL_LABEL_WIDTH = 200.0f;
		constexpr float SEL_TRI_WIDTH = 30.0f;
		constexpr float SEL_FONTSIZ = 0.65f;
		constexpr float SEL_TRI_PAD = 5.0f;
	}

	template <typename TEnum>
	class Selector : public ui::BaseWidget
	{ UI_WIDGET("Selector")
	public:
		void setup(const std::vector<std::string>& labels, const std::vector<TEnum>& values, TEnum *res = nullptr)
		{
			this->values = &values;
			this->res = res;

			this->buf = C2D_TextBufNew(this->accumul_size(labels));

			for(const std::string& label : labels)
			{
				C2D_Text text;
				C2D_TextParse(&text, this->buf, label.c_str());
				C2D_TextOptimize(&text);
				this->labels.push_back(text);
			}

			using namespace constants;
			this->x = (ui::screen_width(this->screen) - SEL_LABEL_WIDTH) / 2;
			this->y = (ui::dimensions::height - SEL_LABEL_HEIGHT) / 2;
			this->assign_txty();

			static ui::slot_color_getter getters[] = {
				color_button, color_text
			};
			this->slots = ui::ThemeManager::global()->get_slots(this, "Selector", 2, getters);
		}

		void set_x(float) override { } /* stub */
		void set_y(float) override { } /* stub */

		void destroy() override
		{
			C2D_TextBufClear(this->buf);
		}

		bool render(const ui::Keys& keys) override
		{
			using namespace constants;

			// The following maths is very long and cursed
			// So it is recommended to do something else

			// Draw box for label..
			C2D_DrawRectSolid(this->x, this->y, this->z, SEL_LABEL_WIDTH, SEL_LABEL_HEIGHT, this->slots.get(0));

			// Draw triangles for next/prev...
#define TRI() C2D_DrawTriangle(coords[0], coords[1], this->slots.get(0), coords[2], coords[3], this->slots.get(0), \
			                         coords[4], coords[5], this->slots.get(0), this->z)
			u32 kdown = keys.kDown;
			/*     B
			 *    /|
			 * A / |
			 *   \ |
			 *    \|
			 *     C
			 */
			float coords[6] = {
				this->x - SEL_TRI_WIDTH, (SEL_LABEL_HEIGHT / 2) + this->y, // A
				this->x - SEL_TRI_PAD, this->y,                            // B
				this->x - SEL_TRI_PAD, this->y + SEL_LABEL_HEIGHT          // C
			};
			TRI(); // prev
			if((keys.kDown & KEY_TOUCH) && keys.touch.px < coords[2])
				kdown |= KEY_LEFT;
			coords[0] += SEL_LABEL_WIDTH + 2*SEL_TRI_WIDTH;
			coords[2] += SEL_LABEL_WIDTH + 2*SEL_TRI_PAD;
			coords[4] += SEL_LABEL_WIDTH + 2*SEL_TRI_PAD;
			TRI(); // right
			if((keys.kDown & KEY_TOUCH) && keys.touch.px > coords[2])
				kdown |= KEY_RIGHT;
#undef TRI

			// Draw label...
			C2D_DrawText(&this->labels[this->idx], C2D_WithColor, this->tx, this->ty, this->z, SEL_FONTSIZ, SEL_FONTSIZ, this->slots.get(1));

			// Take input...
			if(keys.kDown & KEY_A)
			{
				if(this->res != nullptr)
					*this->res = (*this->values)[this->idx];
				return false;
			}

			if(kdown & (KEY_LEFT | KEY_L))
				this->wrap_minus();

			if(kdown & (KEY_RIGHT | KEY_R))
				this->wrap_plus();

			if(keys.kDown & KEY_B)
				return false;

			return true;
		}

		void search_set_idx(TEnum value)
		{
			for(size_t i = 0; i < this->values->size(); ++i)
			{
				if((*this->values)[i] == value)
				{
					if(this->res != nullptr)
						*this->res = value;
					this->idx = i;
					this->assign_txty();
					break;
				}
			}
		}

		float width()
		{
			using namespace constants;
			return (SEL_TRI_WIDTH * 2) + (SEL_TRI_PAD * 2) + SEL_LABEL_WIDTH;
		}

		float height()
		{
			using namespace constants;
			return SEL_LABEL_HEIGHT;
		}


	private:
		ui::SlotManager slots { nullptr };

		const std::vector<TEnum> *values;
		std::vector<C2D_Text> labels;
		TEnum *res = nullptr;
		C2D_TextBuf buf;
		size_t idx = 0;

		float tx, ty;

		void assign_txty()
		{
			using namespace constants;
			float h, w;
			C2D_TextGetDimensions(&this->labels[this->idx], SEL_FONTSIZ, SEL_FONTSIZ, &w, &h);
			this->tx = (SEL_LABEL_WIDTH / 2) - (w / 2) + this->x;
			this->ty = (SEL_LABEL_HEIGHT / 2) - (h / 2) + this->y;
		}

		size_t accumul_size(const std::vector<std::string>& labels)
		{
			size_t ret = 0;
			for(const std::string& label : labels)
			{ ret += label.size() + 1; }
			return ret;
		}

		void wrap_minus()
		{
			if(this->idx > 0) --this->idx;
			else this->idx = this->labels.size() - 1;
			this->assign_txty();
		}

		void wrap_plus()
		{
			if(this->idx < this->labels.size() - 1) ++this->idx;
			else this->idx = 0;
			this->assign_txty();
		}

		UI_CTHEME_GETTER(color_button, ui::theme::button_background_color)
		UI_CTHEME_GETTER(color_text, ui::theme::text_color)

	};
}

#endif

