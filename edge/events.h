
#pragma once
#include <typeindex>
#include <unordered_map>
#include <array>

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

		// TODO: Make this a pointer to save RAM.
		std::unordered_multimap<std::type_index, handler> _events;

	protected:
		template<typename event_t>
		typename event_t::invoker event_invoker()
		{
			return typename event_t::invoker(this);
		}
	};

	// Note that this currently works only with a single thread;
	// don't try to do something with events in more than one thread.
	template<typename event_t, typename... args_t>
	struct event abstract
	{
		using callback_t = void(*)(void* callback_arg, args_t... args);

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
				auto type = std::type_index(typeid (event_t));
				_em->_events.insert({ type, { callback, callback_arg } });
			}

			void remove_handler (void(*callback)(void* callback_arg, args_t... args), void* callback_arg)
			{
				auto type = std::type_index(typeid (event_t));
				auto range = _em->_events.equal_range(type);

				auto it = std::find_if(range.first, range.second, [=](const std::pair<std::type_index, event_manager::handler>& p)
				{
					return (p.second.callback == callback) && (p.second.callback_arg == callback_arg);
				});

				assert(it != range.second); // handler to remove not found

				_em->_events.erase(it);
			}
		};

	private:
		friend class event_manager;

		static void make_handler_list(std::type_index eventType, const event_manager* em, std::vector<event_manager::handler>& longList, std::array<event_manager::handler, 8>& shortList, size_t& shortListSizeOut)
		{
			shortListSizeOut = 0;

			const size_t count = em->_events.count(eventType);
			if (count == 0)
				return;

			auto range = em->_events.equal_range(eventType);

			if (count <= shortList.size())
			{
				for (auto it = range.first; it != range.second; it++)
					shortList[shortListSizeOut++] = it->second;
			}
			else
			{
				for (auto it = range.first; it != range.second; it++)
					longList.push_back(it->second);
			}
		}

		class invoker
		{
			event_manager* const _em;

		public:
			invoker (event_manager* em)
				: _em(em)
			{ }

			bool has_handlers() const { return _em->_events.find(std::type_index(typeid (event_t))) != _em->_events.end(); }

			void operator()(args_t... args)
			{
				// Note that this function must be reentrant (one event handler can invoke another event).
				// We use one of two lists: one in the stack in case we have a few handlers, the other in the heap for many handlers.
				std::vector<event_manager::handler> longList;
				std::array<event_manager::handler, 8> shortList;
				size_t shortListSize;

				make_handler_list(std::type_index(typeid (event_t)), _em, longList, shortList, shortListSize);

				if (longList.empty())
				{
					for (size_t i = 0; i < shortListSize; i++)
					{
						auto callback = (callback_t)shortList[i].callback;
						auto arg = shortList[i].callback_arg;
						callback (arg, std::forward<args_t>(args)...);
					}
				}
				else
				{
					for (auto h : longList)
						((callback_t)h.callback) (h.callback_arg, std::forward<args_t>(args)...);
				}
			}
		};
	};
}
