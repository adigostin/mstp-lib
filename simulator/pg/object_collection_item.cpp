
#include "include/pg/property_grid.h"

using namespace pg;
using namespace edge;

class object_collection_item : public object_collection_item_i
{
	event_manager _em;
	group_item_i* const _parent;
	const edge::object_collection_property* const _prop;

public:
	object_collection_item (group_item_i* parent, const object_collection_property* prop)
		: _parent(parent), _prop(prop)
	{ }

	~object_collection_item()
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

	#pragma region property_item_i
	virtual item_i* as_item() override final { return this; }
	virtual group_item_i* parent() const override final { return _parent; }
	//virtual const edge::property* property() const override final { return _prop; }

	// These two functions are called from code in the object_item class, which listens to corresponding events.
	virtual void on_property_changing (size_t object_index, const edge::property_change_args& args) override final
	{
		rassert(false);
	}

	virtual void on_property_changed (size_t object_index, const edge::property_change_args& args) override final
	{
		rassert(false);
	}
	#pragma endregion

	#pragma region object_collection_item_i
	virtual const edge::object_collection_property* property() const override final
	{
		return _prop;
	}
	#pragma endregion

	bool multiple_child_types() const
	{
		//auto& objs = parent()->parent()->objects();
		rassert(false); return false;
	}

	#pragma region expandable_item_i
	//virtual item_i* as_item() = 0;

	virtual size_t child_count() const override final { return 0; }

	virtual item_i* child_at(size_t index) const override final
	{
		rassert(false); return { };
	}

	virtual bool expanded() const override final
	{
		return false;
	}

	virtual void expand() override final
	{
	}

	virtual void collapse() override final
	{
	}
	#pragma endregion

	#pragma region collection_item_i
	virtual size_t collection_entry_count() const override
	{
		return size_t();
	}
	virtual collection_existing_child_item_i* collection_entry_at(size_t index) const override
	{
		return nullptr;
	}

	virtual collection_new_child_item_i* collection_new_entry() const override
	{
		return nullptr;
	}
	#pragma endregion
};

std::unique_ptr<property_item_i> make_object_collection_item (group_item_i* parent, const edge::object_collection_property* prop)
{
	return std::make_unique<object_collection_item>(parent, prop);
}
