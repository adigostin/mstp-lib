
#pragma once
#include "vector_nothrow.h"

template<typename key_t, typename value_t>
class unordered_map_nothrow
{
public:
	using value_type = std::pair<key_t, value_t>;
	using iterator   = typename vector_nothrow<value_type>::iterator;
	using reverse_iterator = typename vector_nothrow<value_type>::reverse_iterator;

private:
	vector_nothrow<value_type> _v;

public:
	unordered_map_nothrow() = default;

	unordered_map_nothrow (const unordered_map_nothrow&) = delete;
	unordered_map_nothrow& operator= (const unordered_map_nothrow&) = delete;

	unordered_map_nothrow (unordered_map_nothrow&& from)
		: _v(std::move(from._v))
	{
	}

	unordered_map_nothrow& operator= (unordered_map_nothrow&& from)
	{
		WI_ASSERT(false); // not yet implemented
		return *this;
	}

	void clear()
	{
		_v.clear();
	}

	iterator find (const key_t& key)
	{
		for (auto it = _v.begin(); it != _v.end(); it++)
		{
			if (it->first == key)
				return it;
		}

		return _v.end();
	}

	template<typename predicate_t>
	iterator find_if (predicate_t pred) //requires std::is_invocable_r_v<bool, predicate_t, const value_type&>
	{
		return _v.find_if(std::move(pred));
	}

	value_type remove (iterator it)
	{
		return _v.remove(it);
	}
	
	void erase (iterator it)
	{
		_v.erase(it);
	}

	[[nodiscard]] bool try_insert (value_type&& val)
	{
		for (uint32_t i = 0; i < _v.size(); i++)
			WI_ASSERT(_v[i].first != val.first);

		return _v.try_push_back(std::move(val));
	}

	[[nodiscard]] bool try_reserve (uint32_t new_cap)
	{
		return _v.try_reserve(new_cap);
	}

	bool empty() const { return _v.empty(); }
	uint32_t size() const { return _v.size(); }
	
	iterator begin() { return _v.begin(); }
	iterator end() { return _v.end(); }

	reverse_iterator rbegin() { return _v.rbegin(); }
	reverse_iterator rend() { return _v.rend(); }
};
