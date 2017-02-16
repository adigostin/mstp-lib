#pragma once

class ComException : public std::exception
{
	HRESULT const _hr;

public:
	ComException(HRESULT hr) : _hr(hr) { }
};

class Win32Exception : public std::exception
{
	DWORD const _lastError;

public:
	Win32Exception(DWORD lastError) : _lastError(lastError) { }
};

struct IWin32Window abstract
{
	virtual HWND GetHWnd() const = 0;
};

inline void ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
		throw ComException(hr);
}
