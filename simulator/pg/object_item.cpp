
#include "object_item.h"
#include "include/pg/property_grid.h"

using namespace pg;
using namespace edge;

extern std::unique_ptr<group_item_i> make_group_item(object_item_i* parent, const property_group* group);

object_item_child_manager::object_item_child_manager (object_item_i* owner, object_list_i& selected_objects)
	: _owner(owner)
	, _selected_objects(selected_objects)
{
	if (_selected_objects.size())
		on_selected_objects_inserted({ 0, _selected_objects.size() });

	_selected_objects.objects_change().add_handler<&object_item_child_manager::on_selected_objects_change>(this);
}

object_item_child_manager::~object_item_child_manager()
{
	_selected_objects.objects_change().remove_handler<&object_item_child_manager::on_selected_objects_change>(this);

	if (_selected_objects.size())
		on_selected_objects_removing({ 0, _selected_objects.size() });
}

void object_item_child_manager::register_property_change_events (range_t range)
{
	for (size_t i = range.from; i < range.to; i++)
	{
		if (auto np = dynamic_cast<edge::notify_property_change*>(_selected_objects[i]))
		{
			np->property_changing().add_handler<&object_item_child_manager::on_property_changing>(this);
			np->property_changed().add_handler<&object_item_child_manager::on_property_changed>(this);
		}
	}
}

void object_item_child_manager::unregister_property_change_events (range_t range)
{
	for (int i = (int)range.to - 1; i >= (int)range.from; i--)
	{
		if (auto np = dynamic_cast<edge::notify_property_change*>(_selected_objects[i]))
		{
			np->property_changed().remove_handler<&object_item_child_manager::on_property_changed>(this);
			np->property_changing().remove_handler<&object_item_child_manager::on_property_changing>(this);
		}
	}
}

void object_item_child_manager::on_property_changing (object* obj, const property_change_args& args)
{
	size_t object_index = _selected_objects.index_of(obj);

	for (auto& gi : _children)
	{
		for (auto& pi : gi->children())
		{
			if (pi->property() == args.property)
			{
				pi->on_property_changing (object_index, args);
				return;
			}
		}
	}
}

void object_item_child_manager::on_property_changed (object* obj, const property_change_args& args)
{
	size_t object_index = _selected_objects.index_of(obj);

	for (auto& gi : this->children())
	{
		for (auto& pi : gi->children())
		{
			if (pi->property() == args.property)
			{
				pi->on_property_changed (object_index, args);
				return;
			}
		}
	}
}

struct group_comparer
{
	bool operator()(const property_group* g1, const property_group* g2) const 
	{
		int32_t g1prio = g1 ? g1->prio : 0;
		int32_t g2prio = g2 ? g2->prio : 0;
		if (g1prio < g2prio)
			return true;
		if (g1prio > g2prio)
			return false;

		const char* g1name = g1 ? g1->name : "";
		const char* g2name = g2 ? g2->name : "";
		int cmp = strcmp(g1name, g2name);
		if (cmp < 0)
			return true;
		if (cmp > 0)
			return false;

		return g1 < g2;
	}
};

static std::set<const property_group*, group_comparer> make_group_list (const object_list_i& objs)
{
	std::set<const property_group*, group_comparer> groups;
	if (!objs.empty() && objs.all([](object* o) { return o; }))
	{
		for (auto pe = objs.front()->type()->make_property_enumerator(); pe; pe++)
		{
			if (auto uiprop = dynamic_cast<const ui_property_i*>(*pe))
			{
				// Have we looked at this group in a previous property?
				if (groups.contains(uiprop->group()))
					continue;
				bool seen_before = false;
				for (auto pe1 = objs.front()->type()->make_property_enumerator(); *pe1 != *pe; pe1++)
				{
					if (auto uiprop1 = dynamic_cast<const ui_property_i*>(*pe1); uiprop1 && uiprop1->group() == uiprop->group())
					{
						seen_before = true;
						break;
					}
				}
				if (seen_before)
					continue;

				bool missing_in_any_other_object = false;
				for (size_t i = 1; i < objs.size(); i++)
				{
					bool present_in_this_object = false;
					for (auto pe1 = objs[i]->type()->make_property_enumerator(); pe1; pe1++)
					{
						if (auto uiprop1 = dynamic_cast<const ui_property_i*>(*pe1); uiprop1 && uiprop1->group() == uiprop->group())
						{
							present_in_this_object = true;
							break;
						}
					}

					if (!present_in_this_object)
					{
						missing_in_any_other_object = true;
						break;
					}
				}

				if (!missing_in_any_other_object)
					groups.insert(uiprop->group());
			}
		}
	}

	return groups;
}

void object_item_child_manager::on_selected_objects_change (const object_list_i::change_args& args)
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

void object_item_child_manager::on_selected_objects_inserting (const object_list_i::inserting_args& args)
{
	// For each existing group item, we look at its group and we check if it's missing
	// in any of the objects to insert. If it's missing, we remove that group item.

	for (size_t i = 0; i < _children.size(); )
	{
		bool missing_in_objects_to_insert = false;
		for (object* oi : args.objects_to_insert)
		{
			bool present_in_this_object = false;
			
			if (oi)
			{
				for (auto pe = oi->type()->make_property_enumerator(); pe; pe++)
				{
					if (auto uiprop = dynamic_cast<const ui_property_i*>(*pe); uiprop && uiprop->group() == _children[i]->group())
					{
						present_in_this_object = true;
						break;
					}
				}
			}

			if (!present_in_this_object)
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

void object_item_child_manager::on_selected_objects_inserted (const object_list_i::inserted_args& args)
{
	if (args.size == _selected_objects.size())
	{
		// Objects have been inserted into an empty object list. We create group items for all of their groups.
		for (auto* group : make_group_list(_selected_objects))
			_children.push_back(make_group_item(_owner, group));
	}
	
	register_property_change_events({ args.index, args.index + args.size });
}

void object_item_child_manager::on_selected_objects_removing (const object_list_i::removing_args& args)
{
	unregister_property_change_events({ args.index, args.index + args.size });

	if (args.size == _selected_objects.size())
	{
		// Last remaining objects are being removed from the list.
		_children.clear();
	}
}

void object_item_child_manager::on_selected_objects_removed (const object_list_i::removed_args& args)
{
	// Find groups that are present in all remaining objects and missing in some removed objects,
	// create group items for every such group, and insert them at the right place.
	
	auto new_groups = make_group_list(_selected_objects);
	auto new_it = new_groups.begin();
	size_t i = 0;
	auto root = _owner->as_item()->root();
	while (new_it != new_groups.end())
	{
		if ((i == _children.size()) || (_children[i]->group() != *new_it))
			_children.insert(_children.begin() + i, make_group_item(_owner, *new_it));
		new_it++;
		i++;
	}
}

void object_item_child_manager::on_selected_objects_replacing (const object_list_i::replacing_args& args)
{
	// TODO: optimize this, then take care to call unregister_property_change_events() here.
	on_selected_objects_removing (object_list_i::removing_args{ args.index, args.new_objs.size() });
	on_selected_objects_inserting (object_list_i::inserting_args{ args.new_objs });
}

void object_item_child_manager::on_selected_objects_replaced (const object_list_i::replaced_args& args)
{
	// TODO: optimize this, then take care to call register_property_change_events() here.
	on_selected_objects_inserted (object_list_i::inserted_args{ args.index, args.old_objs.size() });
	on_selected_objects_removed (object_list_i::removed_args{ args.old_objs });
}

