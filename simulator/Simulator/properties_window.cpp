
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "pg/property_grid.h"

using namespace edge;
using namespace pg;

class properties_window : public IPropertiesWindow, IObjectCollectionChangeEvents
{
	ULONG _refCount = 0;
	WeakRefToThis _weakRefToThis;
	ISimulatorApp* _app;
	DWORD _selectedVlan;
	wil::unique_hwnd _hWnd;
	com_ptr<IPropertyGrid> _pg;
	com_ptr<IObjectList> _selection;
	com_ptr<IObjectList> _treeSelection;
	com_ptr<IVlanSelection> _vlanSel;
	AdviseSinkToken _selectionChangeToken;

public:
	HRESULT InitInstance (const properties_window_create_params& cps)
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);

		_app = cps.app;
		_selection = cps.selection;
		_vlanSel = cps.vlanSel;

		static const WNDCLASS wnd_class = {
			.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = WndProc,
			.hInstance = (HINSTANCE)&__ImageBase,
			.hCursor = ::LoadCursor(nullptr, IDC_ARROW),
			.lpszClassName = L"properties_window",
		};

		auto atom = RegisterClass(&wnd_class);

		int x = cps.rect.left;
		int y = cps.rect.top;
		int w = cps.rect.right - cps.rect.left;
		int h = cps.rect.bottom - cps.rect.top;
		_hWnd.reset (CreateWindowEx (WS_EX_CLIENTEDGE, wnd_class.lpszClassName, L"", WS_CHILD | WS_VISIBLE,
									 x, y, w, h, cps.hwnd_parent, nullptr, (HINSTANCE)&__ImageBase, nullptr));
		RETURN_LAST_ERROR_IF_NULL(_hWnd);
		SetWindowLongPtr (_hWnd.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		hr = MakePropertyGrid (_hWnd.get(), edge::client_rect_pixels(_hWnd.get()), cps.app->GetThemeColorProvider(), &_pg); RETURN_IF_FAILED(hr);

		_pg->AddSection (_selection, true, nullptr);
		if (_selection->size()
			&& (wil::try_com_query_nothrow<IBridge>(_selection->front()) || wil::try_com_query_nothrow<IPort>(_selection->front())))
		{
			hr = MakeTreeSelection (_selection, cps.vlanSel, &_treeSelection); RETURN_IF_FAILED(hr);
			_pg->AddSection (_treeSelection, false, nullptr);
		}

		//hr = AdviseSink<IVlanSelectionEvents>(cps.vlanSel, _weakRefToThis, &_vlanChangeToken); RETURN_IF_FAILED(hr);

		hr = AdviseSink<IObjectCollectionChangeEvents>(_selection, _weakRefToThis, &_selectionChangeToken); RETURN_IF_FAILED(hr);

		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IPropertiesWindow*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IPropertiesWindow>(this, riid, ppvObject)
			|| TryQI<IObjectCollectionChangeEvents>(this, riid, ppvObject)
			//|| TryQI<IVlanSelectionEvents>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanging (IUnknown *sender, const struct ObjectCollectionChangeArgs *args) override
	{
		HRESULT hr;

		if (args->changeType == CollectionChangeType::Insert)
		{
			auto iargs = &args->setInsertRemoveArgs;

			if (_selection->empty())
			{
				// Adding items to an empty selection.
				hr = AllSameType(iargs->childObjs, iargs->childObjs + iargs->count); RETURN_IF_FAILED(hr);
				RETURN_HR_IF(E_UNEXPECTED, hr != S_OK);

				if (wil::try_com_query_nothrow<IBridge>(iargs->childObjs[0])
					|| wil::try_com_query_nothrow<IPort>(iargs->childObjs[0]))
				{
					com_ptr<IVlanSelection> vlanSel;
					hr = _vlanSel->QueryInterface(&vlanSel); RETURN_IF_FAILED(hr);
					hr = MakeTreeSelection (_selection, vlanSel, &_treeSelection); RETURN_IF_FAILED(hr);
					_pg->AddSection (_treeSelection, false, nullptr);
					return S_OK;
				}
				else if (wil::try_com_query_nothrow<IWire>(iargs->childObjs[0]))
				{
					return S_OK;
				}
				else
					RETURN_HR(E_NOTIMPL);
			}
			else
				return S_OK;
		}
		else if (args->changeType == CollectionChangeType::Remove)
		{
			return S_OK;
		}
		else
			RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE OnCollectionChanged (IUnknown *sender, const struct ObjectCollectionChangeArgs *args) override
	{
		HRESULT hr;

		if (args->changeType == CollectionChangeType::Insert)
		{
			return S_OK;
		}
		else if (args->changeType == CollectionChangeType::Remove)
		{
			auto iargs = &args->setInsertRemoveArgs;

			if (_selection->empty())
			{
				// Removed all items from the selection.
				hr = AllSameType(iargs->childObjs, iargs->childObjs + iargs->count); RETURN_IF_FAILED(hr);
				RETURN_HR_IF(E_UNEXPECTED, hr != S_OK);

				if (wil::try_com_query_nothrow<IBridge>(iargs->childObjs[0])
					|| wil::try_com_query_nothrow<IPort>(iargs->childObjs[0]))
				{
					hr = _pg->RemoveSection(_treeSelection); RETURN_IF_FAILED(hr);
					auto raw = _treeSelection.detach();
					ULONG refCount = raw->Release();
					_ASSERT(refCount == 0);
					return S_OK;
				}
				else if (wil::try_com_query_nothrow<IWire>(iargs->childObjs[0]))
				{
					return S_OK;
				}
				else
					RETURN_HR(E_NOTIMPL);
			}

			return S_OK;
		}
		else
			RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	// properties_window_i
	virtual HWND hWnd() const override { return _hWnd.get(); }

	virtual IPropertyGrid* pg() const override { return _pg.get(); }

	static LRESULT CALLBACK WndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (auto pw = reinterpret_cast<properties_window*>(GetWindowLongPtr(hWnd, GWLP_USERDATA)))
		{
			if (uMsg == WM_SIZE)
			{
				RECT r;
				BOOL bres = ::GetClientRect(hWnd, &r); RETURN_IF_WIN32_BOOL_FALSE(bres);
				pw->_pg->SetBounds(r);
				::UpdateWindow(hWnd);
			}
		}

		return DefWindowProc (hWnd, uMsg, wParam, lParam);
	}
};

HRESULT MakePropertiesWindow (const properties_window_create_params& cps, IPropertiesWindow** ppPW)
{
	auto p = com_ptr(new (std::nothrow) properties_window()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(cps); RETURN_IF_FAILED(hr);
	*ppPW = p.detach();
	return S_OK;
}
