
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once

namespace edge
{
	struct range_t
	{
		uint32_t from;
		uint32_t to;
		uint32_t size() const { return to - from; }
		bool empty() const { return from == to; }
	};

	struct DECLSPEC_NOVTABLE DECLSPEC_UUID("17E0F04B-DCFF-4D00-9456-7C0D9FF85BC7") IObjectList : IUnknown
	{
		virtual uint32_t size() const = 0;
		virtual IDispatch* operator[](uint32_t index) const = 0;

		class iterator
		{
			const IObjectList* const _objects;
			uint32_t _index;

		public:
			iterator (const IObjectList* oi, uint32_t index)
				: _objects(oi), _index(index)
			{ }

			bool operator!= (nullptr_t np) const
			{
				return this->_index < _objects->size();
			}

			iterator& operator++()
			{
				_ASSERT(_index < _objects->size());
				_index++;
				return *this;
			}

			IDispatch* operator*() const
			{
				_ASSERT(_index < _objects->size());
				return _objects->operator[](_index);
			}

			iterator operator+ (uint32_t other) const
			{
				return { _objects, _index + 1 };
			}
		};

		iterator begin() const { return iterator(this, 0); }
		nullptr_t end() const { return nullptr; }

		IDispatch* front() const { return this->operator[](0); }

		IDispatch* back() const
		{
			uint32_t oc = this->size();
			_ASSERT(oc);
			return this->operator[](oc - 1);
		}

		bool empty() const { return !this->size(); }

		uint32_t index_of (IDispatch* o) const
		{
			uint32_t oc = this->size();
			for (uint32_t i = 0; i < oc; i++)
			{
				if (this->operator[](i) == o)
					return i;
			}

			_ASSERT(false); return -1;
		}

		bool contains (IDispatch* e) const
		{
			uint32_t oc = this->size();
			for (uint32_t i = 0; i < oc; i++)
			{
				if (this->operator[](i) == e)
					return true;
			}

			return false;
		}

		template<typename predicate_t> requires std::is_invocable_r_v<bool, predicate_t, IDispatch*> 
		bool any (uint32_t from, uint32_t to, const predicate_t& pred) const
		{
			_ASSERT ((from <= to) && (to <= size()));
			for (uint32_t i = from; i < to; i++)
			{
				if (pred(this->operator[](i)))
					return true;
			}

			return false;
		}

		template<typename predicate_t> requires std::is_invocable_r_v<bool, predicate_t, IDispatch*> 
		bool any (const predicate_t& pred) const
		{
			return any (0, size(), pred);
		}

		template<typename predicate_t> requires std::is_invocable_r_v<bool, predicate_t, IDispatch*> 
		bool all (uint32_t from, uint32_t to, const predicate_t& pred) const
		{
			_ASSERT ((from <= to) && (to <= size()));
			for (uint32_t i = from; i < to; i++)
			{
				if (!pred(this->operator[](i)))
					return false;
			}

			return true;
		}

		template<typename predicate_t> requires std::is_invocable_r_v<bool, predicate_t, IDispatch*> 
		bool all (const predicate_t& pred) const
		{
			return all (0, size(), pred);
		}
		/*
		std::vector<IDispatch*> to_vector() const
		{
			std::vector<IDispatch*> res;
			uint32_t s = this->size();
			res.reserve(s);
			for (uint32_t i = 0; i < s; i++)
				res.push_back(this->operator[](i));
			return res;
		}
		*/
	};
}
