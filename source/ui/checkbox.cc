
#include <ui/checkbox.hh>

UI_CTHEME_GETTER(color_border, ui::theme::checkbox_border_color)
UI_CTHEME_GETTER(color_check, ui::theme::checkbox_check_color)
UI_SLOTS(ui::CheckBox, color_border, color_check)

#define BORDER_THICKNESS 1.0f/*px*/
#define BORDER_THICKNESS2 (BORDER_THICKNESS*2)


void ui::CheckBox::setup(bool isInitialChecked)
{
	this->flags = isInitialChecked ? ui::CheckBox::CHECKED : 0;
	this->w = this->h = 10.0f;
}

void ui::CheckBox::resize(float nw, float nh)
{
	this->ox = this->x + nw;
	this->oy = this->y + nh;
	this->w = nw;
	this->h = nh;
}

void ui::CheckBox::set_y(float y)
{
	this->y = y;
	this->oy = y + this->h;
}

void ui::CheckBox::set_x(float x)
{
	this->x = x;
	this->ox = x + this->w;
}

bool ui::CheckBox::render(const ui::Keys& keys)
{
	/* the checkbox may get pressed here */
	if(keys.touch.px >= this->x && keys.touch.px <= this->ox &&
			keys.touch.py >= this->y && keys.touch.py <= this->oy)
	{
		if(!(this->flags & ui::CheckBox::LOCKED))
		{
			this->flags |= ui::CheckBox::LOCKED;
			this->flags ^= ui::CheckBox::CHECKED;
		}
	}
	else
		this->flags &= ~ui::CheckBox::LOCKED;
	/* maybe draw check */
	if(this->flags & ui::CheckBox::CHECKED)
	{
		/* who cares about offseting this with BORDER_THICKNESS, the next set of border draws will overwrite that part anyway */
		C2D_DrawRectSolid(this->x, this->y, this->z, this->w, this->h, this->slots[1]);
	}
	/* and lastly draw border */
	/** MAP:
	 * O  B
	 *  |---|
	 * A|   | C
	 *  |---|
	 *    D
	 */
	/* A */ C2D_DrawLine(this->x , this->y , this->slots[0], this->x , this->oy, this->slots[0], BORDER_THICKNESS, this->z);
	/* B */ C2D_DrawLine(this->x , this->y , this->slots[0], this->ox, this->y , this->slots[0], BORDER_THICKNESS, this->z);
	/* C */ C2D_DrawLine(this->ox, this->oy, this->slots[0], this->ox, this->y , this->slots[0], BORDER_THICKNESS, this->z);
	/* D */ C2D_DrawLine(this->x , this->oy, this->slots[0], this->ox, this->oy, this->slots[0], BORDER_THICKNESS, this->z);
	
	return true;
}

bool ui::CheckBox::status()
{
	return (this->flags & ui::CheckBox::CHECKED) != 0;
}

float ui::CheckBox::height()
{
	return this->h;
}

float ui::CheckBox::width()
{
	return this->w;
}

