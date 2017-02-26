
#include "pch.h"
#include "Simulator.h"
#include "Resource.h"

using namespace std;

class BridgePropsArea : public IBridgePropsArea
{
	ULONG _refCount = 1;
	HWND _hwnd = nullptr;

public:
	BridgePropsArea (HWND hWndParent, DWORD controlId, const RECT& rect)
	{
		auto hwnd = CreateDialogParamW (nullptr, MAKEINTRESOURCE(IDD_DIALOG_BRIDGE_PROPS), hWndParent, DialogProcStatic, (LPARAM)this);
		assert (hwnd == _hwnd);
		::MoveWindow (_hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, TRUE);
	}

	~BridgePropsArea()
	{
		assert (_refCount == 0);
		if (_hwnd != nullptr)
			::DestroyWindow (_hwnd);
	}

	struct Result
	{
		INT_PTR dialogProcResult;
		LRESULT messageResult;
	};

	static INT_PTR CALLBACK DialogProcStatic (HWND hwnd, UINT uMsg, WPARAM wParam , LPARAM lParam)
	{
		BridgePropsArea* window;
		if (uMsg == WM_INITDIALOG)
		{
			window = reinterpret_cast<BridgePropsArea*>(lParam);
			window->_hwnd = hwnd;
			assert (GetWindowLongPtr(hwnd, GWLP_USERDATA) == 0);
			SetWindowLongPtr (hwnd, GWLP_USERDATA, reinterpret_cast<LPARAM>(window));
		}
		else
			window = reinterpret_cast<BridgePropsArea*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

		if (window == nullptr)
		{
			// this must be one of those messages sent before WM_NCCREATE or after WM_NCDESTROY.
			return DefWindowProc (hwnd, uMsg, wParam, lParam);
		}

		Result result = window->DialogProc (uMsg, wParam, lParam);

		if (uMsg == WM_NCDESTROY)
		{
			window->_hwnd = nullptr;
			SetWindowLongPtr (hwnd, GWLP_USERDATA, 0);
		}

		::SetWindowLong (hwnd, DWL_MSGRESULT, result.messageResult);
		return result.dialogProcResult;
	}

	Result DialogProc (UINT msg, WPARAM wParam , LPARAM lParam)
	{
		return { FALSE, 0 };
	}

	virtual HWND GetHWnd() const override final { return _hwnd; }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override final { return E_NOTIMPL; }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		auto newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
	#pragma endregion
};

extern const BridgePropsAreaFactory bridgePropsAreaFactory = [](auto... params) { return ComPtr<IBridgePropsArea>(new BridgePropsArea(params...), false); };
