
#include "include/pg/property_grid.h"
#include "edge/utility_functions.h"

using namespace edge;
using namespace pg;

class value_property_item : public value_property_item_i
{
	event_manager _em;
	group_item_i* const _parent;
	const edge::value_property* const _prop;
	event_token const _soc_token = _parent->parent()->objects().objects_change().add_auto_handler<&value_property_item::on_selected_objects_change>(this);
	edge::text_layout_with_metrics _name = make_name_layout();
	value_layout_t _value = make_value_layout();

public:
	value_property_item (group_item_i* parent, const value_property* prop)
		: _parent(parent), _prop(prop)
	{ }

	~value_property_item()
	{
		item_removing_e::invoker(_em).invoke(this);
	}

	virtual group_item_i* parent() const override final
	{
		return _parent;
	}

	virtual void perform_layout() override final
	{
		_name = make_name_layout();
		_value = make_value_layout();
		grid()->invalidate_item(this);
	}

	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		render_default_background (rc, y, selected, hot, focused);

		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		auto lt = grid->line_width(dpi);

		rc.dc->DrawTextLayout ({ grid->expand_column_left(dpi) + indent() * grid->indent_width() + lt + text_lr_padding, y }, _name, rc.fore);

		if (auto& tl = _value.tl)
		{
			ID2D1Brush* brush;
			if (grid->read_only() || !can_edit())
				brush = rc.disabled_fore;
			else
				brush = rc.fore;

			D2D1_POINT_2F l = { grid->value_column_left(dpi) + lt + text_lr_padding, y };
			rc.dc->DrawTextLayout (l, _value.tl, brush);
		}
	}

	virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const override
	{
		if (this->grid()->read_only() || !can_edit())
			return ::LoadCursor(nullptr, IDC_ARROW);

		if (property()->nvps())
			return ::LoadCursor(nullptr, IDC_HAND);

		if (dynamic_cast<const pg_custom_editor_i*>(property()))
			return ::LoadCursor(nullptr, IDC_HAND);

		return ::LoadCursor (nullptr, IDC_IBEAM);
	}

	virtual bool selectable() const override final { return true; }

	bool can_edit() const
	{
		// TODO: Allow editing and setting a property that couldn't be read.
		if (_value.state != value_layout_t::read_state::ok)
			return false;

		if (dynamic_cast<const pg_custom_editor_i*>(property()))
			return true;

		auto& objs = parent()->parent()->objects();
		bool can_set = objs.all ([prop=property()](object* o) { return prop->can_set(o); });
		return can_set;
	}

	virtual float content_height() const override final
	{
		return std::max (_name ? _name.height() : 0, _value.tl ? _value.tl.height() : 0);
	}

	virtual std::string description_title() const override final
	{
		std::stringstream ss;
		ss << property()->name() << " (" << property()->type_name() << ")";
		return ss.str();
	}

	virtual std::string description_text() const override final
	{
		auto prop = dynamic_cast<const ui_property_i*>(this->property());
		return (prop && prop->description()) ? std::string(prop->description()) : std::string();
	}

	virtual item_removing_e::subscriber item_removing() override final
	{
		return item_removing_e::subscriber(_em);
	}

	virtual void on_property_changing (size_t object_index, const edge::property_change_args& args) override final
	{
	}

	virtual void on_property_changed (size_t object_index, const edge::property_change_args& args) override final
	{
		_value = make_value_layout();
		root()->grid()->invalidate();
	}

	void on_selected_objects_change (const object_list_i::change_args& args)
	{
		if (object_list_i::is_changed_event(args))
			perform_layout();
	}

	virtual void on_mouse_down (const edge::mouse_ud_args& ma, float item_y) override
	{
		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		auto vcx = grid->value_column_left(dpi);
		if (ma.pd.x < vcx)
			return;

		if (auto cep = dynamic_cast<const pg_custom_editor_i*>(property()))
		{
			// TODO: pass the read_only flag to the editor.
			rassert(false);
			//auto editor = cep->create_editor(parent()->parent()->object_list());
			//editor->show(&grid->window());
			return;
		}

		if (grid->read_only() || !can_edit())
			return;

		if (auto nvps = property()->nvps())
		{
			int selected_nvp_index = grid->show_enum_editor(ma.pd, nvps);
			if (selected_nvp_index >= 0)
			{
				auto changed = [new_value=nvps[selected_nvp_index].value, prop=property()]
					(const object* o) { return prop->enum_value_as_int(o) != new_value; };
				auto& objects = parent()->parent()->objects();
				if (objects.any(changed))
				{
					try
					{
						auto new_value_str = nvps[selected_nvp_index].name;
						grid->change_property (objects, property(), new_value_str, root()->app_context());
					}
					catch (const std::exception& ex)
					{
						::MessageBoxA (grid->window().hwnd(), ex.what(), "Error setting property", 0);
					}
				}
			}
		}
		else
		{
			bool bold = false;
			std::string str;
			if (_value.state == value_layout_t::read_state::ok)
			{
				auto& objs = parent()->parent()->objects();
				bold = objs.any ([p=property()](object* o) { return p->changed_from_default(o); });
				str = property()->get_to_string(objs[0], root()->app_context());
			}

			rassert(grid->selected_item() == this);
			if (grid->try_show_text_editor_on_selected_item (bold, str))
			{
				//editor->on_mouse_down (button, mks, pp, pd);
			}
		}
	}

	virtual void on_mouse_up (const edge::mouse_ud_args& ma, float item_y) override final
	{
	}

	// value_property_item_i
	virtual const edge::value_property* property() const override final { return _prop; }
};

std::unique_ptr<value_property_item_i> make_value_property_item (group_item_i* parent, const edge::value_property* prop)
{
	return std::make_unique<value_property_item>(parent, prop);
}
