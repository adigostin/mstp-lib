#pragma once
#include "..\events.h"

namespace edge
{
	struct GdiObjectDeleter
	{
		void operator() (HGDIOBJ object) { ::DeleteObject(object); }
	};
	typedef std::unique_ptr<std::remove_pointer<HFONT>::type, GdiObjectDeleter> HFONT_unique_ptr;

	struct __declspec(novtable) win32_window_i
	{
		virtual ~win32_window_i() { }

		struct destroying_event : public edge::event<destroying_event, win32_window_i*> { };

		virtual destroying_event::subscriber destroying() = 0;
		virtual HWND hwnd() const = 0;

		bool IsVisible() const;

		RECT client_rect_pixels() const;
		SIZE client_size_pixels() const;
		RECT GetRect() const;

	public:
		LONG GetX() const { return GetRect().left; }
		LONG GetY() const { return GetRect().top; }
		POINT GetLocation() const;
		LONG GetWidth() const;
		LONG GetHeight() const;
		SIZE GetSize() const;

		void SetRect (const RECT& rect)
		{
			BOOL bRes = ::MoveWindow (hwnd(), rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
			assert(bRes);
		}

		void SetX (LONG x)
		{
			auto rect = GetRect();
			rect.right = x + rect.right - rect.left;
			rect.left = x;
			SetRect (rect);
		}

		void SetY (LONG y);
		void SetLocation (POINT pt);

		void SetWidth (LONG width)
		{
			auto rect = GetRect();
			rect.right = rect.left + width;
			SetRect(rect);
		}

		void SetHeight (LONG height);
		void SetSize (SIZE size);
	};

	struct zoomable_i abstract
	{
		virtual D2D1_POINT_2F pointd_to_pointw (D2D1_POINT_2F dlocation) const = 0;
		virtual D2D1_POINT_2F pointw_to_pointd (D2D1_POINT_2F wlocation) const = 0;
		virtual float lengthw_to_lengthd (float  wlength) const = 0;
		//virtual zoom_transform_changed_event::subscriber zoom_transform_changed() = 0;
	};
}
