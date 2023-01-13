
#include "include/pg/property_grid.h"
#include "edge/utility_functions.h"

using namespace pg;
using namespace edge;

class value_collection_existing_child_item : public value_collection_child_item_i
{
	edge::event_manager _em;
	value_collection_item_i* const _parent;
	edge::text_layout_with_metrics _name;
	value_layout_t _value;
	uint32_t _property_setting_semaphore = 0;

public:
	value_collection_existing_child_item (value_collection_item_i* parent)
		: _parent(parent)
	{
		auto& objs = _parent->parent()->parent()->objects();
		objs.objects_change().add_handler<&value_collection_existing_child_item::on_selected_objects_change>(this);
	}

	~value_collection_existing_child_item()
	{
		item_removing_e::invoker(_em).invoke(this);
		auto& objs = _parent->parent()->parent()->objects();
		objs.objects_change().remove_handler<&value_collection_existing_child_item::on_selected_objects_change>(this);
	}

	bool multiple_selection (size_t value_index) const
	{
		auto& objs = parent()->parent()->parent()->objects();
		for (size_t obj_index = 1; obj_index < objs.size(); obj_index++)
		{
			if (!parent()->property()->equal(value_index, objs[0], objs[obj_index]))
				return true;
		}

		return false;
	}

	bool changed(size_t value_index) const
	{
		rassert (!multiple_selection(value_index));
		auto& objs = parent()->parent()->parent()->objects();
		auto prop = parent()->property();
		if (prop->can_insert_remove())
			return true;
	
		return objs.any ([prop, value_index](edge::object* o) { return prop->changed(o, value_index); });
	}

	void perform_layout (size_t value_index)
	{
		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		float line_width = grid->line_width(dpi);
		auto dwf = grid->renderer()->dwrite_factory();
		auto prop = parent()->property();
		
		float name_layout_width = grid->value_column_left(dpi) - grid->name_column_left(indent()) - line_width - 2 * text_lr_padding;
		if (name_layout_width <= 0)
			_name = { };
		else
			_name = text_layout_with_metrics(dwf, grid->text_format(), std::string("[") + std::to_string(value_index) + "]", name_layout_width);

		float value_layout_width = grid->value_column_right(dpi) - grid->value_column_left(dpi) - line_width - 2 * text_lr_padding;
		try
		{
			if (multiple_selection(value_index))
			{
				_value = {
					text_layout_with_metrics(dwf, grid->bold_text_format(), "(multiple selection)", value_layout_width), 
					value_layout_t::read_state::multiple_values
				};
			}
			else
			{
				auto& objs = parent()->parent()->parent()->objects();
				auto value_str = prop->get_to_string(objs[0], value_index, root()->app_context());
				auto format = changed(value_index) ? grid->bold_text_format() : grid->text_format();
				_value = { text_layout_with_metrics(dwf, format, value_str, value_layout_width), value_layout_t::read_state::ok };
			}
		}
		catch (const std::exception& ex)
		{
			_value = { text_layout_with_metrics(dwf, grid->bold_text_format(), ex.what(), value_layout_width), value_layout_t::read_state::read_exception };
		}

		grid->invalidate_item(this);
	}

	#pragma region item_i
	//virtual value_collection_item_i* parent() const override final

	virtual void perform_layout() override final
	{
		perform_layout(parent()->index_of(this));
	}

	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		auto grid = root()->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		float pw = edge::pixel_width(dpi);
		auto lt = grid->line_width(dpi);
		float height = std::ceil(this->content_height() / pw) * pw;

		render_default_background(rc, y, selected, hot, focused);

		float name_line_x = grid->expand_column_left(dpi) + indent() * grid->indent_width();
		rc.dc->DrawLine ({ name_line_x + lt / 2, y }, { name_line_x + lt / 2, y + height }, rc.disabled_fore, lt);
		rc.dc->DrawTextLayout ({ grid->expand_column_left(dpi) + indent() * grid->indent_width() + lt + text_lr_padding, y }, _name, rc.fore);

		float linex = grid->value_column_left(dpi) + lt / 2;
		rc.dc->DrawLine ({ linex, y }, { linex, y + height }, rc.disabled_fore, lt);

		if (auto& tl = _value.tl)
		{
			ID2D1Brush* brush = root()->grid()->read_only() ? rc.disabled_fore.get() : rc.fore.get();
			D2D1_POINT_2F l = { grid->value_column_left(dpi) + lt + text_lr_padding, y };
			rc.dc->DrawTextLayout (l, _value.tl, brush);
		}
	}

	virtual float content_height() const override final
	{
		return std::max (_name ? _name.height() : 0, _value.tl ? _value.tl.height() : 0);
	}

	virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const override final
	{
		return ::LoadCursor(nullptr, IDC_ARROW);
	}

	virtual bool selectable() const override final
	{
		return true;
	}

	virtual void on_mouse_down (const edge::mouse_ud_args& ma, float item_y) override final
	{
		size_t value_index = parent()->index_of(this);
		auto& objs = parent()->parent()->parent()->objects();
		auto prop = parent()->property();

		bool bold;
		std::string initial_text;
		if (multiple_selection(value_index))
		{
			bold = true;
		}
		else
		{
			bold = changed(value_index);
			initial_text = prop->get_to_string(objs[0], value_index, root()->app_context());
		}

		grid()->try_show_text_editor_on_selected_item(bold, initial_text);
	}

	virtual void on_mouse_up   (const edge::mouse_ud_args& ma, float item_y) override final
	{
	}

	virtual std::string description_title() const override final
	{
		return std::string(parent()->property()->name()) + "[" + std::to_string(parent()->index_of(this)) + "]";
	}

	virtual std::string description_text() const override final
	{
		return { };
	}

	virtual item_removing_e::subscriber item_removing() override final
	{
		return item_removing_e::subscriber(_em);
	}
	#pragma endregion

	#pragma region collection_existing_child_item_i
	virtual size_t collection_entry_count() const override final
	{
		rassert(false); return { };
	}

	virtual collection_existing_child_item_i* collection_entry_at (size_t index) const override final
	{
		rassert(false); return { };
	}

	virtual collection_new_child_item_i* collection_new_entry() const override final
	{
		rassert(false); return { };
	}

	virtual void on_property_setting (size_t object_index) override final
	{
		_property_setting_semaphore++;
	}

	virtual void on_property_set     (size_t object_index) override final
	{
		_property_setting_semaphore--;
		if (!_property_setting_semaphore)
		{
			this->perform_layout(); // TODO: reconcile layout instead of unconditionally recreating it.
			root()->grid()->invalidate();
		}
	}
	#pragma endregion

	void on_selected_objects_inserting (std::span<edge::object* const> objects_to_insert)
	{
	}

	void on_selected_objects_change (const object_list_i::change_args& args)
	{
		if (object_list_i::is_changed_event(args))
			perform_layout();
	}

	#pragma region value_collection_child_item_i
	virtual value_collection_item_i* parent() const override final
	{
		return _parent;
	}
	#pragma endregion
};

extern std::unique_ptr<value_collection_child_item_i> make_value_collection_child_item (value_collection_item_i* parent)
{
	return std::make_unique<value_collection_existing_child_item>(parent);
}
