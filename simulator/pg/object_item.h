
#pragma once
#include "include/pg/item.h"

namespace pg
{
	class object_item_child_manager
	{
		object_item_i* const _owner;
		object_list_i& _selected_objects;
		std::vector<std::unique_ptr<group_item_i>> _children;

	public:
		object_item_child_manager (object_item_i* owner, object_list_i& selected_objects);
		~object_item_child_manager();

		auto& children() const { return _children; }
		auto& selected_objects() const { return _selected_objects; }

	private:
		void on_selected_objects_change (const object_list_i::change_args& args);
		void on_selected_objects_inserting (const object_list_i::inserting_args& args);
		void on_selected_objects_inserted (const object_list_i::inserted_args& args);
		void on_selected_objects_removing (const object_list_i::removing_args& args);
		void on_selected_objects_removed (const object_list_i::removed_args& args);
		void on_selected_objects_replacing (const object_list_i::replacing_args& args);
		void on_selected_objects_replaced (const object_list_i::replaced_args& args);

		void register_property_change_events (range_t range);
		void unregister_property_change_events (range_t range);
		void on_property_changing (edge::object* obj, const edge::property_change_args& args);
		void on_property_changed (edge::object* obj, const edge::property_change_args& args);
	};
}
