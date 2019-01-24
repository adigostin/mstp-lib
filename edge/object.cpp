
#include "object.h"

namespace edge
{
	void type_t::add_properties (std::vector<const property*>& properties) const
	{
		if (this->base != nullptr)
			this->base->add_properties (properties);

		for (auto p = this->props.from; p != this->props.to; p++)
			properties.push_back (*p);
	}

	std::vector<const property*> type_t::make_property_list() const
	{
		std::vector<const property*> props;
		this->add_properties (props);
		return props;
	}

	void object::on_property_changing (const property* property)
	{
		this->event_invoker<property_changing_e>()(this, property);
	}

	void object::on_property_changed (const property* property)
	{
		this->event_invoker<property_changed_e>()(this, property);
	}
}
