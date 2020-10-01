
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#pragma once
#include <vector>
#include <array>
#include "rassert.h"

namespace edge
{
	// Note that this event system currently works only on a single thread;
	// don't try to do something with events on more than one thread.

	class event_manager
	{
		template<typename event_t, typename... args_t>
		friend struct event_base;

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

		std::vector<id_and_handler> handlers;

	protected:
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

	template<typename event_t, typename... args_t>
	struct event_base
	{
		friend class event_manager;

		using callback_t = void(*)(void*, args_t...);

	private:
		// The address of this field is used within this file to tell between different event types,
		// even if they have handlers with the same signature. This eliminates the need for RTTI.
		static constexpr char id = 0;

	public:
		event_base() = delete; // this class and classes derived from it are not meant to be instantiated.

		class subscriber
		{
			event_manager* const _em;

		public:
			subscriber (event_manager* em)
				: _em(em)
			{ }

			void add_handler (callback_t callback, void* callback_arg)
			{
				_em->handlers.push_back( { &id, { reinterpret_cast<void*>(callback), callback_arg } });
			}

			void remove_handler (callback_t callback, void* callback_arg)
			{
				for (auto it = _em->handlers.begin(); it != _em->handlers.end(); it++)
				{
					if ((it->id == &id) && (it->handler.callback == reinterpret_cast<void*>(callback)) && (it->handler.callback_arg == callback_arg))
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
			struct extract_class<void(C::*)(A...)>
			{
				using class_type = C;
			};

			template<class target_class, void(target_class::*member_callback)(args_t...)>
			static void proxy (void* arg, args_t... args)
			{
				auto c = static_cast<target_class*>(arg);
				(c->*member_callback)(std::forward<args_t>(args)...);
			}

		public:
			template<auto member_callback, typename class_type = extract_class<decltype(member_callback)>::class_type>
			std::enable_if_t<std::is_member_function_pointer_v<decltype(member_callback)>>
			add_handler (class_type* target)
			{
				add_handler (&subscriber::proxy<class_type, member_callback>, target);
			}

			template<auto member_callback, typename class_type = extract_class<decltype(member_callback)>::class_type>
			std::enable_if_t<std::is_member_function_pointer_v<decltype(member_callback)>>
			remove_handler (class_type* target)
			{
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
				for (auto& h : _em->handlers)
				{
					if (h.id == &id)
						return true;
				}

				return false;
			}

		protected:
			handler_list make_handler_list() const
			{
				// Note that the invoker must be reentrant (one event handler can invoke
				// another event, or add/remove events), that's why these stack copies.

				handler_list res;

				for (auto& p : _em->handlers)
				{
					if (p.id == &id)
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
	struct event : event_base<event_t, args_t...>
	{
		using base = event_base<event_t, args_t...>;

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

	template<typename event_t, typename cancel_type, typename... args_t>
	struct cancelable_event : event_base<event_t, std::optional<cancel_type>&, args_t...>
	{
		using base = event_base<event_t, std::optional<cancel_type>&, args_t...>;

		struct invoker : base::invoker
		{
			using base::invoker::invoker;

			void operator()(std::optional<cancel_type>& cancel, args_t... args) const
			{
				auto hl = base::invoker::make_handler_list();
				for (size_t i = 0; i < hl.count; i++)
				{
					auto handler = (i < std::size(hl.first)) ? hl.first[i] : hl.rest[i - std::size(hl.first)];
					handler.callback (handler.arg, cancel, std::forward<args_t>(args)...);
					if (cancel)
						return;
				}
			}
		};
	};
}
