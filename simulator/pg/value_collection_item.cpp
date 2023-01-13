
#include "include/pg/property_grid.h"

using namespace edge;
using namespace pg;

extern std::unique_ptr<value_collection_child_item_i> make_value_collection_child_item (value_collection_item_i* parent);
extern std::unique_ptr<collection_new_child_item_i> make_collection_new_child_item (collection_item_i* parent);

class value_collection_item : public value_collection_item_i
{
	event_manager _em;
	group_item_i* const _parent;
	const edge::value_collection_property* const _prop;

	// TODO: get rid of this type, as we need to handle anyway all 4 combinations of null/non-null.
	struct children_t
	{
		std::vector<std::unique_ptr<value_collection_child_item_i>> existing_values;
		std::unique_ptr<collection_new_child_item_i> new_value;
	};

	std::optional<children_t> _children;

public:
	value_collection_item (group_item_i* parent, const edge::value_collection_property* prop)
		: _parent(parent), _prop(prop)
	{ }

	~value_collection_item()
	{
		item_removing_e::invoker(_em).invoke(this);
	}

	#pragma region item_i
	virtual void perform_layout() override final
	{
		rassert(false);
	}

	virtual void render (const render_context& rc, float y, bool selected, bool hot, bool focused) const override final
	{
		rassert(false);
	}

	virtual float content_height() const override final
	{
		rassert(false); return { };
	}

	virtual HCURSOR cursor_at(D2D1_POINT_2F pd, float item_y) const override final
	{
		rassert(false); return { };
	}

	virtual bool selectable() const override final
	{
		rassert(false); return { };
	}

	virtual void on_mouse_down (const edge::mouse_ud_args& ma, float item_y) override final
	{
		rassert(false);
	}

	virtual void on_mouse_up   (const edge::mouse_ud_args& ma, float item_y) override final
	{
		rassert(false);
	}

	virtual std::string description_title() const override final
	{
		rassert(false); return { };
	}

	virtual std::string description_text() const override final
	{
		rassert(false); return { };
	}

	virtual item_removing_e::subscriber item_removing() override final
	{
		return item_removing_e::subscriber(_em);
	}
	#pragma endregion

	#pragma region value_collection_item_i
	virtual const edge::value_collection_property* property() const override final
	{
		return _prop;
	}
	#pragma endregion

	#pragma region expandable_item_i
	virtual item_i* as_item() override final { return this; }

	virtual size_t child_count() const override final
	{
		rassert(false); return { };
	}

	virtual item_i* child_at(size_t index) const override final
	{
		rassert(false); return { };
	}

	virtual bool expanded() const override final
	{
		return _children.has_value();
	}

	virtual void expand() override final
	{
		rassert (!_children);
		_children.emplace();
		auto& objs = parent()->parent()->objects();
		size_t child_count = property()->size(objs[0]);
		bool all_same_child_count = objs.all ([child_count, p=property()](object* o) { return p->size(o) == child_count; });
		rassert (all_same_child_count);
		auto root = this->root();
		
		for (size_t i = 0; i < child_count; i++)
			_children->existing_values.push_back(make_value_collection_child_item(this));

		if (property()->can_insert_remove())
			_children->new_value = make_collection_new_child_item(this);

		this->grid()->invalidate();
	}

	virtual void collapse() override final
	{
		if (_children)
		{
			_children.reset();
			this->grid()->invalidate();
		}
	}
	#pragma endregion

	#pragma region property_item_i
	virtual group_item_i* parent() const override final { return _parent; }

	virtual void on_property_changing (size_t object_index, const edge::property_change_args& args) override final
	{
		rassert(false);
	}

	virtual void on_property_changed (size_t object_index, const edge::property_change_args& args) override final
	{
		rassert(false);
	}

	void on_selected_objects_inserting (std::span<edge::object* const> objects_to_insert)
	{
	}

	void on_selected_objects_inserted (range_t range)
	{
		perform_layout();
	}

	void on_selected_objects_removing (range_t range)
	{
	}

	void on_selected_objects_removed (std::span<edge::object* const> objects_removed)
	{
		perform_layout();
	}

	void on_selected_objects_changing (size_t from_index, std::span<edge::object* const> objects_to_insert)
	{
		rassert(false);
	}

	void on_selected_objects_changed (size_t from_index, std::span<edge::object* const> objects_removed)
	{
		rassert(false);
	}
	#pragma endregion

	#pragma region collection_item_i
	virtual size_t collection_entry_count() const override final
	{
		return _children ? _children->existing_values.size() : 0;
	}

	virtual collection_existing_child_item_i* collection_entry_at (size_t index) const override final
	{
		return _children->existing_values[index].get();
	}

	virtual collection_new_child_item_i* collection_new_entry() const override final
	{
		return _children ? _children->new_value.get() : nullptr;
	}
	#pragma endregion
};

std::unique_ptr<property_item_i> make_value_collection_item (group_item_i* parent, const edge::value_collection_property* prop)
{
	return std::make_unique<value_collection_item>(parent, prop);
}
