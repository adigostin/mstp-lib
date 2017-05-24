#pragma once

// This needs to be a base class (rather than a member variable) because the member variable
// might be destroyed before subscribers have a chance to unsubscribe themselves.
struct EventManager abstract
{
	friend struct EventBase;

public:
	struct EventHandler
	{
		void* callback;
		void* callbackArg;
	};

private:
	std::unordered_multimap<std::type_index, EventHandler> _events;
};

struct EventBase abstract
{
protected:
	static void AddEventHandlerInternal(std::type_index eventType, EventManager* em, void* callback, void* callbackArg)
	{
		em->_events.insert({ eventType,{ callback, callbackArg } });
	}

	static void RemoveEventHandlerInternal(std::type_index eventType, EventManager* em, void* callback, void* callbackArg)
	{
		auto range = em->_events.equal_range(eventType);

		auto it = std::find_if(range.first, range.second, [=](const std::pair<std::type_index, EventManager::EventHandler>& p)
		{
			return (p.second.callback == callback) && (p.second.callbackArg == callbackArg);
		});

		assert(it != range.second); // handler to remove not found

		em->_events.erase(it);
	}

	static void MakeHandlerList(std::type_index eventType, const EventManager& em, std::vector<EventManager::EventHandler>& longList, std::array<EventManager::EventHandler, 8>& shortList, size_t& shortListSizeOut)
	{
		shortListSizeOut = 0;

		size_t count = em._events.count(eventType);
		if (count == 0)
			return;

		auto range = em._events.equal_range(eventType);

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
};

template<typename TEventType, typename... Args>
struct Event abstract : EventBase
{ };

// Note that this currently works only with a single thread;
// don't try to do something with events in more than one thread.
template<typename TEventType, typename... Args>
struct Event<TEventType, void(Args...)> abstract : EventBase
{
	typedef void(*callback_t) (void* callbackArg, Args... args);

	Event() = delete; // this class and classes derived from this one are not meant to be instantiated.

	class Subscriber
	{
		EventManager* const _em;

	public:
		Subscriber(EventManager* em)
			: _em(em)
		{ }

		void AddHandler(callback_t callback, void* callbackArg)
		{
			AddEventHandlerInternal(std::type_index(typeid (TEventType)), _em, callback, callbackArg);
		}

		void RemoveHandler(callback_t callback, void* callbackArg)
		{
			RemoveEventHandlerInternal(std::type_index(typeid (TEventType)), _em, callback, callbackArg);
		}
	};

	static void InvokeHandlers(const EventManager& em, Args... args)
	{
		// Note that this function must be reentrant (one event handler can invoke another event).
		// We use one of two lists: one in the stack in case we have a few handlers, the other in the heap for many handlers.
		std::vector<EventManager::EventHandler> longList;
		std::array<EventManager::EventHandler, 8> shortList;
		size_t shortListSize;

		MakeHandlerList(std::type_index(typeid (TEventType)), em, longList, shortList, shortListSize);

		if (longList.empty())
		{
			for (size_t i = 0; i < shortListSize; i++)
			{
				auto callback = (callback_t)shortList[i].callback;
				auto arg = shortList[i].callbackArg;
				callback (arg, std::forward<Args>(args)...);
			}
		}
		else
		{
			for (auto h : longList)
				((callback_t)h.callback) (h.callbackArg, std::forward<Args>(args)...);
		}
	}
};
