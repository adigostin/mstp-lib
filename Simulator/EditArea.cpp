
#include "pch.h"
#include "SimulatorDefs.h"
#include "ZoomableWindow.h"
#include "Ribbon/RibbonIds.h"
#include "EditStates/EditState.h"
#include "Bridge.h"

using namespace std;
using namespace D2D1;

static constexpr float SnappingDistance = 6;

class EditArea : public ZoomableWindow, public IEditArea
{
	typedef ZoomableWindow base;

	ULONG _refCount = 1;
	IProjectWindow* const _pw;
	IUIFramework* const _rf;
	ComPtr<ISelection> const _selection;
	ComPtr<IProject> const _project;
	ComPtr<IDWriteFactory> const _dWriteFactory;
	DrawingObjects _drawingObjects;
	uint16_t _selectedVlanNumber = 1;
	unique_ptr<EditState> _state;
	Port* _hoverPort = nullptr;

	struct BeginningDrag
	{
		POINT pt;
		D2D1_POINT_2F dLocation;
		D2D1_POINT_2F wLocation;
		MouseButton button;
		HTCodeAndObject ht;
	};

	optional<BeginningDrag> _beginningDrag;

public:
	EditArea(IProject* project, IProjectWindow* pw, DWORD controlId, ISelection* selection, IUIFramework* rf, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
		: base (WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, rect, pw->GetHWnd(), controlId, deviceContext, dWriteFactory, wicFactory)
		, _project(project), _pw(pw), _rf(rf), _selection(selection), _dWriteFactory(dWriteFactory)
	{
		_selection->GetSelectionChangedEvent().AddHandler (&OnSelectionChanged, this);
		_project->GetObjectRemovingEvent().AddHandler (&OnObjectRemoving, this);
		_project->GetProjectInvalidateEvent().AddHandler (&OnProjectInvalidate, this);
		auto dc = base::GetDeviceContext();
		auto hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green), &_drawingObjects._poweredOutlineBrush); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::PaleGreen), &_drawingObjects._poweredFillBrush); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_drawingObjects._unpoweredBrush); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Red), &_drawingObjects._brushDiscardingPort); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gold), &_drawingObjects._brushLearningPort); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green), &_drawingObjects._brushForwarding); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_drawingObjects._brushNoForwardingWire); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Blue), &_drawingObjects._brushTempWire); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &_drawingObjects._brushWindowText); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOW), &_drawingObjects._brushWindow); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_HIGHLIGHT), &_drawingObjects._brushHighlight); ThrowIfFailed(hr);
		hr = _dWriteFactory->CreateTextFormat (L"Tahoma", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 14, L"en-US", &_drawingObjects._regularTextFormat); ThrowIfFailed(hr);
	}

	virtual ~EditArea()
	{
		assert (_refCount == 0);
		_project->GetProjectInvalidateEvent().RemoveHandler(&OnProjectInvalidate, this);
		_project->GetObjectRemovingEvent().RemoveHandler (&OnObjectRemoving, this);
		_selection->GetSelectionChangedEvent().RemoveHandler (&OnSelectionChanged, this);
	}

	static void OnObjectRemoving (void* callbackArg, IProject* project, size_t index, Object* o)
	{
		auto area = static_cast<EditArea*>(callbackArg);
		if (area->_hoverPort->GetBridge() == o)
		{
			area->_hoverPort = nullptr;
			InvalidateRect (area->GetHWnd(), nullptr, FALSE);
		}
	}

	static void OnProjectInvalidate (void* callbackArg, IProject*)
	{
		auto area = static_cast<EditArea*>(callbackArg);
		::InvalidateRect (area->GetHWnd(), nullptr, FALSE);
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto editArea = static_cast<EditArea*>(callbackArg);
		::InvalidateRect (editArea->GetHWnd(), nullptr, FALSE);
	}

	struct LegendInfoEntry
	{
		const wchar_t* text;
		STP_PORT_ROLE role;
		bool learning;
		bool forwarding;
		bool operEdge;
	};

	static constexpr LegendInfoEntry LegendInfo[] =
	{
		{ L"Disabled",							STP_PORT_ROLE_DISABLED,   false, false, false },
		
		{ L"Designated discarding",				STP_PORT_ROLE_DESIGNATED, false, false, false },
		{ L"Designated learning",				STP_PORT_ROLE_DESIGNATED, true,  false, false },
		{ L"Designated forwarding",				STP_PORT_ROLE_DESIGNATED, true,  true,  false },
		{ L"Designated forwarding operEdge",	STP_PORT_ROLE_DESIGNATED, true,  true,  true  },
		
		{ L"Root/Master discarding",			STP_PORT_ROLE_ROOT,       false, false, false },
		{ L"Root/Master learning",				STP_PORT_ROLE_ROOT,       true,  false, false },
		{ L"Root/Master forwarding",			STP_PORT_ROLE_ROOT,       true,  true,  false },
		
		{ L"Alternate discarding",				STP_PORT_ROLE_ALTERNATE,  false, false, false },
		{ L"Alternate learning",				STP_PORT_ROLE_ALTERNATE,  true,  false, false },
		
		{ L"Backup discarding",					STP_PORT_ROLE_BACKUP,     false, false, false },
		{ L"Undefined",							STP_PORT_ROLE_UNKNOWN,    false, false, false },
	};

	void RenderLegend (ID2D1DeviceContext* dc) const
	{
		auto clientSizeDips = GetClientSizeDips();

		float maxLineWidth = 0;
		float maxLineHeight = 0;
		vector<ComPtr<IDWriteTextLayout>> layouts;
		for (auto& info : LegendInfo)
		{
			ComPtr<IDWriteTextLayout> tl;
			auto hr = _dWriteFactory->CreateTextLayout (info.text, wcslen(info.text), _drawingObjects._regularTextFormat, 1000, 1000, &tl); ThrowIfFailed(hr);

			DWRITE_TEXT_METRICS metrics;
			tl->GetMetrics (&metrics);

			if (metrics.width > maxLineWidth)
				maxLineWidth = metrics.width;

			if (metrics.height > maxLineHeight)
				maxLineHeight = metrics.height;

			layouts.push_back(move(tl));
		}

		float textX = clientSizeDips.width - (5 + maxLineWidth + 5 + PortExteriorHeight + 5);
		float bitmapX = clientSizeDips.width - (5 + PortExteriorHeight + 5);
		float rowHeight = 2 + max (maxLineHeight, PortExteriorWidth);
		float y = clientSizeDips.height - _countof(LegendInfo) * rowHeight;

		Matrix3x2F oldTransform;
		dc->GetTransform (&oldTransform);

		for (size_t i = 0; i < _countof(LegendInfo); i++)
		{
			auto& info = LegendInfo[i];

			auto oldaa = dc->GetAntialiasMode();
			dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
			dc->DrawLine (Point2F (textX, y), Point2F (clientSizeDips.width, y), _drawingObjects._brushWindowText);
			dc->SetAntialiasMode(oldaa);

			dc->DrawTextLayout (Point2F (textX, y + 1), layouts[i], _drawingObjects._brushWindowText);

			// Rotate 270 degrees and then translate.
			Matrix3x2F trans (0.0f, -1.0f, 1.0f, 0.0f, bitmapX, y + rowHeight / 2);
			trans.SetProduct (oldTransform, trans);
			dc->SetTransform (&trans);

			Bridge::RenderExteriorStpPort (dc, _drawingObjects, info.role, info.learning, info.forwarding, info.operEdge);

			dc->SetTransform (&oldTransform);

			y += rowHeight;
		}
	}

	void RenderSelectionRectangles (ID2D1DeviceContext* dc) const
	{
		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		for (auto& o : _selection->GetObjects())
		{
			if (auto b = dynamic_cast<Bridge*>(o.Get()))
			{
				auto tl = GetDLocationFromWLocation ({ b->GetLeft() - BridgeOutlineWidth / 2, b->GetTop() - BridgeOutlineWidth / 2 });
				auto br = GetDLocationFromWLocation ({ b->GetRight() + BridgeOutlineWidth / 2, b->GetBottom() + BridgeOutlineWidth / 2 });

				ComPtr<ID2D1Factory> factory;
				dc->GetFactory(&factory);
				D2D1_STROKE_STYLE_PROPERTIES ssprops = {};
				ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
				ComPtr<ID2D1StrokeStyle> ss;
				auto hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &ss); ThrowIfFailed(hr);
				dc->DrawRectangle ({ tl.x - 10, tl.y - 10, br.x + 10, br.y + 10 }, _drawingObjects._brushHighlight, 2, ss);
			}
		}

		dc->SetAntialiasMode(oldaa);
	}

	void RenderHoverCP (ID2D1RenderTarget* rt) const
	{
		auto oldaa = rt->GetAntialiasMode();
		rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		auto cpw = _hoverPort->GetConnectionPointLocation();
		auto cpd = GetDLocationFromWLocation(cpw);
		auto rect = RectF (cpd.x - SnappingDistance, cpd.y - SnappingDistance, cpd.x + SnappingDistance, cpd.y + SnappingDistance);
		rt->DrawRectangle (rect, _drawingObjects._brushHighlight, 2);

		rt->SetAntialiasMode(oldaa);
	}
	/*
	void RenderWire (Wire* wire, unsigned short selectedVlanNumber)
	{
		ID2D1SolidColorBrush* brush = _brushNoForwardingWire;
		ID2D1StrokeStyle* strokeStyle = _strokeStyleNoForwardingWire;

		// If both ends are forwarding, make the wire thick.
		ConnectedWireEnd* firstConnectedEnd  = dynamic_cast<ConnectedWireEnd*> (wire->GetFirstEnd  ());
		ConnectedWireEnd* secondConnectedEnd = dynamic_cast<ConnectedWireEnd*> (wire->GetSecondEnd ());
		if ((firstConnectedEnd != NULL) && (secondConnectedEnd != NULL)
			&& firstConnectedEnd->Port->GetMacOperational () && secondConnectedEnd->Port->GetMacOperational ())
		{
			STP_BRIDGE* stpBridge = firstConnectedEnd->Port->Bridge->LockStpBridge ();
			byte oneEndTreeIndex = STP_GetTreeIndexFromVlanNumber (stpBridge, selectedVlanNumber);
			bool oneForwarding = STP_GetPortForwarding (stpBridge, firstConnectedEnd->Port->PortIndex, oneEndTreeIndex);
			firstConnectedEnd->Port->Bridge->UnlockStpBridge ();

			if (oneForwarding)
			{
				stpBridge = secondConnectedEnd->Port->Bridge->LockStpBridge ();
				byte otherEndTreeIndex = STP_GetTreeIndexFromVlanNumber (stpBridge, selectedVlanNumber);
				bool otherForwarding = STP_GetPortForwarding (stpBridge, secondConnectedEnd->Port->PortIndex, otherEndTreeIndex);
				secondConnectedEnd->Port->Bridge->UnlockStpBridge ();

				if (otherForwarding)
				{
					brush = _brushForwardingWire;
					strokeStyle = NULL;
				}
			}
		}

		_renderTarget->DrawLine (wire->GetFirstEnd ()->GetLocation (), wire->GetSecondEnd ()->GetLocation (), brush, 2.0f, strokeStyle);
	}
	*/
	void RenderObjects (ID2D1DeviceContext* dc) const
	{
		D2D1_MATRIX_3X2_F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform(GetZoomTransform());
		for (auto& o : _project->GetObjects())
		{
			if (auto b = dynamic_cast<Bridge*>(o.Get()))
				b->Render (dc, _drawingObjects, _dWriteFactory, _selectedVlanNumber);
			else
				throw not_implemented_exception();
		}
		dc->SetTransform(oldtr);
	}

	virtual void Render(ID2D1DeviceContext* dc) const override final
	{
		dc->Clear(GetD2DSystemColor(COLOR_WINDOW));

		auto clientSizeDips = GetClientSizeDips();

		HRESULT hr;

		if (none_of (_project->GetObjects().begin(), _project->GetObjects().end(), [](auto&& o) { return dynamic_cast<Bridge*>(o.Get()) != nullptr; }))
		{
			auto text = L"No bridges created. Right-click to create some.";
			ComPtr<IDWriteTextFormat> tf;
			hr = _dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16, L"en-US", &tf); ThrowIfFailed(hr);
			ComPtr<IDWriteTextLayout> tl;
			hr = _dWriteFactory->CreateTextLayout (text, wcslen(text), tf, 10000, 10000, &tl); ThrowIfFailed(hr);
			DWRITE_TEXT_METRICS metrics;
			hr = tl->GetMetrics(&metrics); ThrowIfFailed(hr);
			D2D1_POINT_2F origin = { clientSizeDips.width / 2 - metrics.width / 2, clientSizeDips.height / 2 - metrics.height / 2 };
			dc->DrawTextLayout (origin, tl, _drawingObjects._brushWindowText);
		}
		else
		{
			RenderLegend(dc);
			RenderObjects(dc);
			RenderSelectionRectangles(dc);
			if (_hoverPort != nullptr)
				RenderHoverCP(dc);
		}

		if (_state != nullptr)
			_state->Render(dc);
	}

	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		if ((uMsg == WM_LBUTTONDOWN) || (uMsg == WM_RBUTTONDOWN))
		{
			auto button = (uMsg == WM_LBUTTONDOWN) ? MouseButton::Left : MouseButton::Right;
			auto result = ProcessMouseButtonDown (button, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return result ? result.value() : base::WindowProc (hwnd, uMsg, wParam, lParam);
		}
		else if ((uMsg == WM_LBUTTONUP) || (uMsg == WM_RBUTTONUP))
		{
			auto button = (uMsg == WM_LBUTTONUP) ? MouseButton::Left : MouseButton::Right;
			auto result = ProcessMouseButtonUp (button, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return result ? result.value() : base::WindowProc (hwnd, uMsg, wParam, lParam);
		}
		else if (uMsg == WM_SETCURSOR)
		{
			if (((HWND) wParam == GetHWnd()) && (LOWORD (lParam) == HTCLIENT))
			{
				// Let's check the result because GetCursorPos fails when the input desktop is not the current desktop
				// (happens for example when the monitor goes to sleep and then the lock screen is displayed).
				POINT pt;
				if (::GetCursorPos (&pt))
				{
					if (ScreenToClient (GetHWnd(), &pt))
					{
						this->ProcessWmSetCursor(pt);
						return TRUE;
					}
				}
			}

			return nullopt;
		}
		else if (uMsg == WM_MOUSEMOVE)
		{
			ProcessWmMouseMove (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			base::WindowProc (hwnd, uMsg, wParam, lParam);
			return 0;
		}
		else if (uMsg == WM_CONTEXTMENU)
		{
			return ProcessWmContextMenu (hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
		}

		return base::WindowProc (hwnd, uMsg, wParam, lParam);
	}

	HTCodeAndObject HitTestObjects (D2D1_POINT_2F dipLocation, float tolerance) const
	{
		for (auto& o : _project->GetObjects())
		{
			if (auto b = dynamic_cast<Bridge*>(o.Get()))
			{
				for (auto& port : b->GetPorts())
				{
					auto cpWLocation = port->GetConnectionPointLocation();
					auto cpDLocation = GetDLocationFromWLocation(cpWLocation);
					if ((abs (cpDLocation.x - dipLocation.x) <= tolerance) && (abs (cpDLocation.y - dipLocation.y) <= tolerance))
						return { HTCode::PortConnectionPoint, port };
				}

				if ((dipLocation.x >= b->GetLeft()) && (dipLocation.x < b->GetRight())
					&& (dipLocation.y >= b->GetTop()) && (dipLocation.y < b->GetBottom()))
				{
					return { HTCode::BridgeInner, b };
				}
			}
			else
				throw not_implemented_exception();
		}

		return { HTCode::Nothing, nullptr };
	};
	/*
	Port* GetConnectionPointAt (D2D1_POINT_2F dipLocation, float snappingDistance)
	{
		for (auto& bridge : _project->GetBridges())
		{
			for (auto& port : bridge->GetPorts())
			{
				auto cpWLocation = port->GetConnectionPointLocation();
				auto cpDLocation = GetDLocationFromWLocation(cpWLocation);

				if ((abs (cpDLocation.x - dipLocation.x) <= snappingDistance) && (abs (cpDLocation.y - dipLocation.y) <= snappingDistance))
					return port.get();
			}
		}

		return nullptr;
	}
	*/
	std::optional<LRESULT> ProcessMouseButtonDown (MouseButton button, POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		auto hitTestedObject = HitTestObjects (dLocation, SnappingDistance);

		if (_state != nullptr)
		{
			_state->OnMouseDown (dLocation, wLocation, button, hitTestedObject);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};
			
			return 0;
		}

		if (hitTestedObject.object == nullptr)
			_selection->Clear();
		else
			_selection->Select(hitTestedObject.object);

		if (!_beginningDrag)
		{
			_beginningDrag = BeginningDrag { pt, dLocation, wLocation, button, hitTestedObject };
			return 0;
		}

		return nullopt;
	}

	std::optional<LRESULT> ProcessMouseButtonUp (MouseButton button, POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_state != nullptr)
		{
			_state->OnMouseUp (dLocation, wLocation, button);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return 0;
		}

		if (_beginningDrag && (button == _beginningDrag->button))
			_beginningDrag = nullopt;

		if (button == MouseButton::Right)
			return nullopt; // return "not handled", to cause our called to pass the message to DefWindowProc, which will generate WM_CONTEXTMENU

		return 0;
	}

	virtual EditStateDeps MakeEditStateDeps() override final
	{
		return EditStateDeps { _project, this, _selection };
	}

	virtual void EnterState (std::unique_ptr<EditState>&& state) override final
	{
		_beginningDrag = nullopt;
		_state = move(state);
	}

	void ProcessWmSetCursor (POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_beginningDrag)
		{
		}
		else if (_state != nullptr)
		{
			::SetCursor (GetCursor());
		}
		else
		{
			auto ht = HitTestObjects(dLocation, SnappingDistance);
			if (ht.code == HTCode::PortConnectionPoint)
				::SetCursor (LoadCursor(nullptr, IDC_CROSS));
			else
				::SetCursor (LoadCursor(nullptr, IDC_ARROW));
		}
	}

	void ProcessWmMouseMove (POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_beginningDrag)
		{
			RECT rc = { _beginningDrag->pt.x, _beginningDrag->pt.y, _beginningDrag->pt.x, _beginningDrag->pt.y };
			InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
			if (!PtInRect (&rc, pt))
			{
				if (_beginningDrag->ht.code == HTCode::Nothing)
				{
					// TODO: area selection
				}
				else if (_beginningDrag->ht.code == HTCode::BridgeInner)
				{
					if (_beginningDrag->button == MouseButton::Left)
						_state = CreateStateMoveBridges (MakeEditStateDeps());
				}
				//else if (auto b = dynamic_cast<Port*>(_beginningDrag->clickedObj))
				//{
					// TODO: move port
					//if (_beginningDrag->button == MouseButton::Left)
					//	_state = CreateStateMovePorts (this, _selection);
				//}
				else if (_beginningDrag->ht.code == HTCode::PortConnectionPoint)
				{
					_state = CreateStateCreateWire(MakeEditStateDeps());
				}
				else
					throw not_implemented_exception();

				if (_state != nullptr)
				{
					_state->OnMouseDown (_beginningDrag->dLocation, _beginningDrag->wLocation, _beginningDrag->button, _beginningDrag->ht);
					assert (!_state->Completed());
					_state->OnMouseMove (dLocation, wLocation);
				}

				_beginningDrag = nullopt;
			}
		}
		else if (_state != nullptr)
		{
			_state->OnMouseMove (dLocation, wLocation);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			}
		}
		else
		{
			auto ht = HitTestObjects(dLocation, SnappingDistance);

			if (((ht.code == HTCode::PortConnectionPoint) && (ht.object != _hoverPort))
				|| ((ht.code != HTCode::PortConnectionPoint) && (_hoverPort != nullptr)))
			{
				_hoverPort = dynamic_cast<Port*>(ht.object);
				InvalidateRect (GetHWnd(), nullptr, FALSE);
			}
		}
	}

	std::optional<LRESULT> ProcessWmContextMenu (HWND hwnd, POINT pt)
	{
		//D2D1_POINT_2F dipLocation = GetDipLocationFromPixelLocation(pt);
		//_elementsAtContextMenuLocation.clear();
		//GetElementsAt(_project->GetInnerRootElement(), { dipLocation.x, dipLocation.y }, _elementsAtContextMenuLocation);

		UINT32 viewId;
		if (_selection->GetObjects().empty())
			viewId = cmdContextMenuBlankArea;
		else if (dynamic_cast<Bridge*>(_selection->GetObjects()[0].Get()) != nullptr)
			viewId = cmdContextMenuBridge;
		else if (dynamic_cast<Port*>(_selection->GetObjects()[0].Get()) != nullptr)
			viewId = cmdContextMenuPort;
		else
			throw not_implemented_exception();

		ComPtr<IUIContextualUI> ui;
		auto hr = _rf->GetView(viewId, IID_PPV_ARGS(&ui)); ThrowIfFailed(hr);
		hr = ui->ShowAtLocation(pt.x, pt.y); ThrowIfFailed(hr);
		return 0;
	}

	virtual void SelectVlan (uint16_t vlanNumber) override final
	{
		if (_selectedVlanNumber != vlanNumber)
		{
			_selectedVlanNumber = vlanNumber;
			::InvalidateRect (GetHWnd(), nullptr, FALSE);
		}
	};

	virtual uint16_t GetSelectedVlanNumber() const override final { return _selectedVlanNumber; }

	virtual const DrawingObjects& GetDrawingObjects() const override final { return _drawingObjects; }

	virtual IDWriteFactory* GetDWriteFactory() const override final { return _dWriteFactory; }

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

extern const EditAreaFactory editAreaFactory = [](auto... params) { return ComPtr<IEditArea>(new EditArea(params...), false); };
