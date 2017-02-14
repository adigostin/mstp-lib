
#include "pch.h"
#include "EventManager.h"

// static
void EventBase::AddEventHandlerInternal(std::type_index eventType, EventManager& em, void* callback, void* callbackArg)
{
	em._events.insert ({ eventType,{ callback, callbackArg } });
}

// static
void EventBase::RemoveEventHandlerInternal(std::type_index eventType, EventManager& em, void* callback, void* callbackArg)
{
	auto range = em._events.equal_range(eventType);

	auto it = std::find_if(range.first, range.second, [=](std::pair<std::type_index, EventHandler> p)
	{
		return (p.second.callback == callback) && (p.second.callbackArg == callbackArg);
	});

	assert(it != range.second); // handler to remove not found

	em._events.erase(it);
}

// static
void EventBase::MakeHandlerList(std::type_index eventType, const EventManager& em, std::vector<EventHandler>& longList, std::array<EventHandler, 8>& shortList, size_t& shortListSizeOut)
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
