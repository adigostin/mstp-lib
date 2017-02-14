#pragma once

struct EventBase abstract
{
	friend class EventManager;

protected:
	
	struct EventHandler
	{
		void* callback;
		void* callbackArg;
	};
	
protected:
	static void AddEventHandlerInternal(std::type_index eventType, EventManager& em, void* callback, void* callbackArg);
	static void RemoveEventHandlerInternal(std::type_index eventType, EventManager& em, void* callback, void* callbackArg);
	static void MakeHandlerList(std::type_index eventType, const EventManager& em, std::vector<EventHandler>& longList, std::array<EventHandler, 8>& shortList, size_t& shortListSizeOut);
};

class EventManager
{
	friend struct EventBase;
	std::unordered_multimap<std::type_index, EventBase::EventHandler> _events;
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
		EventManager& _em;

	public:
		Subscriber(EventManager& em)
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
		std::vector<EventHandler> longList;
		std::array<EventHandler, 8> shortList;
		size_t shortListSize;

		MakeHandlerList(std::type_index(typeid (TEventType)), em, longList, shortList, shortListSize);

		if (longList.empty())
		{
			for (size_t i = 0; i < shortListSize; i++)
				((callback_t)shortList[i].callback) (shortList[i].callbackArg, args...);
		}
		else
		{
			for (auto h : longList)
				((callback_t)h.callback) (h.callbackArg, args...);
		}
	}
};
