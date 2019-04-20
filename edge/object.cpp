
#include "object.h"

namespace edge
{
	std::vector<const type*>* type::known_types;

	type::type (const char* name, const type* base_type, span<const property* const> props)
		: name(name), base_type(base_type), props(props)
	{
		if (known_types == nullptr)
			known_types = new std::vector<const type*>();
		known_types->push_back(this);
	}

	type::~type()
	{
		auto it = std::find(known_types->begin(), known_types->end(), this);
		assert (it != known_types->end());
		known_types->erase(it);
		if (known_types->empty())
		{
			delete known_types;
			known_types = nullptr;
		}
	}

	//static
	const type* type::find_type (const char* name)
	{
		if (known_types == nullptr)
			return nullptr;
		auto it = std::find_if(known_types->begin(), known_types->end(), [name](const type* t) { return (t->name == name) || strcmp(t->name, name) == 0; });
		return (it != known_types->end()) ? *it : nullptr;
	}

	void type::add_properties (std::vector<const property*>& properties) const
	{
		if (this->base_type != nullptr)
			this->base_type->add_properties (properties);

		for (auto p : props)
			properties.push_back (p);
	}

	std::vector<const property*> type::make_property_list() const
	{
		std::vector<const property*> props;
		this->add_properties (props);
		return props;
	}

	const property* type::find_property (const char* name) const
	{
		for (auto p : props)
		{
			if ((p->_name == name) || (strcmp(p->_name, name) == 0))
				return p;
		}

		if (base_type != nullptr)
			return base_type->find_property(name);
		else
			return nullptr;
	}

	void object::on_property_changing (const property_change_args& args)
	{
		this->event_invoker<property_changing_e>()(this, args);
	}

	void object::on_property_changed (const property_change_args& args)
	{
		this->event_invoker<property_changed_e>()(this, args);
	}

	const xtype<object> object::_type = { "object", nullptr, { }, [] { return new object(); }, };

}
