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

struct HWndDeleter
{
	void operator() (HWND hWnd) { ::DestroyWindow(hWnd); }
};
typedef std::unique_ptr<std::remove_pointer<HWND>::type, HWndDeleter> HWND_unique_ptr;

struct TimerQueueTimerDeleter
{
	void operator() (HANDLE handle) { ::DeleteTimerQueueTimer (nullptr, handle, INVALID_HANDLE_VALUE); }
};
typedef std::unique_ptr<std::remove_pointer<HANDLE>::type, TimerQueueTimerDeleter> TimerQueueTimer_unique_ptr;

template<typename T> class ComPtr
{
	T* m_ComPtr;
public:
	ComPtr()
		: m_ComPtr(nullptr)
	{
		static_assert (std::is_base_of<IUnknown, T>::value, "T needs to be IUnknown based");
	}

	ComPtr(T* lComPtr, bool addRef = true)
		: m_ComPtr(lComPtr)
	{
		static_assert (std::is_base_of<IUnknown, T>::value, "T needs to be IUnknown based");
		if ((m_ComPtr != nullptr) && addRef)
			m_ComPtr->AddRef();
	}

	ComPtr(const ComPtr<T>& lComPtrObj)
	{
		static_assert (std::is_base_of<IUnknown, T>::value, "T needs to be IUnknown based");
		m_ComPtr = lComPtrObj.m_ComPtr;
		if (m_ComPtr)
			m_ComPtr->AddRef();
	}

	ComPtr(ComPtr<T>&& lComPtrObj)
	{
		m_ComPtr = lComPtrObj.m_ComPtr;
		lComPtrObj.m_ComPtr = nullptr;
	}

	_Check_return_ HRESULT CoCreateInstance(
		_In_ REFCLSID rclsid,
		_Inout_opt_ LPUNKNOWN pUnkOuter = NULL,
		_In_ DWORD dwClsContext = CLSCTX_ALL) throw()
	{
		assert(m_ComPtr == NULL);
		return ::CoCreateInstance(rclsid, pUnkOuter, dwClsContext, __uuidof(T), (void**)&m_ComPtr);
	}

	T* operator=(T* lComPtr)
	{
		if (m_ComPtr)
			m_ComPtr->Release();

		m_ComPtr = lComPtr;

		if (m_ComPtr)
			m_ComPtr->AddRef();

		return m_ComPtr;
	}

	T* operator=(const ComPtr<T>& lComPtrObj)
	{
		if (m_ComPtr)
			m_ComPtr->Release();

		m_ComPtr = lComPtrObj.m_ComPtr;

		if (m_ComPtr)
			m_ComPtr->AddRef();

		return m_ComPtr;
	}

	~ComPtr()
	{
		static_assert (std::is_base_of<IUnknown, T>::value, "T needs to be IUnknown based");
		if (m_ComPtr)
		{
			m_ComPtr->Release();
			m_ComPtr = nullptr;
		}
	}

	// Attach to an interface (does not call AddRef)
	void Attach(T* ptr)
	{
		assert(ptr != nullptr);

		if (m_ComPtr != nullptr)
			m_ComPtr->Release();

		m_ComPtr = ptr;
	}

	operator T*() const
	{
		return m_ComPtr;
	}

	T* Get() const
	{
		return m_ComPtr;
	}

	T* const* GetInterfacePtr() const
	{
		return &m_ComPtr;
	}

	T** operator&()
	{
		//The assert on operator& usually indicates a bug. Could be a potential memory leak.
		// If this really what is needed, however, use GetInterface() explicitly.
		assert(nullptr == m_ComPtr);
		return &m_ComPtr;
	}

	T* operator->() const
	{
		return m_ComPtr;
	}

	bool operator==(const ComPtr<T>& other) const
	{
		return this->m_ComPtr == other.m_ComPtr;
	}

	bool operator==(T* other) const
	{
		return this->m_ComPtr == other;
	}

	template <typename I>
	HRESULT QueryInterface(I **interfacePtr)
	{
		return m_ComPtr->QueryInterface(IID_PPV_ARGS(interfacePtr));
	}
};


template<typename T> class ComTaskMemPtr
{
	T* _ptr;

public:
	ComTaskMemPtr()
		: _ptr(nullptr)
	{ }

	ComTaskMemPtr(const ComTaskMemPtr&) = delete;
	ComTaskMemPtr(ComTaskMemPtr&&) = delete;
	ComTaskMemPtr& operator= (const ComTaskMemPtr&) = delete;
	ComTaskMemPtr& operator= (ComTaskMemPtr&&) = delete;

	~ComTaskMemPtr()
	{
		if (_ptr != nullptr)
		{
			CoTaskMemFree(_ptr);
			_ptr = nullptr;
		}
	}

	T** operator&()
	{
		//The assert on operator& usually indicates a bug. Could be a potential memory leak.
		// If this really what is needed, however, use GetPtr() explicitly.
		assert(_ptr == nullptr);
		return &_ptr;
	}

	T* GetPtr() const
	{
		return _ptr;
	}
};
