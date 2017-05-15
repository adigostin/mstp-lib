
#include "pch.h"
#include "Simulator.h"
#include "Win32/ZoomableWindow.h"
#include "Resource.h"
#include "EditStates/EditState.h"
#include "Bridge.h"
#include "Port.h"
#include "Wire.h"

using namespace std;
using namespace D2D1;

static const D2D1_COLOR_F RegionColors[] =
{
	ColorF(ColorF::LightBlue),
	ColorF(ColorF::Green),
	ColorF(ColorF::LightPink),
	ColorF(ColorF::Aqua),
	ColorF(ColorF::YellowGreen),
	ColorF(ColorF::DarkOrange),
	ColorF(ColorF::Yellow),
	ColorF(ColorF::DarkMagenta),
};


class EditArea : public ZoomableWindow, public IEditArea
{
	typedef ZoomableWindow base;

	ISimulatorApp*  const _app;
	IProjectWindow* const _pw;
	IProjectPtr     const _project;
	IActionListPtr  const _actionList;
	ISelectionPtr   const _selection;
	IDWriteTextFormatPtr _legendFont;
	DrawingObjects _drawingObjects;
	unique_ptr<EditState> _state;
	HTResult _htResult = { nullptr, 0 };

	struct BeginningDrag
	{
		MouseLocation location;
		MouseButton button;
		HCURSOR cursor;
		unique_ptr<EditState> stateMoveThreshold;
		unique_ptr<EditState> stateButtonUp;
	};

	optional<BeginningDrag> _beginningDrag;

public:
	EditArea (ISimulatorApp* app,
			  IProjectWindow* pw,
			  IProject* project,
			  IActionList* actionList,
			  ISelection* selection,
			  HWND hWndParent,
			  const RECT& rect,
			  ID3D11DeviceContext1* deviceContext,
			  IDWriteFactory* dWriteFactory)
		: base (app->GetHInstance(), WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, rect, hWndParent, nullptr, deviceContext, dWriteFactory)
		, _app(app)
		, _pw(pw)
		, _project(project)
		, _actionList(actionList)
		, _selection(selection)
	{
		auto dc = base::GetDeviceContext();
		_drawingObjects._dWriteFactory = dWriteFactory;
		auto hr = dc->CreateSolidColorBrush (ColorF (ColorF::PaleGreen), &_drawingObjects._poweredFillBrush); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_drawingObjects._unpoweredBrush); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_drawingObjects._brushDiscardingPort); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gold), &_drawingObjects._brushLearningPort); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green), &_drawingObjects._brushForwarding); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_drawingObjects._brushNoForwardingWire); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Red), &_drawingObjects._brushLoop); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Blue), &_drawingObjects._brushTempWire); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &_drawingObjects._brushWindowText); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOW), &_drawingObjects._brushWindow); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_HIGHLIGHT), &_drawingObjects._brushHighlight); ThrowIfFailed(hr);
		hr = GetDWriteFactory()->CreateTextFormat (L"Tahoma", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_drawingObjects._regularTextFormat); ThrowIfFailed(hr);

		GetDWriteFactory()->CreateTextFormat (L"Tahoma", nullptr,  DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
										  DWRITE_FONT_STRETCH_CONDENSED, 11, L"en-US", &_legendFont); ThrowIfFailed(hr);

		ID2D1FactoryPtr factory;
		dc->GetFactory(&factory);

		D2D1_STROKE_STYLE_PROPERTIES ssprops = {};
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawingObjects._strokeStyleSelectionRect); ThrowIfFailed(hr);

		ssprops = { };
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawingObjects._strokeStyleNoForwardingWire); ThrowIfFailed(hr);

		ssprops = { };
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawingObjects._strokeStyleForwardingWire); ThrowIfFailed(hr);

		_selection->GetChangedEvent().AddHandler (&OnSelectionChanged, this);
		_project->GetBridgeRemovingEvent().AddHandler (&OnBridgeRemoving, this);
		_project->GetWireRemovingEvent().AddHandler (&OnWireRemoving, this);
		_project->GetProjectInvalidateEvent().AddHandler (&OnProjectInvalidate, this);
		_pw->GetSelectedVlanNumerChangedEvent().AddHandler (&OnSelectedVlanChanged, this);
	}

	virtual ~EditArea()
	{
		_pw->GetSelectedVlanNumerChangedEvent().RemoveHandler (&OnSelectedVlanChanged, this);
		_project->GetProjectInvalidateEvent().RemoveHandler(&OnProjectInvalidate, this);
		_project->GetWireRemovingEvent().RemoveHandler (&OnWireRemoving, this);
		_project->GetBridgeRemovingEvent().RemoveHandler (&OnBridgeRemoving, this);
		_selection->GetChangedEvent().RemoveHandler (&OnSelectionChanged, this);
	}

	static void OnSelectedVlanChanged (void* callbackArg, IProjectWindow* pw, unsigned int vlanNumber)
	{
		auto area = static_cast<EditArea*>(callbackArg);
		::InvalidateRect (area->GetHWnd(), nullptr, FALSE);
	}

	static void OnBridgeRemoving (void* callbackArg, IProject* project, size_t index, Bridge* b)
	{
		auto area = static_cast<EditArea*>(callbackArg);
		if (dynamic_cast<Bridge*>(area->_htResult.object) != nullptr)
		{
			auto htBridge = static_cast<Bridge*>(area->_htResult.object);
			if (htBridge == b)
			{
				area->_htResult = { nullptr, 0 };
				InvalidateRect (area->GetHWnd(), nullptr, FALSE);
			}
		}
		else if (dynamic_cast<Port*>(area->_htResult.object) != nullptr)
		{
			auto htPort = static_cast<Port*>(area->_htResult.object);
			if (htPort->GetBridge() == b)
			{
				area->_htResult = { nullptr, 0 };
				InvalidateRect (area->GetHWnd(), nullptr, FALSE);
			}
		}
	}

	static void OnWireRemoving (void* callbackArg, IProject* project, size_t index, Wire* w)
	{
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
		vector<IDWriteTextLayoutPtr> layouts;
		for (auto& info : LegendInfo)
		{
			IDWriteTextLayoutPtr tl;
			auto hr = GetDWriteFactory()->CreateTextLayout (info.text, (UINT32) wcslen(info.text), _legendFont, 1000, 1000, &tl); ThrowIfFailed(hr);

			DWRITE_TEXT_METRICS metrics;
			tl->GetMetrics (&metrics);

			if (metrics.width > maxLineWidth)
				maxLineWidth = metrics.width;

			if (metrics.height > maxLineHeight)
				maxLineHeight = metrics.height;

			layouts.push_back(move(tl));
		}

		float textX = clientSizeDips.width - (5 + maxLineWidth + 5 + Port::ExteriorHeight + 5);
		float lineX = textX - 3;
		float bitmapX = clientSizeDips.width - (5 + Port::ExteriorHeight + 5);
		float rowHeight = 2 + max (maxLineHeight, Port::ExteriorWidth);
		float y = clientSizeDips.height - _countof(LegendInfo) * rowHeight;

		auto lineWidth = GetDipSizeFromPixelSize({ 0, 1 }).height;

		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
		ID2D1SolidColorBrushPtr brush;
		dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_INFOBK), &brush);
		brush->SetOpacity (0.8f);
		dc->FillRectangle (D2D1_RECT_F { lineX, y, clientSizeDips.width, clientSizeDips.height }, brush);
		dc->DrawLine ({ lineX, y }, { lineX, clientSizeDips.height }, _drawingObjects._brushWindowText, lineWidth);
		dc->SetAntialiasMode (oldaa);

		Matrix3x2F oldTransform;
		dc->GetTransform (&oldTransform);

		for (size_t i = 0; i < _countof(LegendInfo); i++)
		{
			auto& info = LegendInfo[i];

			auto oldaa = dc->GetAntialiasMode();
			dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
			dc->DrawLine (Point2F (lineX, y), Point2F (clientSizeDips.width, y), _drawingObjects._brushWindowText, lineWidth);
			dc->SetAntialiasMode(oldaa);

			dc->DrawTextLayout (Point2F (textX, y + 1), layouts[i], _drawingObjects._brushWindowText);

			// Rotate 270 degrees and then translate.
			Matrix3x2F trans (0.0f, -1.0f, 1.0f, 0.0f, bitmapX, y + rowHeight / 2);
			trans.SetProduct (oldTransform, trans);
			dc->SetTransform (&trans);

			Port::RenderExteriorStpPort (dc, _drawingObjects, info.role, info.learning, info.forwarding, info.operEdge);

			dc->SetTransform (&oldTransform);

			y += rowHeight;
		}
	}

	void RenderConfigIdList (ID2D1DeviceContext* dc, const std::set<STP_MST_CONFIG_ID>& configIds) const
	{
		size_t colorIndex = 0;

		auto clientSizeDips = GetClientSizeDips();

		float maxLineWidth = 0;
		float lineHeight = 0;
		vector<pair<TextLayout, D2D1_COLOR_F>> lines;
		for (const STP_MST_CONFIG_ID& configId : configIds)
		{
			wstringstream ss;
			ss << configId.ConfigurationName << " -- " << (configId.RevisionLevelLow | (configId.RevisionLevelHigh << 8)) << " -- "
				<< uppercase << setfill(L'0') << hex
				<< setw(2) << configId.ConfigurationDigest[0] << setw(2) << configId.ConfigurationDigest[1] << ".."
				<< setw(2) << configId.ConfigurationDigest[14] << setw(2) << configId.ConfigurationDigest[15];
			wstring line = ss.str();
			auto tl = TextLayout::Create (GetDWriteFactory(), _legendFont, line.c_str());

			if (tl.metrics.width > maxLineWidth)
				maxLineWidth = tl.metrics.width;

			if (tl.metrics.height > lineHeight)
				lineHeight = tl.metrics.height;

			lines.push_back ({ move(tl), RegionColors[colorIndex] });
			colorIndex = (colorIndex + 1) % _countof(RegionColors);
		}

		float LeftRightPadding = 3;
		float UpDownPadding = 2;
		float coloredRectWidth = lineHeight * 2;

		auto title = TextLayout::Create (GetDWriteFactory(), _legendFont, L"MST Config IDs:");

		float y = clientSizeDips.height - lines.size() * (lineHeight + 2 * UpDownPadding) - title.metrics.height - 2 * UpDownPadding;

		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		float lineWidth = GetDipSizeFromPixelSize({ 0, 1 }).height;
		float lineX = LeftRightPadding + coloredRectWidth + LeftRightPadding + maxLineWidth + LeftRightPadding;
		ID2D1SolidColorBrushPtr brush;
		dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_INFOBK), &brush);
		brush->SetOpacity (0.8f);
		dc->FillRectangle ({ 0, y, lineX, clientSizeDips.height }, brush);
		dc->DrawLine ({ 0, y }, { lineX, y }, _drawingObjects._brushWindowText, lineWidth);
		dc->DrawLine ({ lineX, y }, { lineX, clientSizeDips.height }, _drawingObjects._brushWindowText, lineWidth);
		dc->SetAntialiasMode(oldaa);

		dc->DrawTextLayout ({ LeftRightPadding, y + UpDownPadding }, title.layout, _drawingObjects._brushWindowText);
		y += (title.metrics.height + 2 * UpDownPadding);

		for (auto& p : lines)
		{
			ID2D1SolidColorBrushPtr brush;
			dc->CreateSolidColorBrush (p.second, &brush);
			D2D1_RECT_F rect = { LeftRightPadding, y + UpDownPadding, LeftRightPadding + coloredRectWidth, y + UpDownPadding + lineHeight };
			dc->FillRectangle (&rect, brush);
			D2D1_POINT_2F pt = { LeftRightPadding + coloredRectWidth + LeftRightPadding, y + UpDownPadding };
			dc->DrawTextLayout (pt, p.first.layout, _drawingObjects._brushWindowText);
			y += (lineHeight + 2 * UpDownPadding);
		}
	}

	virtual void RenderSnapRect (ID2D1RenderTarget* rt, D2D1_POINT_2F wLocation) const override final
	{
		auto oldaa = rt->GetAntialiasMode();
		rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		auto cpd = GetDLocationFromWLocation(wLocation);
		auto rect = RectF (cpd.x - SnapDistance, cpd.y - SnapDistance, cpd.x + SnapDistance, cpd.y + SnapDistance);
		rt->DrawRectangle (rect, _drawingObjects._brushHighlight, 2);

		rt->SetAntialiasMode(oldaa);
	}

	void RenderHint (ID2D1DeviceContext* dc, float centerX, float topY, const wchar_t* text) const
	{
		float padding = 3.0f;
		auto tl = TextLayout::Create (GetDWriteFactory(), _drawingObjects._regularTextFormat, text);

		D2D1_ROUNDED_RECT rr;
		rr.rect.left = centerX - tl.metrics.width / 2 - padding;
		rr.rect.top = topY;
		rr.rect.right = centerX + tl.metrics.width / 2 + padding;
		rr.rect.bottom = topY + padding + tl.metrics.height + padding;
		rr.radiusX = rr.radiusY = 4;
		ID2D1SolidColorBrushPtr brush;
		dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_INFOBK), &brush);
		dc->FillRoundedRectangle (&rr, brush);
		brush->SetColor (GetD2DSystemColor(COLOR_INFOTEXT));
		dc->DrawRoundedRectangle (&rr, brush);
		dc->DrawTextLayout ({ rr.rect.left + padding, rr.rect.top + padding }, tl.layout, brush);
	}

	void RenderBridges (ID2D1DeviceContext* dc, const std::set<STP_MST_CONFIG_ID>& configIds) const
	{
		D2D1_MATRIX_3X2_F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform(GetZoomTransform());

		for (const unique_ptr<Bridge>& bridge : _project->GetBridges())
		{
			D2D1_COLOR_F color = ColorF(ColorF::LightGreen);
			if (STP_GetStpVersion(bridge->GetStpBridge()) >= STP_VERSION_MSTP)
			{
				auto it = find (configIds.begin(), configIds.end(), *STP_GetMstConfigId(bridge->GetStpBridge()));
				if (it != configIds.end())
				{
					size_t colorIndex = (std::distance (configIds.begin(), it)) % _countof(RegionColors);
					color = RegionColors[colorIndex];
				}
			}

			bridge->Render (dc, _drawingObjects, _pw->GetSelectedVlanNumber(), color);
		}

		dc->SetTransform(oldtr);
	}

	void RenderWires (ID2D1DeviceContext* dc) const
	{
		D2D1_MATRIX_3X2_F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform(GetZoomTransform());

		for (const unique_ptr<Wire>& w : _project->GetWires())
			w->Render (dc, _drawingObjects, _pw->GetSelectedVlanNumber());

		dc->SetTransform(oldtr);

		if (_project->GetBridges().empty())
		{
			RenderHint (dc, GetClientWidthDips() / 2, GetClientHeightDips() / 2, L"No bridges created. Right-click to create some.");
		}
		else if (_project->GetBridges().size() == 1)
		{
			RenderHint (dc, GetClientWidthDips() / 2, GetClientHeightDips() / 2, L"Right-click to add more bridges.");
		}
		else
		{
			bool anyPortConnected = false;
			for (auto& b : _project->GetBridges())
				anyPortConnected |= any_of (b->GetPorts().begin(), b->GetPorts().end(),
											[this](const unique_ptr<Port>& p) { return _project->GetWireConnectedToPort(p.get()).first != nullptr; });

			if (!anyPortConnected)
			{
				Bridge* b = _project->GetBridges().front().get();
				auto text = L"No port connected. You can connect\r\nports by drawing wires with the mouse.";
				auto wl = D2D1_POINT_2F { b->GetLeft() + b->GetWidth() / 2, b->GetBottom() + Port::ExteriorHeight * 1.5f };
				auto dl = GetDLocationFromWLocation(wl);
				RenderHint (dc, dl.x, dl.y, text);
			}
		}
	}

	void RenderHover (ID2D1DeviceContext* dc) const
	{
		if (dynamic_cast<Port*>(_htResult.object) != nullptr)
		{
			if (_htResult.code == Port::HTCodeCP)
				RenderSnapRect (dc, static_cast<Port*>(_htResult.object)->GetCPLocation());
		}
		else if (dynamic_cast<Wire*>(_htResult.object) != nullptr)
		{
			if (_htResult.code >= 0)
				RenderSnapRect (dc, static_cast<Wire*>(_htResult.object)->GetPointCoords(_htResult.code));
		}
	}

	virtual void Render(ID2D1DeviceContext* dc) const override final
	{
		std::set<STP_MST_CONFIG_ID> configIds;
		for (auto& bridge : _project->GetBridges())
		{
			auto stpb = bridge->GetStpBridge();
			if (STP_GetStpVersion(stpb) >= STP_VERSION_MSTP)
				configIds.insert (*STP_GetMstConfigId(stpb));
		}

		dc->Clear(GetD2DSystemColor(COLOR_WINDOW));

		RenderBridges (dc, configIds);

		RenderWires (dc);

		for (auto& o : _selection->GetObjects())
			o->RenderSelection(this, dc, _drawingObjects);

		RenderLegend(dc);

		if (!configIds.empty())
			RenderConfigIdList (dc, configIds);

		if (_htResult.object != nullptr)
			RenderHover(dc);

		if (_state != nullptr)
			_state->Render(dc);
	}

	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }

	static UINT GetModifierKeys()
	{
		UINT modifierKeys = 0;

		if (GetKeyState (VK_SHIFT) < 0)
			modifierKeys |= MK_SHIFT;

		if (GetKeyState (VK_CONTROL) < 0)
			modifierKeys |= MK_CONTROL;

		if (GetKeyState (VK_MENU) < 0)
			modifierKeys |= MK_ALT;

		return modifierKeys;
	}

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		if ((uMsg == WM_LBUTTONDOWN) || (uMsg == WM_RBUTTONDOWN))
		{
			auto button = (uMsg == WM_LBUTTONDOWN) ? MouseButton::Left : MouseButton::Right;
			UINT modifierKeysDown = (UINT) wParam;
			auto result = ProcessMouseButtonDown (button, modifierKeysDown, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
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
		else if ((uMsg == WM_KEYDOWN) || (uMsg == WM_SYSKEYDOWN))
		{
			return ProcessKeyOrSysKeyDown ((UINT) wParam, GetModifierKeys());
		}
		else if ((uMsg == WM_KEYUP) || (uMsg == WM_SYSKEYUP))
		{
			return ProcessKeyOrSysKeyUp ((UINT) wParam, GetModifierKeys());
		}
		else if (uMsg == WM_COMMAND)
		{
			if (wParam == ID_NEW_BRIDGE)
			{
				EnterState (CreateStateCreateBridge(MakeEditStateDeps()));
			}
			else if ((wParam == ID_BRIDGE_ENABLE_STP) || (wParam == ID_BRIDGE_DISABLE_STP))
			{
				bool enable = (wParam == ID_BRIDGE_ENABLE_STP);
				auto timestamp = GetTimestampMilliseconds();
				for (Object* o : _selection->GetObjects())
				{
					auto b = dynamic_cast<Bridge*>(o);
					if (b != nullptr)
					{
						if (enable)
						{
							if (!STP_IsBridgeStarted(b->GetStpBridge()))
								STP_StartBridge (b->GetStpBridge(), timestamp);
						}
						else
						{
							if (STP_IsBridgeStarted(b->GetStpBridge()))
								STP_StopBridge (b->GetStpBridge(), timestamp);
						}
					}
				}
			}

			return 0;
		}

		return base::WindowProc (hwnd, uMsg, wParam, lParam);
	}

	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const override final
	{
		for (auto& b : _project->GetBridges())
		{
			for (auto& port : b->GetPorts())
			{
				if (port->HitTestCP (this, dLocation, tolerance))
					return port.get();
			}
		}

		return nullptr;
	}

	HTResult HitTestObjects (D2D1_POINT_2F dLocation, float tolerance) const
	{
		for (auto& w : _project->GetWires())
		{
			auto ht = w->HitTest (this, dLocation, tolerance);
			if (ht.object != nullptr)
				return ht;
		}

		for (auto& b : _project->GetBridges())
		{
			auto ht = b->HitTest(this, dLocation, tolerance);
			if (ht.object != nullptr)
				return ht;
		}

		return { };
	}

	std::optional<LRESULT> ProcessKeyOrSysKeyDown (UINT virtualKey, UINT modifierKeys)
	{
		if (_beginningDrag)
		{
			if (virtualKey == VK_ESCAPE)
			{
				_beginningDrag = nullopt;
				return 0;
			}

			return nullopt;
		}

		if (_state != nullptr)
		{
			auto res = _state->OnKeyDown (virtualKey, modifierKeys);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return res;
		}

		if (virtualKey == VK_DELETE)
		{
			while (!_selection->GetObjects().empty())
				_project->Remove(_selection->GetObjects().back());
			return 0;
		}

		return nullopt;
	}

	std::optional<LRESULT> ProcessKeyOrSysKeyUp (UINT virtualKey, UINT modifierKeys)
	{
		if (_beginningDrag)
			return nullopt;

		if (_state != nullptr)
			return _state->OnKeyUp (virtualKey, modifierKeys);

		return nullopt;
	}

	std::optional<LRESULT> ProcessMouseButtonDown (MouseButton button, UINT modifierKeysDown, POINT pt)
	{
		::SetFocus(GetHWnd());
		if (::GetFocus() != GetHWnd())
			// Some validation code (maybe in the Properties Window) must have failed and taken focus back.
			return nullopt;

		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_beginningDrag)
		{
			// user began dragging with a mouse button, now he pressed a second button.
			return nullopt;
		}

		if (_state != nullptr)
		{
			_state->OnMouseDown ({ pt, dLocation, wLocation }, button);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return 0;
		}

		auto ht = HitTestObjects (dLocation, SnapDistance);
		if (ht.object == nullptr)
			_selection->Clear();
		else
		{
			if (modifierKeysDown & MK_CONTROL)
				_selection->Add(ht.object);
			else
				_selection->Select(ht.object);
		}

		_beginningDrag = BeginningDrag();
		_beginningDrag->location.pt = pt;
		_beginningDrag->location.d = dLocation;
		_beginningDrag->location.w = wLocation;
		_beginningDrag->button = button;
		_beginningDrag->cursor = ::GetCursor();

		if (ht.object == nullptr)
		{
			// TODO: area selection
			//stateForMoveThreshold =
		}
		else if (dynamic_cast<Bridge*>(ht.object) != nullptr)
		{
			if (button == MouseButton::Left)
				_beginningDrag->stateMoveThreshold = CreateStateMoveBridges (MakeEditStateDeps());
		}
		else if (dynamic_cast<Port*>(ht.object) != nullptr)
		{
			auto port = dynamic_cast<Port*>(ht.object);

			if (ht.code == Port::HTCodeInnerOuter)
			{
				if (button == MouseButton::Left)
					_beginningDrag->stateMoveThreshold = CreateStateMovePort (MakeEditStateDeps());
			}
			else if (ht.code == Port::HTCodeCP)
			{
				auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
				if (alreadyConnectedWire.first == nullptr)
				{
					_beginningDrag->stateMoveThreshold = CreateStateCreateWire(MakeEditStateDeps());
					_beginningDrag->stateButtonUp = CreateStateCreateWire(MakeEditStateDeps());
				}
			}
		}
		else if (dynamic_cast<Wire*>(ht.object) != nullptr)
		{
			auto wire = static_cast<Wire*>(ht.object);
			if (ht.code >= 0)
			{
				_beginningDrag->stateMoveThreshold = CreateStateMoveWirePoint(MakeEditStateDeps(), wire, ht.code);
				_beginningDrag->stateButtonUp = CreateStateMoveWirePoint (MakeEditStateDeps(), wire, ht.code);
			}
		}

		return 0;
	}

	std::optional<LRESULT> ProcessMouseButtonUp (MouseButton button, POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_beginningDrag)
		{
			if (button != _beginningDrag->button)
				return nullopt; // return "not handled"

			if (_beginningDrag->stateButtonUp != nullptr)
			{
				_htResult = { nullptr };
				_state = move(_beginningDrag->stateButtonUp);
				_state->OnMouseDown (_beginningDrag->location, _beginningDrag->button);
				assert (!_state->Completed());
				if ((pt.x != _beginningDrag->location.pt.x) || (pt.y != _beginningDrag->location.pt.y))
					_state->OnMouseMove (_beginningDrag->location);
			}

			_beginningDrag = nullopt;
			// fall-through to the code that handles the state
		}

		if (_state != nullptr)
		{
			_state->OnMouseUp ({ pt, dLocation, wLocation }, button);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return 0;
		}

		if (button == MouseButton::Right)
			return nullopt; // return "not handled", to cause our called to pass the message to DefWindowProc, which will generate WM_CONTEXTMENU

		return 0;
	}

	virtual void EnterState (std::unique_ptr<EditState>&& state) override final
	{
		_beginningDrag = nullopt;
		_state = move(state);
		_htResult = { nullptr };
	}

	void ProcessWmSetCursor (POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_beginningDrag)
		{
			::SetCursor (_beginningDrag->cursor);
		}
		else if (_state != nullptr)
		{
			::SetCursor (_state->GetCursor());
		}
		else
		{
			auto ht = HitTestObjects (dLocation, SnapDistance);

			LPCWSTR idc = IDC_ARROW;
			if (dynamic_cast<Port*>(ht.object) != nullptr)
			{
				if (ht.code == Port::HTCodeCP)
					idc = IDC_CROSS;
			}
			else if (dynamic_cast<Wire*>(ht.object) != nullptr)
			{
				if (ht.code >= 0)
					// wire point
					idc = IDC_CROSS;
				else
					// wire line
					idc = IDC_ARROW;
			}

			::SetCursor (LoadCursor(nullptr, idc));

			if (_htResult != ht)
			{
				_htResult = ht;
				::InvalidateRect(GetHWnd(), nullptr, 0);
			}
		}
	}

	void ProcessWmMouseMove (POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_beginningDrag)
		{
			RECT rc = { _beginningDrag->location.pt.x, _beginningDrag->location.pt.y, _beginningDrag->location.pt.x, _beginningDrag->location.pt.y };
			InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
			if (!PtInRect (&rc, pt))
			{
				if (_beginningDrag->stateMoveThreshold != nullptr)
				{
					_state = move(_beginningDrag->stateMoveThreshold);
					_state->OnMouseDown (_beginningDrag->location, _beginningDrag->button);
					assert (!_state->Completed());
					_state->OnMouseMove ({ pt, dLocation, wLocation });
				}

				_beginningDrag = nullopt;
				_htResult = { nullptr };
			}
		}
		else if (_state != nullptr)
		{
			_state->OnMouseMove ({ pt, dLocation, wLocation });
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			}
		}
	}

	std::optional<LRESULT> ProcessWmContextMenu (HWND hwnd, POINT pt)
	{
		//D2D1_POINT_2F dipLocation = GetDipLocationFromPixelLocation(pt);
		//_elementsAtContextMenuLocation.clear();
		//GetElementsAt(_project->GetInnerRootElement(), { dipLocation.x, dipLocation.y }, _elementsAtContextMenuLocation);

		HMENU hMenu;
		if (_selection->GetObjects().empty())
			hMenu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_EMPTY_SPACE));
		else if (dynamic_cast<Bridge*>(_selection->GetObjects().front()) != nullptr)
			hMenu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_BRIDGE));
		else if (dynamic_cast<Port*>(_selection->GetObjects().front()) != nullptr)
			hMenu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_PORT));
		else
			throw not_implemented_exception();

		TrackPopupMenuEx (GetSubMenu(hMenu, 0), 0, pt.x, pt.y, GetHWnd(), nullptr);
		return 0;
	}

	virtual const DrawingObjects& GetDrawingObjects() const override final { return _drawingObjects; }

	virtual D2D1::Matrix3x2F GetZoomTransform() const override final { return base::GetZoomTransform(); }

	EditStateDeps MakeEditStateDeps() const
	{
		return EditStateDeps { _pw, _project, _actionList, _selection };
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface (REFIID riid, void** ppvObject) override { return base::QueryInterface(riid, ppvObject); }
	virtual ULONG STDMETHODCALLTYPE AddRef() override { return base::AddRef(); }
	virtual ULONG STDMETHODCALLTYPE Release() override { return base::Release(); }
};

template<typename... Args>
static IEditAreaPtr Create (Args... args)
{
	return IEditAreaPtr(new EditArea (std::forward<Args>(args)...), false);
}

extern const EditAreaFactory editAreaFactory = &Create;
