#pragma once

namespace edge
{
	template<typename I> class com_ptr
	{
		static_assert (std::is_base_of_v<IUnknown, I>);

		I* _ptr = nullptr;

	public:
		com_ptr() = default;

		com_ptr (const com_ptr<I>& other)
		{
			static_assert(false); // use move constructor instead
		}

		com_ptr<I>& operator= (const com_ptr<I>& other)
		{
			static_assert(false); // use move-assignment instead
		}

		com_ptr (com_ptr<I>&& other) noexcept
		{
			if (other._ptr != nullptr)
				std::swap (this->_ptr, other._ptr);
		}

		com_ptr<I>& operator= (com_ptr<I>&& other) noexcept
		{
			if (_ptr != nullptr)
			{
				_ptr->Release();
				_ptr = nullptr;
			}

			std::swap (this->_ptr, other._ptr);
			return *this;
		}

		com_ptr(I* from)
			: _ptr(from)
		{
			static_assert (std::is_convertible<I*, IUnknown*>::value, "");
			if (_ptr != nullptr)
				_ptr->AddRef();
		}

		com_ptr(I* from, bool add_ref)
			: _ptr(from)
		{
			static_assert (std::is_convertible<I*, IUnknown*>::value, "");
			if (add_ref && (_ptr != nullptr))
				_ptr->AddRef();
		}

		com_ptr (IUnknown* punk)
		{
			if (punk != nullptr)
			{
				auto hr = punk->QueryInterface(&_ptr);
				assert(SUCCEEDED(hr));
			}
		}

		template<typename IFrom>
		com_ptr<I>& operator= (const com_ptr<IFrom>& rhs)
		{
			static_assert (std::is_base_of_v<IUnknown, I>);
			static_assert (std::is_base_of_v<IUnknown, IFrom>);

			if (_ptr)
			{
				_ptr->Release();
				_ptr = nullptr;
			}

			rhs.get()->QueryInterface(&_ptr);
			return *this;
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
			static_assert (std::is_base_of_v<IUnknown, I>);

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
	};
}
