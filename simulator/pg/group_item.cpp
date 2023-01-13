
#include "include/pg/property_grid.h"
#include "edge/utility_functions.h"

using namespace pg;
using namespace edge;

extern std::unique_ptr<property_item_i> make_property_item (group_item_i* parent, const edge::property* prop);

class group_item : public group_item_i
{
	edge::event_manager _em;
	object_item_i* const _parent;
	const property_group* const _group;
	edge::text_layout_with_metrics _layout;
	std::vector<std::unique_ptr<property_item_i>> _children;

public:
	group_item (object_item_i* parent, const property_group* group)
		: _parent(parent), _group(group)
	{
		_parent->objects().objects_change().add_handler<&group_item::on_selected_objects_change>(this);
		perform_layout();
		auto root = this->root();
		for (const edge::property* prop : make_property_list(_parent->objects(), _group))
			_children.push_back(make_property_item(this, prop));
	}

	~group_item()
	{
		item_removing_e::invoker(_em).invoke(this);
		_parent->objects().objects_change().remove_handler<&group_item::on_selected_objects_change>(this);
	}

	#pragma region group_item_i
	virtual object_item_i* parent() const override final { return _parent; }

	virtual const property_group* const group() const override final { return _group; }

	virtual std::span<std::unique_ptr<property_item_i> const> children() const override final { return _children; }
	#pragma endregion

	struct property_comparer
	{
		bool operator()(const edge::property* p1, const edge::property* p2) const 
		{
			if (p1 == p2)
				return false;
			int cmp = strcmp(p1->name(), p2->name());
			if (cmp < 0)
				return true;
			if (cmp > 0)
				return false;
			return p1 < p2;
		}
	};

	static std::set<const property*, property_comparer> make_property_list (const object_list_i& objs, const property_group* group)
	{
		std::set<const property*, property_comparer> props;
		if (!objs.empty())
		{
			for (auto pe = objs.front()->type()->make_property_enumerator(); pe; pe++)
			{
				if (auto uiprop = dynamic_cast<const ui_property_i*>(*pe);
					uiprop && uiprop->group() == group)
				{
					bool missing_in_any_other_object = false;
					for (size_t i = 1; i < objs.size(); i++)
					{
						if (!objs[i]->type()->has_property(*pe))
						{
							missing_in_any_other_object = true;
							break;
						}
					}

					if (!missing_in_any_other_object)
						props.insert(*pe);
				}
			}
		}

		return props;
	}

	void on_selected_objects_change (const object_list_i::change_args& args)
	{
		if (std::holds_alternative<object_list_i::inserting_args>(args))
			on_selected_objects_inserting(std::get<object_list_i::inserting_args>(args));
		else if (std::holds_alternative<object_list_i::inserted_args>(args))
			on_selected_objects_inserted(std::get<object_list_i::inserted_args>(args));
		else if (std::holds_alternative<object_list_i::removing_args>(args))
			on_selected_objects_removing(std::get<object_list_i::removing_args>(args));
		else if (std::holds_alternative<object_list_i::removed_args>(args))
			on_selected_objects_removed(std::get<object_list_i::removed_args>(args));
		else if (std::holds_alternative<object_list_i::replacing_args>(args))
			on_selected_objects_replacing(std::get<object_list_i::replacing_args>(args));
		else if (std::holds_alternative<object_list_i::replaced_args>(args))
			on_selected_objects_replaced(std::get<object_list_i::replaced_args>(args));
		else
			rassert(false);
	}

	void on_selected_objects_inserting (const object_list_i::inserting_args& args)
	{
		// For each existing property item, we look at its property and we check if it's missing
		// in any of the objects to insert. If it's missing, we remove that property item.
		// Otherwise we keep it and ask it to reconcile itself.
		auto root = this->root();

		for (size_t i = 0; i < _children.size(); )
		{
			bool missing_in_objects_to_insert = false;
			for (object* oi : args.objects_to_insert)
			{
				if (!oi || !oi->type()->has_property(_children[i]->property()))
				{
					missing_in_objects_to_insert = true;
					break;
				}
			}

			if (missing_in_objects_to_insert)
				_children.erase(_children.begin() + i);
			else
				i++;
		}
	}

	void on_selected_objects_inserted (const object_list_i::inserted_args& args)
	{
		rassert (args.size);

		this->perform_layout();

		auto& objs = parent()->objects();
		if (args.size == objs.size())
		{
			// Objects have been inserted into an empty object list. We property group items for all of their groups.
			for (auto* prop : make_property_list(parent()->objects(), _group))
			{
				rassert(false);
				//_children.push_back(make_child_item(prop));
			}
		}
	}

	void on_selected_objects_removing (const object_list_i::removing_args& args)
	{
		rassert (args.size);

		if (args.size == parent()->objects().size())
		{
			// Last remaining objects removed from list.
			rassert(false);
			//_children.clear();
		}
	}

	void on_selected_objects_removed (const object_list_i::removed_args& args)
	{
		// Find properties that are present in all remaining objects and missing in some removed objects,
		// create property items for every such property, and insert them at the right place.
		// For the property items that are there already, call on_selected_objects_removed.

		auto root = this->root();
		auto new_props = make_property_list(parent()->objects(), _group);
		auto new_it = new_props.begin();
		size_t i = 0;
		while (new_it != new_props.end())
		{
			if ((i == _children.size()) || (_children[i]->property() != *new_it))
				_children.insert(_children.begin() + i, make_property_item(this, *new_it));
			new_it++;
			i++;
		}
	}

	void on_selected_objects_replacing (const object_list_i::replacing_args& args)
	{
		rassert(false);
	}

	void on_selected_objects_replaced (const object_list_i::replaced_args& args)
	{
		rassert(false);
	}

	#pragma region expandable_item_i
	virtual item_i* as_item() override final { return this; }

	virtual size_t child_count() const override final { return _children.size(); }

	virtual item_i* child_at(size_t index) const override final { return _children[index].get(); }

	virtual bool expanded() const override final
	{
		// Group items are currently always expanded.
		return true;
	}

	virtual void expand() override final
	{
		rassert(false);
	}

	virtual void collapse() override final
	{
		rassert(false);
	}
	#pragma region

	#pragma region item_i
	//virtual object_item_i* parent() const override final { return _parent; }

	virtual void perform_layout() override final
	{
		if (_group)
		{
			auto grid = this->grid();
			uint32_t dpi = edge::dpi(grid->window().hwnd());
			float layout_width = grid->value_column_right(dpi) - grid->expand_column_left(dpi) - 2 * title_lr_padding;
			if (layout_width > 0)
				_layout = text_layout_with_metrics (grid->renderer()->dwrite_factory(), grid->bold_text_format(), _group->name, layout_width);
			else
				_layout.clear();
			grid->invalidate_item(this);
		}
	}

	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		if (!_layout)
			return;
		
		auto grid = this->grid();
		uint32_t dpi = edge::dpi(grid->window().hwnd());
		float pw = edge::pixel_width(dpi);
		float height = std::ceil (this->content_height() / pw) * pw;
		rc.dc->FillRectangle ({ grid->expand_column_left(dpi), y, grid->value_column_right(dpi), y + height }, rc.back);
		rc.dc->DrawTextLayout ({ grid->expand_column_left(dpi) + indent() * grid->indent_width() + text_lr_padding, y }, _layout, rc.fore);
	}

	virtual float content_height() const override final
	{
		return _layout ? _layout.height() : 0;
	}

	virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const override final
	{
		return ::LoadCursor(nullptr, IDC_ARROW);
	}

	virtual bool selectable() const override final { return false; }
	virtual void on_mouse_down (const edge::mouse_ud_args& ma, float item_y) { }
	virtual void on_mouse_up   (const edge::mouse_ud_args& ma, float item_y) { }
	virtual std::string description_title() const override final { return { }; }
	virtual std::string description_text() const override final { return { }; }
	virtual item_removing_e::subscriber item_removing() override final { return item_removing_e::subscriber(_em); }
	#pragma endregion
};

std::unique_ptr<group_item_i> make_group_item (object_item_i* parent, const property_group* group)
{
	return std::make_unique<group_item>(parent, group);
}
