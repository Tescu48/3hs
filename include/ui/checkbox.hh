
#ifndef inc_ui_checkbox_hh
#define inc_ui_checkbox_hh

#include <ui/base.hh>


namespace ui
{
	UI_SLOTS_PROTO_EXTERN(CheckBox)
	class CheckBox : public ui::BaseWidget
	{ UI_WIDGET("CheckBox")
	public:
		void setup(bool isInitialChecked = false);

		void resize(float nw, float nh);

		bool render(const ui::Keys&) override;
		float height() override;
		float width() override;

		void set_y(float y) override;
		void set_x(float x) override;

		bool status();


	private:
		UI_SLOTS_PROTO(CheckBox, 2)
		enum {
			CHECKED = 0x1,
			LOCKED  = 0x2,
		};
		float ox, oy;
		float w, h;
		u8 flags;

	};
}

#endif

