
#include "include/pg/property_grid.h"
#include "object_item.h"
#include "edge/utility_functions.h"

using namespace edge;
using namespace pg;

class root_item : public root_item_i
{
	event_manager _em;
	property_grid_i* const _grid;
	std::string      const _heading;
	object_list_i& _ol;
	edge::string_convert_context_i* const _scc;
	object_item_child_manager _child_manager;
	edge::text_layout_with_metrics _text_layout;

public:
	root_item (property_grid_i* grid, std::string_view heading, object_list_i& ol, edge::string_convert_context_i* scc)
		: _grid(grid)
		, _heading(heading)
		, _ol(ol)
		, _scc(scc)
		, _child_manager(this, ol)
	{
		// Start listening to changes in the list of objects (objects arriving or removing).
		_ol.objects_change().add_handler<&root_item::on_selected_objects_change>(this);
		perform_layout();
	}

	~root_item()
	{
		item_removing_e::invoker(_em).invoke(this);
		_ol.objects_change().remove_handler<&root_item::on_selected_objects_change>(this);
	}
	
	#pragma region root_item_i
	virtual property_grid_i* grid() const override final { return _grid; }

	virtual edge::string_convert_context_i* app_context() const override final { return _scc; }
	#pragma endregion

	#pragma region object_item_i
	virtual object_list_i& objects() override final { return _ol; }
	#pragma endregion

	#pragma region item_i
	virtual expandable_item_i* parent() const override final { return nullptr; }

	virtual void perform_layout() override final
	{
		_text_layout.clear();
		uint32_t dpi = edge::dpi(_grid->window().hwnd());
		float layout_width = _grid->value_column_right(dpi) - _grid->expand_column_left(dpi) - 2 * title_lr_padding;
		if (layout_width > 0)
		{
			std::string s;
			if (!_heading.empty())
				s = _heading;
			else
			{
				if (_ol.empty())
				{
					s = "(no selection)";
				}
				else if (_ol.size() == 1)
				{
					s = _ol.front()->type()->name;
				}
				else
				{
					bool all_same_type = _ol.all ([front=_ol.front()](object* o) { return same_type(front, o); });
					if (all_same_type)
						s = std::to_string(_ol.size()) + " x " + _ol.front()->type()->name;
					else
						s = std::to_string(_ol.size()) + " elements";
				}
			}

			auto dwf = _grid->renderer()->dwrite_factory();
			auto tf = _grid->bold_text_format();
			_text_layout = text_layout_with_metrics (dwf, tf, s, layout_width);
		}

		_grid->invalidate_item(this);
	}

	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		float height = content_height();
		if (!height)
			return;

		uint32_t dpi = edge::dpi(_grid->window().hwnd());
		float pw = edge::pixel_width(dpi);
		height = std::ceil(height / pw) * pw;

		D2D1_RECT_F rect = { _grid->expand_column_left(dpi), y, _grid->value_column_right(dpi), y + height };
		rc.dc->FillRectangle (&rect, rc.root_item_back);
		float x = (rect.left + rect.right) / 2 - _text_layout.width() / 2;
		rc.dc->DrawTextLayout ({ x, y + title_ud_padding }, _text_layout, rc.root_item_fore);
	}

	virtual float content_height() const override final
	{
		if (!_text_layout)
			return 0;

		return title_ud_padding + _text_layout.height() + title_ud_padding;
	}

	virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const override final { return ::LoadCursor(nullptr, IDC_ARROW); }
	virtual bool selectable() const override final { return false; }
	virtual void on_mouse_down (const edge::mouse_ud_args& ma, float item_y) override final { }
	virtual void on_mouse_up   (const edge::mouse_ud_args& ma, float item_y) override final { }
	virtual std::string description_title() const override final { return { }; }
	virtual std::string description_text() const override final { return { }; }
	virtual root_item* as_root() override final { return this; }
	virtual item_removing_e::subscriber item_removing() override final { return item_removing_e::subscriber(_em); }
	#pragma endregion

	#pragma region expandable_item_i
	virtual item_i* as_item() override { return this; }

	virtual size_t child_count() const override final { return _child_manager.children().size(); }

	virtual group_item_i* child_at(size_t index) const override final
	{
		return _child_manager.children().at(index).get();
	}

	virtual bool expanded() const override final { return true; }
	virtual void expand() override final { rassert(false); }
	virtual void collapse() override final { rassert(false); }
	#pragma endregion

private:
	void on_selected_objects_change (const object_list_i::change_args& args)
	{
		if (object_list_i::is_changed_event(args))
			perform_layout();
	}
};

std::unique_ptr<root_item_i> make_root_item (property_grid_i* grid, std::string_view heading, object_list_i& objects, string_convert_context_i* app_context)
{
	return std::make_unique<root_item>(grid, heading, objects, app_context);
}
