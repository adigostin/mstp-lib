#pragma once
#include <assert.h>

class com_exception : public std::exception
{
public:
	HRESULT const _hr;
	com_exception(HRESULT hr) : _hr(hr) { }
};

class win32_exception : public std::exception
{
public:
	DWORD const _lastError;
	win32_exception(DWORD lastError) : _lastError(lastError) { }
};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
		throw com_exception(hr);
}

inline void ThrowWin32IfFailed(BOOL bRes)
{
	if (!bRes)
		throw win32_exception(GetLastError());
}

class not_implemented_exception : public std::exception
{
public:
	not_implemented_exception()
		: std::exception("Not implemented.")
	{ }
};

struct IZoomable abstract
{
	virtual D2D1_POINT_2F GetWLocationFromDLocation (D2D1_POINT_2F dLocation) const = 0;
	virtual D2D1_POINT_2F GetDLocationFromWLocation (D2D1_POINT_2F wLocation) const = 0;
	virtual float GetDLengthFromWLength (float wLength) const = 0;
};

struct GdiObjectDeleter
{
	void operator() (HGDIOBJ object) { ::DeleteObject(object); }
};
typedef std::unique_ptr<std::remove_pointer<HFONT>::type, GdiObjectDeleter> HFONT_unique_ptr;

struct TimerQueueTimerDeleter
{
	void operator() (HANDLE handle) { ::DeleteTimerQueueTimer (nullptr, handle, INVALID_HANDLE_VALUE); }
};
typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, TimerQueueTimerDeleter> TimerQueueTimer_unique_ptr;
