
#include "event_queue.h"
#include "assert.h"
//#include <stm32f769xx.h>
#include <intrinsics.h>
#include <string.h>
#include <stdio.h>

static bool event_queue_initialized;
uint8_t* buffer;
size_t   capacity;
uint8_t* volatile read_ptr;
uint8_t* volatile write_ptr;
uint32_t volatile used_size;

enum callback_type { callback_type_no_arg, callback_type_void_ptr, callback_type_payload };

struct event
{
	uint16_t _len;
	callback_type type;

	union
	{
		void(*callback_no_arg)();
		void(*callback_void_ptr)(void*);
		void(*callback_buffer)(void*, size_t);
		void* callback;
	};

	const char* debug_name;

	uint8_t payload[0]; // this immediately follows a pointer in the struct, so we're guaranteed good alignment for the payload
};

void event_queue_init (uint8_t* buffer, size_t buffer_size)
{
	assert (!event_queue_initialized);
	assert ((buffer_size & 3) == 0);
	assert (((size_t) buffer & 3) == 0);
	::buffer    = buffer;
	::capacity  = buffer_size;
	::read_ptr  = buffer;
	::write_ptr = buffer;
	::used_size = 0;
	::event_queue_initialized = true;
}

bool event_queue_is_init()
{
	return event_queue_initialized;
}

static bool try_push (void* callback, callback_type ct, const void* payload, size_t payload_size, const char* debug_name)
{
	uint32_t alloc_size = (sizeof(event) + payload_size + 3) / 4 * 4;
	assert (alloc_size < capacity);

	// Turn off interrupts since this function could be running at the same time from interrupt and mainline code.
	__istate_t save = __get_interrupt_state();
	__disable_irq ();

	event* e;
	if (&buffer[capacity] - write_ptr >= alloc_size)
	{
		// no wrap (enough free space)
		e = (event*)write_ptr;
	}
	else
	{
		// wraps - we must allocate space at the beginning of the queue buffer

		// write_ptr should never be positioned less than 4 bytes from the end of the queue buffer
		assert (&buffer[capacity] - write_ptr >= offsetof(event, _len) + sizeof(event::_len));
		((event*)write_ptr)->_len = 0; // this tells the function that pops that we wrapped

		alloc_size += (&buffer[capacity] - write_ptr);

		// is there enough free space?
		if (capacity - used_size >= alloc_size)
		{
			// enough space
			e = (event*)buffer;
		}
		else
		{
			// not enough
			__set_interrupt_state (save);
			return false;
		}
	}

//printf ("w%X", (uintptr_t)write_ptr & 0xFFF);

	e->_len = (uint16_t) (sizeof(event) + payload_size);
	e->type = ct;
	e->callback = callback;
	e->debug_name = debug_name;
	memcpy (e->payload, payload, payload_size);

	write_ptr += alloc_size;
	if (write_ptr >= &buffer[capacity])
		write_ptr -= capacity;

	used_size += alloc_size;
//printf ("s%X\r\n", used_size);

	__set_interrupt_state (save);
	return true;
}

bool event_queue_try_push (void(*handler)(void*, size_t), const void* payload, size_t payload_size, const char* debug_name)
{
	return try_push ((void*)handler, callback_type_payload, payload, payload_size, debug_name);
}

bool event_queue_try_push (void(*handler)(void*), void* arg, const char* debug_name)
{
	return try_push ((void*)handler, callback_type_void_ptr, &arg, sizeof(arg), debug_name);
}

bool event_queue_try_push (void(*handler)(), const char* debug_name)
{
	return try_push ((void*)handler, callback_type_no_arg, NULL, 0, debug_name);
}

static void process_event (event* e)
{
	// TODO: measure duration

	if (e->type == callback_type_no_arg)
		e->callback_no_arg();
	else if (e->type == callback_type_void_ptr)
		e->callback_void_ptr (*(void**)e->payload);
	else if (e->type == callback_type_payload)
		e->callback_buffer (e->payload, e->_len - sizeof(event));
	else
		assert(false);

	// if (show_event_durations)
	//	printf(...);
}

static __arm __interwork bool irq_enabled()
{
	return (__get_CPSR () & 0x80) == 0;
}

void event_queue_pop_all()
{
	assert (event_queue_initialized);
	assert (irq_enabled());

	while (used_size > 0)
	{
		event* e;
		uint32_t popping_size;

		// read_ptr should never be positioned less than x bytes from the end of the queue buffer
		assert (&buffer[capacity] - read_ptr >= offsetof(event, _len) + sizeof(event::_len));

		if (((event*)read_ptr)->_len > 0)
		{
			// no wrap
			e = (event*)read_ptr;
			assert ((uint8_t*)e + e->_len <= &buffer[capacity]);
			popping_size = (e->_len + 3) / 4 * 4;
			assert (used_size >= popping_size);
		}
		else
		{
			// wrap
			e = (event*)buffer;
			assert ((uint8_t*)e + e->_len <= &buffer[capacity]);
			popping_size = (&buffer[capacity] - read_ptr + e->_len + 3) / 4 * 4;
			assert (used_size >= popping_size);
		}

//printf ("r%X", (uintptr_t)read_ptr & 0xFFF);

		process_event(e);

		read_ptr += popping_size;
		if (read_ptr >= &buffer[capacity])
			read_ptr -= capacity;

		__disable_irq();
		__no_operation();
		__no_operation();
		used_size -= popping_size;
//printf ("s%X\r\n", used_size);
		__enable_irq();
	}
}
