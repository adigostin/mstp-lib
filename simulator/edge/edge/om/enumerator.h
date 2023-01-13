#pragma once

namespace edge
{
	template<typename T>
	struct __declspec(novtable) enumerator_i
	{
		virtual ~enumerator_i() = default;
		virtual bool valid() const = 0;
		virtual void move_next() = 0;
		virtual T get() const = 0;
	};

	#pragma region container_enumerator
	// This class is an enumerator that holds a standard C++ container with the things to be enumerated.
	// The interface to inherit from must always be specified, and this rules out using Class Template
	// Argument Deduction (CTAD), as CTAD is "all or nothing".
	// To still have some deduction, use make_container_enumerator() below.
	template<typename iface, typename container_t, typename transformer_t>
	class container_enumerator : public iface
	{
		using begin_iterator_t = decltype(std::declval<container_t>().cbegin());
		using end_iterator_t   = decltype(std::declval<container_t>().cend());
		using in_type = typename container_t::value_type;
		using out_type = decltype(std::declval<iface>().get());

		static_assert(std::is_invocable_r_v<out_type, transformer_t, in_type>);

		begin_iterator_t _it;
		container_t _container;
		transformer_t _transformer;

	public:
		container_enumerator (container_t container, transformer_t transformer)
			: _it(container.begin())
			, _container(std::move(container))
			, _transformer(std::move(transformer))
		{ }

		virtual bool valid() const override final { return _it != _container.end(); }
		virtual void move_next() override final { _it++; }
		virtual out_type get() const override final { return _transformer(*_it); }
	};

	template<typename container_t>
	inline typename container_t::value_type identity_function (const typename container_t::value_type& o)
	{
		return o;
	}

	template<typename iface, typename container_t, typename transformer_t = decltype(&identity_function<container_t>)>
	container_enumerator<iface, container_t, transformer_t>*
		make_container_enumerator (container_t container, transformer_t transformer = &identity_function<container_t>)
	{
		return new container_enumerator<iface, container_t, transformer_t>(std::move(container), std::move(transformer));
	}
	#pragma endregion

	#pragma region iterator_pair_enumerator
	// This class is an enumerator similar to container_enumerator, except that it holds iterators
	// to a container rather than the container itself (i.e., it doesn't own the container).
	template<typename iface, typename begin_iterator_t, typename end_iterator_t, typename transformer_t>
	class iterator_pair_enumerator : public iface
	{
		using in_type = typename begin_iterator_t::value_type;
		using out_type = decltype(std::declval<iface>().get());

		static_assert(std::is_invocable_r_v<out_type, transformer_t, in_type>);

		begin_iterator_t _it;
		end_iterator_t _end;
		transformer_t _transformer;

	public:
		iterator_pair_enumerator (begin_iterator_t begin, end_iterator_t end, transformer_t transformer)
			: _it(begin)
			, _end(end)
			, _transformer(std::move(transformer))
		{ }

		virtual bool valid() const override final { return _it != _end; }
		virtual void move_next() override final { _it++; }
		virtual out_type get() const override final { return _transformer(*_it); }
	};

	template<typename iface, typename begin_iterator_t, typename end_iterator_t, typename transformer_t>
	iterator_pair_enumerator<iface, begin_iterator_t, end_iterator_t, transformer_t>*
	make_iterator_pair_enumerator (begin_iterator_t begin, end_iterator_t end, transformer_t transformer)
	{
		return new iterator_pair_enumerator<iface, begin_iterator_t, end_iterator_t, transformer_t>(begin, end, std::move(transformer));
	}

	template<typename iface, typename container_t, typename transformer_t>
	iterator_pair_enumerator<iface, typename container_t::const_iterator, typename container_t::const_iterator, transformer_t>*
	make_iterator_pair_enumerator (const container_t& cont, transformer_t transformer)
	{
		return new iterator_pair_enumerator<iface, typename container_t::const_iterator, typename container_t::const_iterator, transformer_t>
			(cont.begin(), cont.end(), std::move(transformer));
	}
	#pragma endregion

	template<typename T, typename enumerator_t>
	class enumerable
	{
		std::unique_ptr<enumerator_t> e;

	public:
		enumerable (enumerator_t* e)
			: e(e)
		{ }

		enumerable& operator++()
		{
			e->move_next();
			return *this;
		}

		T operator*() const { return e->get(); }

		T operator->() const { return e->get(); }

		enumerable begin()
		{
			return enumerable<T, enumerator_t>(std::move(*this));
		}

		struct end_t { };

		end_t end() { return { }; }

		bool operator!= (end_t) const { return e && e->valid(); }
		bool operator== (end_t) const { return !e || !e->valid(); }
	};
}
