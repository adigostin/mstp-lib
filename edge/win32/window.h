
#pragma once
#include "win32_lib.h"

namespace edge
{
	enum class handled { no = 0, yes = 1 };

	enum class mouse_button { left, right, middle, };

	struct wnd_class_params
	{
		LPCWSTR lpszClassName;
		UINT    style;
		LPCWSTR lpszMenuName;
		LPCWSTR lpIconName;
		LPCWSTR lpIconSmName;
	};

	class window : public event_manager, public virtual win32_window_i
	{
		void register_class (HINSTANCE hInstance, const wnd_class_params& class_params);

	public:
		window (HINSTANCE hInstance, DWORD exStyle, DWORD style, const RECT& rect, HWND hWndParent, int child_control_id);
		window (HINSTANCE hInstance, const wnd_class_params& class_params, DWORD exStyle, DWORD style, int x, int y, int width, int height, HWND hWndParent, HMENU hMenu);
	protected:
		virtual ~window();

		virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		static UINT GetModifierKeys();

	public:
		SIZE client_size_pixels() const { return _clientSize; }
		LONG client_width_pixels() const { return _clientSize.cx; }
		LONG client_height_pixels() const { return _clientSize.cy; }
		RECT client_rect_pixels() const { return { 0, 0, _clientSize.cx, _clientSize.cy }; }

		HWND hwnd() const { return _hwnd; }
		destroying_event::subscriber destroying() { return destroying_event::subscriber(this); }

	private:
		HWND _hwnd = nullptr;
		SIZE _clientSize;

		static LRESULT CALLBACK WindowProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
	};
}
