
#pragma once
#include "edge/om/object.h"

namespace pg
{
	struct range_t
	{
		size_t from;
		size_t to;
		size_t size() const { return to - from; }
		bool empty() const { return from == to; }
	};

	struct object_list_i
	{
		virtual ~object_list_i() = default;
		virtual size_t size() const = 0;
		virtual edge::object* operator[](size_t index) const = 0;

		class iterator
		{
			const object_list_i* const _objects;
			size_t _index;

		public:
			iterator (const object_list_i* oi, size_t index)
				: _objects(oi), _index(index)
			{ }

			bool operator!= (nullptr_t np) const
			{
				return this->_index < _objects->size();
			}

			iterator& operator++()
			{
				rassert(_index < _objects->size());
				_index++;
				return *this;
			}

			edge::object* operator*() const
			{
				rassert(_index < _objects->size());
				return _objects->operator[](_index);
			}

			iterator operator+ (size_t other) const
			{
				return { _objects, _index + 1 };
			}
		};

		iterator begin() const { return iterator(this, 0); }
		nullptr_t end() const { return nullptr; }

		edge::object* front() const { return this->operator[](0); }

		edge::object* back() const
		{
			size_t oc = this->size();
			rassert(oc);
			return this->operator[](oc - 1);
		}

		bool empty() const { return !this->size(); }

		size_t index_of (edge::object* o) const
		{
			size_t oc = this->size();
			for (size_t i = 0; i < oc; i++)
			{
				if (this->operator[](i) == o)
					return i;
			}

			rassert(false); return -1;
		}

		bool contains (const edge::object* e) const
		{
			size_t oc = this->size();
			for (size_t i = 0; i < oc; i++)
			{
				if (this->operator[](i) == e)
					return true;
			}

			return false;
		}

		template<typename predicate_t> requires std::is_invocable_r_v<bool, predicate_t, edge::object*> 
		bool any (size_t from, size_t to, const predicate_t& pred) const
		{
			rassert ((from <= to) && (to <= size()));
			for (size_t i = from; i < to; i++)
			{
				if (pred(this->operator[](i)))
					return true;
			}

			return false;
		}

		template<typename predicate_t> requires std::is_invocable_r_v<bool, predicate_t, edge::object*> 
		bool any (const predicate_t& pred) const
		{
			return any (0, size(), pred);
		}

		template<typename predicate_t> requires std::is_invocable_r_v<bool, predicate_t, edge::object*> 
		bool all (size_t from, size_t to, const predicate_t& pred) const
		{
			rassert ((from <= to) && (to <= size()));
			for (size_t i = from; i < to; i++)
			{
				if (!pred(this->operator[](i)))
					return false;
			}

			return true;
		}

		template<typename predicate_t> requires std::is_invocable_r_v<bool, predicate_t, edge::object*> 
		bool all (const predicate_t& pred) const
		{
			return all (0, size(), pred);
		}

		std::vector<edge::object*> to_vector() const
		{
			std::vector<edge::object*> res;
			size_t s = this->size();
			res.reserve(s);
			for (size_t i = 0; i < s; i++)
				res.push_back(this->operator[](i));
			return res;
		}

		struct inserting_args { std::span<edge::object* const> objects_to_insert; };
		struct inserted_args { size_t index; size_t size; };
		struct removing_args { size_t index; size_t size; };
		struct removed_args { std::span<edge::object* const> objects_removed; };
		struct replacing_args { size_t index; std::span<edge::object* const> new_objs; };
		struct replaced_args { size_t index; std::span<edge::object* const> old_objs; };

		using change_args = std::variant<inserting_args, inserted_args, removing_args, removed_args, replacing_args, replaced_args>;
		
		// Returns true for events generated _after_ the change.
		static bool is_changed_event (const change_args& args)
		{
			return std::holds_alternative<inserted_args>(args)
				|| std::holds_alternative<removed_args>(args)
				|| std::holds_alternative<replaced_args>(args);
		}

		struct change_e : edge::event<change_e, const change_args&> { };

		virtual change_e::subscriber objects_change() = 0;
	};
}
