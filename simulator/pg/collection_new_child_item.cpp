
#include "include/pg/property_grid.h"
#include "edge/utility_functions.h"

using namespace pg;

class collection_new_child_item : collection_new_child_item_i
{
	edge::event_manager _em;
	collection_item_i* const _parent;
	edge::text_layout_with_metrics _text_layout;

public:
	collection_new_child_item (collection_item_i* parent)
		: _parent(parent)
	{ }

	~collection_new_child_item()
	{
		item_removing_e::invoker(_em).invoke(this);
	}

	virtual collection_item_i* parent() const override final { return _parent; }

	virtual void perform_layout() override final
	{
		auto grid = this->grid();
		_text_layout = { grid->renderer()->dwrite_factory(), grid->text_format(), "(click to add)" };
		grid->invalidate_item(this);
	}

	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		render_default_background (rc, y, selected, hot, focused);

		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		ID2D1Brush* brush = grid->read_only() ? rc.disabled_fore.get() : rc.fore.get();
		D2D1_POINT_2F l = { grid->value_column_left(dpi) + grid->line_width(dpi) + text_lr_padding, y };
		rc.dc->DrawTextLayout (l, _text_layout, brush);
	}

	virtual float content_height() const override final
	{
		return 35;
	}

	virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const override final
	{
		auto grid = this->grid();
		LPCWSTR cursor = grid->read_only() ? IDC_ARROW : IDC_HAND;
		return ::LoadCursor(nullptr, cursor);
	}

	virtual bool selectable() const override final { return true; }
	virtual void on_mouse_down (const edge::mouse_ud_args& ma, float item_y) override final { }
	virtual void on_mouse_up   (const edge::mouse_ud_args& ma, float item_y) override final { }
	virtual std::string description_title() const override final { return { }; }
	virtual std::string description_text() const override final { return { }; }
	virtual item_removing_e::subscriber item_removing() override final { return item_removing_e::subscriber(_em); }
};

std::unique_ptr<collection_new_child_item_i> make_collection_new_child_item (collection_item_i* parent)
{
	rassert(false); return { };
}


