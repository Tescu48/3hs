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

#ifndef inc_ui_theme_hh
#define inc_ui_theme_hh

#include <unordered_map>
#include <3ds/types.h>
#include <citro2d.h>
#include <stddef.h>
#include <string>
#include <vector>

#define UI_SLOTS_PROTO_EXTERN(name) \
	extern ui::slot_color_getter *name##__do_not_touch;
#define UI_SLOTS_PROTO(name, count) \
	ui::SlotManager slots = ui::ThemeManager::global()->get_slots(this, id /* id is defined by UI_WIDGET */, count, name##__do_not_touch);
#define UI_SLOTS__3(line, name, ...) \
	static ui::slot_color_getter _slot_data__do_not_touch##line[] = { __VA_ARGS__ }; \
	ui::slot_color_getter *name##__do_not_touch = _slot_data__do_not_touch##line;
#define UI_SLOTS__2(line, name, ...) \
	UI_SLOTS__3(line, name, __VA_ARGS__)
#define UI_SLOTS(name, ...) \
	UI_SLOTS__2(__LINE__, name, __VA_ARGS__)
#define UI_CTHEME_GETTER(symbol, id) \
	static u32 symbol() { return *ui::Theme::global()->get_color(id); }


namespace ui
{
	typedef u32 (*slot_color_getter)();
	class BaseWidget;

	typedef C2D_Image ThemeDescriptorImage;
	typedef u32 ThemeDescriptorColor;

	namespace theme
	{
		enum _enum {
			background_color,
			text_color,
			button_background_color,
			button_border_color,
			battery_green_color,
			battery_red_color,
			toggle_green_color,
			toggle_red_color,
			toggle_slider_color,
			progress_bar_foreground_color,
			progress_bar_background_color,
			scrollbar_color,
			led_green_color,
			led_red_color,
			smdh_icon_border_color,
			/* don't forget to update max_color when adding a color! */
			more_image,
			battery_image,
			search_image,
			settings_image,
			spinner_image,
			random_image,
			max,
		};
		constexpr u32 max_color = smdh_icon_border_color;
	}

	class Theme
	{
	public:
		Theme() { memset(this->descriptors, 0, sizeof(this->descriptors)); }
		~Theme() { this->cleanup_images(); }
		constexpr ThemeDescriptorColor *get_color(u32 descriptor_id)
		{ return &this->descriptors[descriptor_id].color; }
		constexpr ThemeDescriptorImage *get_image(u32 descriptor_id)
		{ return &this->descriptors[descriptor_id].image; }

		static Theme *global();

		std::string author;
		std::string name;

		bool parse(const char *filename);

	private:
		union data_union {
			ThemeDescriptorColor color;
			ThemeDescriptorImage image;
		} descriptors[theme::max];
		void cleanup_images();
	};

	class SlotManager
	{
	public:
		SlotManager(const u32 *colors)
			: colors(colors) { }
		/* no bounds checking! */
		constexpr u32 get(size_t i) { return this->colors[i]; }
		constexpr bool is_initialized() { return this->colors != nullptr; }
	private:
		const u32 *colors;
	};


	class ThemeManager
	{
	public:
		ui::SlotManager get_slots(BaseWidget *that, const char *id, size_t count, const slot_color_getter *getters);
		void reget(const char *id);
		void reget();

		static ui::ThemeManager *global();

		~ThemeManager();


	public:
		struct slot_data {
			const slot_color_getter *getters; /* of size len */
			u32 *colors; /* of size len */
			size_t len;
		};
		std::unordered_map<std::string, slot_data> slots;


	};
}

#endif

