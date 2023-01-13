
#pragma once
#include "collection_property.h"
#include "property_accessors.h"

namespace edge
{
	struct value_collection_property : collection_property
	{
		virtual bool can_insert_remove() const = 0;
		virtual void get_to_string (const object* from_obj, size_t from_index, out_sstream_i* to, const string_convert_context_i* context) const = 0;
		virtual void set_from_string (std::string_view from, object* to_obj, size_t to_index, const string_convert_context_i* context) const = 0;
		virtual void insert_value (std::string_view from, object* to_obj, size_t to_index, const string_convert_context_i* context) const = 0;
		virtual void remove_value (object* obj, size_t index) const = 0;
		virtual bool changed (const object* obj, size_t index) const = 0;
		virtual const char* type_name() const = 0;
		virtual bool equal (size_t index, const object* obj1, const object* obj2) const = 0;

		std::string get_to_string (const object* from_obj, size_t from_index, const string_convert_context_i* context) const
		{
			struct oss : out_sstream_i
			{
				std::string buffer;
				virtual void write (const char* data, size_t size) override { buffer.append(data, size); }
			} s;
			this->get_to_string (from_obj, from_index, &s, context);
			return std::move(s.buffer);
		}
	};

	struct value_collection_property_change_args : collection_property_change_args
	{
		using base = collection_property_change_args;

		value_collection_property_change_args (const value_collection_property* property, size_t index, collection_property_change_type type)
			: base(property, index, type)
		{ }

		value_collection_property_change_args (const value_collection_property* property, size_t first, size_t n_first, size_t last)
			: base(property, first, n_first, last)
		{ }

		const value_collection_property* property() const { return static_cast<const value_collection_property*>(base::property()); }
	};

	template<typename property_traits>
	struct typed_value_collection_property : value_collection_property
	{
		using base = value_collection_property;

		using value_t = typename property_traits::value_t;

		using get_size_t     = pointer_to_derived_member_function_t<size_t, true>;
		using get_value_t    = pointer_to_derived_member_function_t<value_t, true, size_t>;
		using set_value_t    = pointer_to_derived_member_function_t<void, false, size_t, value_t>;
		using insert_value_t = pointer_to_derived_member_function_t<void, false, size_t, value_t>;
		using remove_value_t = pointer_to_derived_member_function_t<value_t, false, size_t>;
		using changed_t      = pointer_to_derived_member_function_t<bool, true, size_t>;

		const char*    const _name;
		get_size_t     const _get_size;
		get_value_t    const _get_value;
		set_value_t    const _set_value;
		insert_value_t const _insert_value;
		remove_value_t const _remove_value;
		changed_t      const _changed;

		// Constructor for fixed-size collections.
		// The constructor of the object must pre-allocate all collection items,
		// and the "changed" function tells if the collection is changed compared to what the constructor put there.
		constexpr typed_value_collection_property (const char* name,
			get_size_t get_size, get_value_t get_value, set_value_t set_value, changed_t changed
		)
			: _name(name)
			, _get_size(get_size)
			, _get_value(get_value)
			, _set_value(set_value)
			, _changed(changed)
		{ }

		// Constructor for variable-sized collections.
		constexpr typed_value_collection_property (const char* name,
			get_size_t get_size, get_value_t get_value, set_value_t set_value, insert_value_t insert_value, remove_value_t remove_value
		)
			: _name(name)
			, _get_size(get_size)
			, _get_value(get_value)
			, _set_value(set_value)
			, _insert_value(insert_value)
			, _remove_value(remove_value)
		{ }

		value_t get (const object* obj, size_t index) const
		{
			return (obj->*_get_value)(index);
		}

		void set (const value_t& from, object* to, size_t index) const
		{
			(to->*_set_value)(from, index);
		}

		virtual const char* name() const override final { return _name; }

		virtual size_t size (const object* obj) const override
		{
			return (obj->*_get_size)();
		}

		virtual void rotate (object* obj, size_t first, size_t n_first, size_t last) const override final
		{
			rassert(false); // not implemented
		}

		virtual void get_to_string (const object* from_obj, size_t from_index, out_sstream_i* to, const string_convert_context_i* context) const override
		{
			auto value = (from_obj->*_get_value)(from_index);
			property_traits::to_string(value, to, context);
		}

		virtual void set_from_string (std::string_view from, object* to_obj, size_t to_index, const string_convert_context_i* context) const override
		{
			typename property_traits::value_t value;
			property_traits::from_string(from, value, context);
			(to_obj->*_set_value) (to_index, value);
		}

		virtual void insert_value (std::string_view from, object* to_obj, size_t to_index, const string_convert_context_i* context) const override
		{
			typename property_traits::value_t value;
			property_traits::from_string(from, value, context);
			(to_obj->*_insert_value) (to_index, value);
		}

		virtual void remove_value (object* obj, size_t index) const override
		{
			rassert(false); // not implemented
		}

		virtual bool can_insert_remove() const override { return _insert_value != nullptr; }

		virtual bool changed (const object* obj, size_t index) const override
		{
			return (obj->*_changed)(index);
		}

		virtual const char* type_name() const override final { return property_traits::type_name; }

		virtual bool equal (size_t index, const object* obj1, const object* obj2) const override final
		{
			return (obj1->*_get_value)(index) == (obj2->*_get_value)(index);
		}
	};
}
