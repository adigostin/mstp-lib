#pragma once

class NotImplementedException : public std::exception
{ };

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

class NullArgumentException : public std::exception
{ };

class InvalidOperationException : public std::exception
{
	std::wstring _message;

public:
	InvalidOperationException() = default;
	InvalidOperationException (const wchar_t* message) : _message(message) { }
	InvalidOperationException (const std::wstring& message) : _message(message) { }
	InvalidOperationException (std::wstring&& message) : _message(move(message)) { }
};