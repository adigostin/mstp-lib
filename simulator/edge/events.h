
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include "rassert.h"

namespace edge
{
	// Note that this event system currently works only on a single thread;
	// don't try to do something with events on more than one thread.

	class event_manager
	{
		template<typename event_t, typename return_t, typename... args_t>
		friend struct event_base;

		struct handler
		{
			void* callback;
			void* callback_arg;
		};

		struct id_and_handler
		{
			std::type_index id;
			struct handler handler;
		};

		std::vector<id_and_handler> handlers;

	public:
		~event_manager()
		{
			rassert(handlers.empty());
		}

		template<typename event_t>
		typename event_t::invoker event_invoker() const
		{
			return typename event_t::invoker(this);
		}
	};

	template<typename event_t, typename return_t, typename... args_t>
	struct event_base
	{
		friend class event_manager;

		using callback_t = return_t(*)(void*, args_t...);

	public:
		event_base() = delete; // this class and classes derived from it are not meant to be instantiated.

		class subscriber
		{
			event_manager* const _em;

		public:
			subscriber (event_manager* em)
				: _em(em)
			{ }

			subscriber (event_manager& em)
				: _em(&em)
			{ }

			void add_handler (callback_t callback, void* callback_arg)
			{
				_em->handlers.push_back( { std::type_index(typeid(event_t)), { reinterpret_cast<void*>(callback), callback_arg } });
			}

			void remove_handler (callback_t callback, void* callback_arg)
			{
				auto id = std::type_index(typeid(event_t));

				for (auto it = _em->handlers.begin(); it != _em->handlers.end(); it++)
				{
					if ((it->id == id) && (it->handler.callback == reinterpret_cast<void*>(callback)) && (it->handler.callback_arg == callback_arg))
					{
						_em->handlers.erase(it);
						return;
					}
				}

				rassert(false); // handler to remove not found
			}

		private:
			template<typename T>
			struct extract_class;

			template<typename C, class... A>
			struct extract_class<return_t(C::*)(A...)>
			{
				using class_type = C;
			};

			template<class target_class, return_t(target_class::*member_callback)(args_t...)>
			static return_t proxy (void* arg, args_t... args)
			{
				auto c = static_cast<target_class*>(arg);
				return (c->*member_callback)(std::forward<args_t>(args)...);
			}

		public:
			template<auto member_callback, typename class_type = extract_class<decltype(member_callback)>::class_type>
			std::enable_if_t<std::is_member_function_pointer_v<decltype(member_callback)>>
			add_handler (class_type* target)
			{
				static_assert(std::is_convertible_v<decltype(member_callback), return_t(class_type::*)(args_t...)>, "types of parameters and/or return value don't match");
				add_handler (&subscriber::proxy<class_type, member_callback>, target);
			}

			template<auto member_callback, typename class_type = extract_class<decltype(member_callback)>::class_type>
			std::enable_if_t<std::is_member_function_pointer_v<decltype(member_callback)>>
			remove_handler (class_type* target)
			{
				static_assert(std::is_convertible_v<decltype(member_callback), return_t(class_type::*)(args_t...)>, "types of parameters and/or return value don't match");
				remove_handler (&subscriber::proxy<class_type, member_callback>, target);
			}
		};

		class invoker
		{
			const event_manager* const _em;

			struct handler
			{
				callback_t callback;
				void* arg;
			};

			struct handler_list
			{
				handler first[8];
				size_t count = 0;
				std::vector<handler> rest;
			};

		public:
			invoker (const event_manager* em)
				: _em(em)
			{ }

			bool has_handlers() const
			{
				auto id = std::type_index(typeid(event_t));

				for (auto& h : _em->handlers)
				{
					if (h.id == id)
						return true;
				}

				return false;
			}

		protected:
			handler_list make_handler_list() const
			{
				// Note that the invoker must be reentrant (one event handler can invoke
				// another event, or add/remove events), that's why these stack copies.

				auto id = std::type_index(typeid(event_t));

				handler_list res;

				for (auto& p : _em->handlers)
				{
					if (p.id == id)
					{
						auto h = handler { reinterpret_cast<callback_t>(p.handler.callback), p.handler.callback_arg };
						if (res.count < std::size(res.first))
							res.first[res.count] = h;
						else
							res.rest.push_back(h);
						res.count++;
					}
				}

				return res;
			}
		};
	};

	template<typename event_t, typename... args_t>
	struct event : event_base<event_t, void, args_t...>
	{
		using base = event_base<event_t, void, args_t...>;

		struct invoker : base::invoker
		{
			using base::invoker::invoker;

			void operator()(args_t... args) const
			{
				auto hl = base::invoker::make_handler_list();
				for (size_t i = 0; i < hl.count; i++)
				{
					auto handler = (i < std::size(hl.first)) ? hl.first[i] : hl.rest[i - std::size(hl.first)];
					handler.callback (handler.arg, std::forward<args_t>(args)...);
				}
			}

		};
	};

	template<typename event_t, typename return_t, typename... args_t>
	struct cancelable_event : event_base<event_t, std::optional<return_t>, args_t...>
	{
		using base = event_base<event_t, std::optional<return_t>, args_t...>;

		struct invoker : base::invoker
		{
			using base::invoker::invoker;

			std::optional<return_t> operator()(args_t... args) const
			{
				auto hl = base::invoker::make_handler_list();
				for (size_t i = hl.count - 1; i != -1; i--)
				{
					auto handler = (i < std::size(hl.first)) ? hl.first[i] : hl.rest[i - std::size(hl.first)];
					auto result = handler.callback (handler.arg, std::forward<args_t>(args)...);
					if (result)
						return result;
				}

				return std::nullopt;
			}
		};
	};
}
