
#include "events.h"
#include "rassert.h"

namespace edge
{
	event_manager::~event_manager()
	{
		bool all_empty = std::all_of (handlers.begin(), handlers.end(), [](auto& pair) { return pair.second.empty(); });
		
		// Only assert in normal program flow, not if we are unwinding.
		if (!std::uncaught_exceptions())
		{
			rassert (all_empty);

			// Trying to delete the event_manager from one of its invocations? It's not possible and will never be.
			rassert (!first_invoke && !last_invoke);
		}
	}

	bool event_manager::has_handlers() const
	{
		for (auto& kvp : handlers)
		{
			if (!kvp.second.empty())
				return true;
		}

		return false;
	}

	std::optional<event_manager::handler_ref_t> event_manager::find_handler (std::type_index event_id, void* callback, void* callback_arg)
	{
		if (auto it = handlers.find(event_id); it != handlers.end())
		{
			for (auto it1 = it->second.begin(); it1 != it->second.end(); it1++)
			{
				if ((it1->callback == callback) && (it1->callback_arg == callback_arg))
					return handler_ref_t{ nullptr, it1 };
			}
		}

		for (auto inv = first_invoke; inv != nullptr; inv = inv->next)
		{
			if (inv->event_id == event_id)
			{
				for (auto it = inv->queued_add_handlers.begin(); it != inv->queued_add_handlers.end(); it++)
				{
					if ((it->callback == callback) && (it->callback_arg == callback_arg))
						return handler_ref_t{ inv, it };
				}
			}
		}

		return std::nullopt;
	}

	event_manager::pending_invoke::pending_invoke (event_manager& em, std::type_index event_id, bool reverse_invoke)
		: em(em)
		, event_id(event_id)
		, reverse_invoke(reverse_invoke)
	{
		// Append "current_invoke" to the list of invocations.
		if (em.first_invoke)
		{
			em.last_invoke->next = this;
			this->prev = em.last_invoke;
		}
		else
			em.first_invoke = this;
		em.last_invoke = this;

		if (!reverse_invoke)
		{
			if (auto it = em.handlers.find(event_id); (it != em.handlers.end()) && !it->second.empty())
			{
				// Start enumeration on permanent list.
				next_handler = handler_ref_t { nullptr, it->second.begin() };
			}
			else if (auto inv = find_invoke_with_queued_handlers_forward (em.first_invoke))
			{
				// Start enumeration on the first list of queued handlers.
				next_handler = handler_ref_t { inv, inv->queued_add_handlers.begin() };
			}
		}
		else
		{
			if (auto inv = find_invoke_with_queued_handlers_backward(this))
			{
				// Starting enumeration on the most recent list of queued handlers.
				next_handler = handler_ref_t { inv, --inv->queued_add_handlers.end() };
			}
			else if (em.handlers.contains(event_id) && !em.handlers.at(event_id).empty())
			{
				// Start enumeration on the permanent list.
				next_handler = handler_ref_t{ nullptr, --em.handlers.at(event_id).end() };
			}
		}
	}

	event_manager::pending_invoke::~pending_invoke()
	{
		// If any of the callback we invoked above added handlers for this event_t,
		// we move them to the outer invocation (up the call stack); if this is the outermost
		// invocation, we move them to the permanent list of handlers instead.
		if (!queued_add_handlers.empty())
		{
			auto prev_inv = prev;
			while (prev_inv && (prev_inv->event_id != event_id))
				prev_inv = prev_inv->prev;

			auto& list_to_move_to = prev_inv ? prev_inv->queued_add_handlers : em.handlers[event_id];

			list_to_move_to.insert (list_to_move_to.end(), queued_add_handlers.begin(), queued_add_handlers.end());
			queued_add_handlers.clear();
		}

		// Remove "current_invoke" from the end of the list of invocations.
		rassert(em.last_invoke == this);
		em.last_invoke = prev;
		prev = nullptr;
		if (em.last_invoke)
			em.last_invoke->next = nullptr;
		else
			em.first_invoke = nullptr;
	}

	void event_manager::pending_invoke::move_next()
	{
		if (!reverse_invoke)
			move_next_forward();
		else
			move_next_backward();
	}

	// "search_from" is the first invocation looked at; if it matches the event_id and it has handlers, it is the one returned.
	event_manager::pending_invoke* event_manager::pending_invoke::find_invoke_with_queued_handlers_forward (pending_invoke* search_from) const
	{
		// We must not look at "this" invocation. Its queued handlers were added by the application
		// _after_ the application invoked the event, thus they must not be called.
		auto inv = search_from;
		while ((inv != this) && ((inv->event_id != event_id) || inv->queued_add_handlers.empty()))
			inv = inv->next;
		if (inv != this)
			return inv;
		else
			return nullptr;
	}

	void event_manager::pending_invoke::move_next_forward()
	{
		rassert (next_handler.has_value());

		if (!next_handler->invoke)
		{
			// We're enumerating the permanent list.
			next_handler->handler_it++;
			if (next_handler->handler_it != em.handlers.at(event_id).end())
				// Found another handler on the pemanent list.
				return;

			// Let's try to look at the queued lists.
			if (auto inv = find_invoke_with_queued_handlers_forward(em.first_invoke))
			{
				// Found one.
				next_handler = handler_ref_t { inv, inv->queued_add_handlers.begin() };
				return;
			}
			
			// There aren't any.
			next_handler = std::nullopt;
			return;
		}

		// We're enumerating a queued list.
		next_handler->handler_it++;
		if (next_handler->handler_it != next_handler->invoke->queued_add_handlers.end())
			// Found another handler on the same list.
			return;

		// We're done with a queued list. Let's try to find another queued list.
		if (auto inv = find_invoke_with_queued_handlers_forward(next_handler->invoke->next))
		{
			// Found another list.
			next_handler = handler_ref_t { inv, inv->queued_add_handlers.begin() };
			return;
		}

		// There aren't any.
		next_handler = std::nullopt;
	}

	// The first invocation looked at is the one before "search_from".
	event_manager::pending_invoke* event_manager::pending_invoke::find_invoke_with_queued_handlers_backward (pending_invoke* search_from) const
	{
		// We must not look at "this" invocation. Its queued handlers were added by the application
		// _after_ the application invoked the event, thus they must not be called.
		auto inv = search_from->prev;
		while (inv && ((inv->event_id != event_id) || inv->queued_add_handlers.empty()))
			inv = inv->prev;
		return inv;
	}

	void event_manager::pending_invoke::move_next_backward()
	{
		rassert (next_handler.has_value());

		if (next_handler->invoke)
		{
			// We're enumerating a queued list. Can we continue on the same list?
			if (next_handler->handler_it != next_handler->invoke->queued_add_handlers.begin())
			{
				// We can continue enumerating on the same list.
				--next_handler->handler_it;
				return;
			}

			// No. Let's try to find another queued list.
			if (auto inv = find_invoke_with_queued_handlers_backward(next_handler->invoke))
			{
				// Found one.
				next_handler = handler_ref_t { inv, --inv->queued_add_handlers.end() };
				return;
			}

			// Finished enumerating all queued lists. Let's try the permanent list.
			if (auto it = em.handlers.find(event_id); (it != em.handlers.end()) && !it->second.empty())
			{
				// Start enumeration on the permanent list.
				next_handler = handler_ref_t { nullptr, --it->second.end() };
				return;
			}

			// Done.
			next_handler = std::nullopt;
			return;
		}

		// We're enumerating the permanent list. Can we continue?
		if (next_handler->handler_it != em.handlers.at(event_id).begin())
		{
			// We can continue enumerating on the permanent list.
			--next_handler->handler_it;
			return;
		}

		// We're done.
		next_handler = std::nullopt;
	}

	void event_manager::add_handler (std::type_index event_id, void* callback, void* callback_arg)
	{
		// We don't allow registering the same handler twice for the same event id.
		// This would complicate our event system; as for the application, it's more
		// likely we're dealing with a programming error rather than with intended use.
		if (find_handler(event_id, callback, callback_arg))
			throw std::runtime_error("Handler already registered.");

		// When the application calls us to add a handler for an event event_t, it's possible
		// it has invoked that event and is calling us now from one of the handlers to add an
		// additional handler for the same event. The behavior we want in this case is to
		// _not_ call the just-added event as part of the same invocation. 
		auto* inv = last_invoke;
		while(inv && (inv->event_id != event_id))
			inv = inv->prev;

		if (!inv)
		{
			// no invoker currently running for this event_t
			handlers[event_id].push_back({ callback, callback_arg });
		}
		else
		{
			// Our invoker is running and calling handlers for this event_t. When it finishes
			// calling them, it will move this handler from here to the previous list of handlers.
			// to be called in future invocations.
			inv->queued_add_handlers.push_back({ callback, callback_arg });
		}
	}

	void event_manager::remove_handler (std::type_index event_id, void* callback, void* callback_arg)
	{
		auto handler_to_remove = find_handler (event_id, callback, callback_arg);
		if (!handler_to_remove)
			throw std::runtime_error("Handler not registered.");
		auto& list_to_remove_from = handler_to_remove->invoke ? handler_to_remove->invoke->queued_add_handlers : handlers.at(event_id);

		// Let's see if one or more of our invocations have iterators to this handler.
		// (Our invoker may have created the iterator directly pointing to the handler we're removing now,
		// or maybe we incremented/decremented in a previous call to remove_handler() what the invoker created.)
		for (auto inv = first_invoke; inv != nullptr; inv = inv->next)
		{
			if (inv->event_id == event_id)
			{
				// We need to check for has_value() in case we already advanced to the end of enumeration.
				if (inv->next_handler.has_value())
				{
					if (inv->next_handler.value() == handler_to_remove.value())
						inv->move_next();
				}
			}
		}

		list_to_remove_from.erase(handler_to_remove->handler_it);

		// Here we need to keep the list we deleted from. We have iterators to it.
	}
}
