#pragma once

template<typename I> class com_ptr
{
	I* _ptr = nullptr;

public:
	com_ptr() = default;

	com_ptr (const com_ptr<I>& other)
	{
		if (other._ptr != nullptr)
		{
			_ptr = other._ptr;
			_ptr->AddRef();
		}
	}

	com_ptr<I>& operator= (const com_ptr<I>& other)
	{
		assert(false); // not implemented
		return *this;
	}

	com_ptr (com_ptr<I>&& other) noexcept
	{
		if (other._ptr != nullptr)
			std::swap (this->_ptr, other._ptr);
	}

	com_ptr<I>& operator= (com_ptr<I>&& other) noexcept
	{
		if (other._ptr != nullptr)
			std::swap (this->_ptr, other._ptr);
		return *this;
	}

	com_ptr (IUnknown* punk)
	{
		if (punk != nullptr)
		{
			auto hr = punk->QueryInterface(&_ptr);
			assert(SUCCEEDED(hr));
		}
	}

	com_ptr(I* from)
		: _ptr(from)
	{
		static_assert (std::is_convertible<I*, IUnknown*>::value, "");
		if (_ptr != nullptr)
			_ptr->AddRef();
	}

	com_ptr (I* from, bool add_ref)
		: _ptr(from)
	{
		static_assert (std::is_convertible<I*, IUnknown*>::value, "");
		if ((_ptr != nullptr) && add_ref)
			_ptr->AddRef();
	}

	com_ptr(nullptr_t np)
	{ }

	com_ptr<I>& operator=(nullptr_t np)
	{
		if (_ptr)
		{
			_ptr->Release();
			_ptr = nullptr;
		}
		return *this;
	}

	~com_ptr()
	{
		if (_ptr != nullptr)
		{
			auto p = _ptr;
			_ptr = nullptr;
			p->Release();
		}
	}

	operator I*() const { return _ptr; }

	I* get() const { return _ptr; }

	I* operator->() const
	{
		assert(_ptr != nullptr);
		return _ptr;
	}

	I** operator&()
	{
		if (_ptr != nullptr)
		{
			auto p = _ptr;
			_ptr = nullptr;
			p->Release();
		}

		return &_ptr;
	}

	bool operator== (I* other) const
	{
		return _ptr == other;
	}

	bool operator!= (I* other) const
	{
		return _ptr != other;
	}

	bool operator== (const com_ptr<I>& other) const
	{
		return _ptr == other._ptr;
	}

	bool operator!= (const com_ptr<I>& other) const
	{
		return _ptr != other._ptr;
	}
};
