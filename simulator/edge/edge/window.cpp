
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "window.h"
#include "rassert.h"

namespace edge
{
	class window : public win32_window_i
	{
		std::shared_ptr<event_manager> const _em = std::make_shared<event_manager>();
		HWND _hwnd;

	public:
		window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, int x, int y, int width, int height)
		{
			auto hm = (HINSTANCE)&__ImageBase;

			WNDCLASSEX copy;
			if (!::GetClassInfoExW(hm, wcex.lpszClassName, &copy))
			{
				copy = wcex;
				copy.cbSize = sizeof(copy);
				copy.lpfnWndProc = window_proc_static;
				copy.hInstance = hm;
				auto atom = RegisterClassEx (&copy);
				rassert (atom != 0);
			}

			_hwnd = ::CreateWindowEx (ex_style, wcex.lpszClassName, L"", style,
				x, y, width, height, parent, nullptr, hm, nullptr); rassert(_hwnd);

			SetWindowLongPtr (_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
		}

		window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, const RECT& rect)
			: window (wcex, ex_style, style, parent, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top)
		{ }

		~window()
		{
			rassert (_hwnd);
			rassert (reinterpret_cast<window*>(GetWindowLongPtr(_hwnd, GWLP_USERDATA)) == this);
			::SetWindowLongPtr (_hwnd, GWLP_USERDATA, 0);
			::DestroyWindow(_hwnd);
		}

		static LRESULT CALLBACK window_proc_static (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
		{
			if (!assert_function_running)
			{
				if (auto w = reinterpret_cast<window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA)))
				{
					if (msg == WM_DESTROY)
					{
						auto classname = std::make_unique<wchar_t>(100);
						GetClassName (hwnd, classname.get(), 100);
						rassert (false); // The HWND wrapped by a "window" object is meant to be destroyed only via ~window().
					}

					if (hwnd != w->_hwnd)
					{
						// Something changed the _hwnd field. Could be that the object was destroyed without
						// calling ~window(), and the VC++ debug runtime filled its memory with 0xDD.
						auto classname = std::make_unique<wchar_t>(100);
						GetClassName (hwnd, classname.get(), 100);
						rassert (false);
					}

					// We consider we have a Z order for handlers: those registered earlier are "in the back",
					// and those registered later are "in the front". The keyboard and mouse messages should go
					// first to the front, which means we need to invoke them in reverse order. The paint messages
					// should go first to the back, which means normal order. For the rest of the messages
					// the order of invocation shouldn't be important.
					bool reverse_invoke = ((msg >= WM_KEYFIRST) && (msg <= WM_KEYLAST))
						|| ((msg >= WM_MOUSEFIRST) && (msg <= WM_MOUSELAST));
					std::optional<LRESULT> result;
					if (reverse_invoke)
						result = window_proc_e::invoker(w->_em).reverse_invoke(hwnd, msg, wparam, lparam);
					else
						result = window_proc_e::invoker(w->_em).invoke(hwnd, msg, wparam, lparam);

					if (result)
						return result.value();
				}
			}

			return DefWindowProc (hwnd, msg, wparam, lparam);
		}

		virtual window_proc_e::subscriber window_proc() override final { return window_proc_e::subscriber(_em.get()); }

		virtual HWND hwnd() const override final { return _hwnd; }
	};

	std::unique_ptr<win32_window_i> make_window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, int x, int y, int width, int height)
	{
		return std::make_unique<window>(wcex, ex_style, style, parent, x, y, width, height);
	}

	std::unique_ptr<win32_window_i> make_window (const WNDCLASSEX& wcex, DWORD ex_style, DWORD style, HWND parent, const RECT& rect)
	{
		return std::make_unique<window>(wcex, ex_style, style, parent, rect);
	}
}

