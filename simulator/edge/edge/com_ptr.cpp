
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "com_ptr.h"
#include "utility_functions.h"

namespace edge
{
	com_exception::com_exception (HRESULT hr)
		: _hr(hr)
	{
		wchar_t* buffer = nullptr;
		::FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER|
			FORMAT_MESSAGE_FROM_SYSTEM|
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			hr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&buffer,
			0,
			NULL);

		if (buffer)
		{
			size_t len = wcslen(buffer);
			if (len > 1 && buffer[len - 1] == '\n')
			{
				len--;
				if (len > 1 && buffer[len - 1] == '\r')
					len--;
			}
			_message = utf16_to_utf8({ buffer, buffer + len });
			::LocalFree(buffer);
		}
		else
		{
			char buffer[16];
			sprintf_s(buffer, "0x%0lX", hr);
			_message = buffer;
		}
	}

	co_task_mem_ptr<wchar_t> co_task_mem_alloc (const wchar_t* null_terminated)
	{
		auto len = wcslen(null_terminated);
		auto res = co_task_mem_ptr<wchar_t>(len + 1);
		wcscpy_s (res.get(), len + 1, null_terminated);
		return res;
	}
}
