
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include <vector>
#include <array>
#include "assert.h"

namespace edge
{
	// In most cases it's easier to use this class as hierarchy root rather than as a member variable.
	// When it is a member variable, it might be destroyed before subscribers have a chance to unsubscribe themselves.
	class event_manager
	{
		template<typename event_t, typename... args_t>
		friend struct event;

		struct handler
		{
			void* callback;
			void* callback_arg;
		};

		struct id_and_handler
		{
			const char* id;
			struct handler handler;
		};

		// TODO: Make this a pointer to save RAM.
		std::vector<id_and_handler> handlers;

	protected:
		~event_manager()
		{
			assert(handlers.empty());
		}

		template<typename event_t>
		typename event_t::invoker event_invoker() const
		{
			return typename event_t::invoker(this);
		}
	};

	// Note that this currently works only with a single thread;
	// don't try to do something with events in more than one thread.
	template<typename event_t, typename... args_t>
	struct event
	{
		using callback_t = void(*)(void* callback_arg, args_t... args);

	private:
		static constexpr char id = 0; // the address of this field is used within this file to tell between different events, even if they have handlers with the same signature

	public:
		event() = delete; // this class and classes derived from it are not meant to be instantiated.

		class subscriber
		{
			event_manager* const _em;

		public:
			subscriber (event_manager* em)
				: _em(em)
			{ }

			void add_handler (void(*callback)(void* callback_arg, args_t... args), void* callback_arg)
			{
				_em->handlers.push_back( { &id, { reinterpret_cast<void*>(callback), callback_arg } });
			}

			void remove_handler (void(*callback)(void* callback_arg, args_t... args), void* callback_arg)
			{
				for (auto it = _em->handlers.begin(); it != _em->handlers.end(); it++)
				{
					if ((it->id == &id) && (it->handler.callback == reinterpret_cast<void*>(callback)) && (it->handler.callback_arg == callback_arg))
					{
						_em->handlers.erase(it);
						return;
					}
				}

				assert(false); // handler to remove not found
			}

		private:
			template<typename T>
			struct extract_class;

			template<typename R, typename C, class... A>
			struct extract_class<R(C::*)(A...)>
			{
				using class_type = C;
			};

			template<typename R, typename C, class... A>
			struct extract_class<R(C::*)(A...) const>
			{
				using class_type = C;
			};

			template<auto member_callback>
			static void proxy (void* arg, args_t... args)
			{
				using member_callback_t = decltype(member_callback);
				using class_type = typename extract_class<member_callback_t>::class_type;
				auto c = static_cast<class_type*>(arg);
				(c->*member_callback)(std::forward<args_t>(args)...);
			}

		public:
			template<auto member_callback>
			std::enable_if_t<std::is_member_function_pointer_v<decltype(member_callback)>>
			add_handler (typename extract_class<decltype(member_callback)>::class_type* target)
			{
				add_handler (&subscriber::proxy<member_callback>, target);
			}

			template<auto member_callback>
			std::enable_if_t<std::is_member_function_pointer_v<decltype(member_callback)>>
			remove_handler (typename extract_class<decltype(member_callback)>::class_type* target)
			{
				remove_handler (&subscriber::proxy<member_callback>, target);
			}
		};

	private:
		friend class event_manager;

		class invoker
		{
			const event_manager* const _em;

		public:
			invoker (const event_manager* em)
				: _em(em)
			{ }

			bool has_handlers() const
			{
				for (auto& h : _em->handlers)
				{
					if (h.id == &id)
						return true;
				}

				return false;
			}

			void operator()(args_t... args)
			{
				event_manager::handler first[8];
				size_t count = 0;
				std::vector<event_manager::handler> rest;

				// Note that this function must be reentrant (one event handler can invoke
				// another event, or add/remove events), that's why these stack copies.

				for (auto& p : _em->handlers)
				{
					if (p.id == &id)
					{
						if (count < std::size(first))
							first[count] = p.handler;
						else
							rest.push_back(p.handler);
						count++;
					}
				}

				for (size_t i = 0; i < count; i++)
				{
					callback_t callback;
					void* arg;
					if (i < std::size(first))
					{
						callback = reinterpret_cast<callback_t>(first[i].callback);
						arg = first[i].callback_arg;
					}
					else
					{
						callback = reinterpret_cast<callback_t>(rest[i - std::size(first)].callback);
						arg = rest[i - std::size(first)].callback_arg;
					}

					callback (arg, std::forward<args_t>(args)...);
				}
			}
		};
	};
}
