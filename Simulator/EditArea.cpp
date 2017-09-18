
#include "pch.h"
#include "Simulator.h"
#include "Win32/ZoomableWindow.h"
#include "Resource.h"
#include "EditStates/EditState.h"
#include "Bridge.h"
#include "Port.h"
#include "Wire.h"
#include "Win32/UtilityFunctions.h"

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

	using HTResult = RenderableObject::HTResult;

	ISimulatorApp*  const _app;
	IProjectWindow* const _pw;
	IProjectPtr     const _project;
	ISelectionPtr   const _selection;
	IDWriteTextFormatPtr _legendFont;
	DrawingObjects _drawingObjects;
	unique_ptr<EditState> _state;
	HTResult _htResult = { nullptr, 0 };

public:
	EditArea (ISimulatorApp* app,
			  IProjectWindow* pw,
			  IProject* project,
			  ISelection* selection,
			  HWND hWndParent,
			  const RECT& rect,
			  IDWriteFactory* dWriteFactory)
		: base (app->GetHInstance(), WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, rect, hWndParent, nullptr, dWriteFactory)
		, _app(app)
		, _pw(pw)
		, _project(project)
		, _selection(selection)
	{
		auto dc = base::GetRenderTarget();
		_drawingObjects._dWriteFactory = dWriteFactory;
		auto hr = dc->CreateSolidColorBrush (ColorF (ColorF::PaleGreen), &_drawingObjects._poweredFillBrush); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_drawingObjects._unpoweredBrush); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_drawingObjects._brushDiscardingPort); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gold), &_drawingObjects._brushLearningPort); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green), &_drawingObjects._brushForwarding); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_drawingObjects._brushNoForwardingWire); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Red), &_drawingObjects._brushLoop); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Blue), &_drawingObjects._brushTempWire); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &_drawingObjects._brushWindowText); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOW), &_drawingObjects._brushWindow); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_HIGHLIGHT), &_drawingObjects._brushHighlight); assert(SUCCEEDED(hr));
		hr = GetDWriteFactory()->CreateTextFormat (L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_drawingObjects._regularTextFormat); assert(SUCCEEDED(hr));
		hr = GetDWriteFactory()->CreateTextFormat (L"Tahoma", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"en-US", &_drawingObjects._smallTextFormat); assert(SUCCEEDED(hr));
		GetDWriteFactory()->CreateTextFormat (L"Tahoma", nullptr,  DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_CONDENSED, 11, L"en-US", &_legendFont); assert(SUCCEEDED(hr));

		ID2D1FactoryPtr factory;
		dc->GetFactory(&factory);

		D2D1_STROKE_STYLE_PROPERTIES ssprops = {};
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawingObjects._strokeStyleSelectionRect); assert(SUCCEEDED(hr));

		ssprops = { };
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawingObjects._strokeStyleNoForwardingWire); assert(SUCCEEDED(hr));

		ssprops = { };
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawingObjects._strokeStyleForwardingWire); assert(SUCCEEDED(hr));

		_selection->GetChangedEvent().AddHandler (&OnSelectionChanged, this);
		_project->GetBridgeRemovingEvent().AddHandler (&OnBridgeRemoving, this);
		_project->GetWireRemovingEvent().AddHandler (&OnWireRemoving, this);
		_project->GetInvalidateEvent().AddHandler (&OnProjectInvalidate, this);
		_pw->GetSelectedVlanNumerChangedEvent().AddHandler (&OnSelectedVlanChanged, this);
	}

	virtual ~EditArea()
	{
		_pw->GetSelectedVlanNumerChangedEvent().RemoveHandler (&OnSelectedVlanChanged, this);
		_project->GetInvalidateEvent().RemoveHandler(&OnProjectInvalidate, this);
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
		area->_htResult = { nullptr, 0 };
	}

	static void OnWireRemoving (void* callbackArg, IProject* project, size_t index, Wire* w)
	{
		auto area = static_cast<EditArea*>(callbackArg);
		area->_htResult = { nullptr, 0 };
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

		{ L"Designated discarding",  STP_PORT_ROLE_DESIGNATED, false, false, false },
		{ L"Designated learning",    STP_PORT_ROLE_DESIGNATED, true,  false, false },
		{ L"Designated forwarding",  STP_PORT_ROLE_DESIGNATED, true,  true,  false },
		{ L"Design. fwd. operEdge",  STP_PORT_ROLE_DESIGNATED, true,  true,  true  },

		{ L"Root/Master discarding", STP_PORT_ROLE_ROOT,       false, false, false },
		{ L"Root/Master learning",   STP_PORT_ROLE_ROOT,       true,  false, false },
		{ L"Root/Master forwarding", STP_PORT_ROLE_ROOT,       true,  true,  false },

		{ L"Alternate discarding",   STP_PORT_ROLE_ALTERNATE,  false, false, false },
		{ L"Alternate learning",     STP_PORT_ROLE_ALTERNATE,  true,  false, false },

		{ L"Backup discarding",      STP_PORT_ROLE_BACKUP,     false, false, false },
		{ L"Undefined",              STP_PORT_ROLE_UNKNOWN,    false, false, false },
	};

	void RenderLegend (ID2D1RenderTarget* dc) const
	{
		auto clientSizeDips = GetClientSizeDips();

		float maxLineWidth = 0;
		float maxLineHeight = 0;
		vector<IDWriteTextLayoutPtr> layouts;
		for (auto& info : LegendInfo)
		{
			IDWriteTextLayoutPtr tl;
			auto hr = GetDWriteFactory()->CreateTextLayout (info.text, (UINT32) wcslen(info.text), _legendFont, 1000, 1000, &tl); assert(SUCCEEDED(hr));

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

	void RenderConfigIdList (ID2D1RenderTarget* dc, const std::set<STP_MST_CONFIG_ID>& configIds) const
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

	void RenderHint (ID2D1RenderTarget* rt, float centerX, float y, const wchar_t* text, bool smallFont = false, bool alignBottom = false) const
	{
		float leftRightPadding = 3;
		float topBottomPadding = 1.5f;
		auto textFormat = smallFont ? _drawingObjects._smallTextFormat.GetInterfacePtr() : _drawingObjects._regularTextFormat.GetInterfacePtr();
		IDWriteTextLayoutPtr tl;
		auto hr = _drawingObjects._dWriteFactory->CreateTextLayout(text, (UINT32) wcslen(text), textFormat, 10000, 10000, &tl); assert(SUCCEEDED(hr));
		DWRITE_TEXT_METRICS metrics;
		hr = tl->GetMetrics(&metrics); assert(SUCCEEDED(hr));

		float pixelWidthDips = GetDipSizeFromPixelSize ({ 1, 0 }).width;
		float lineWidthDips = roundf(1.0f / pixelWidthDips) * pixelWidthDips;

		float left = centerX - metrics.width / 2 - leftRightPadding;
		float top = y - (alignBottom ? (topBottomPadding + metrics.height + topBottomPadding + lineWidthDips) : 0);
		float right = centerX + metrics.width / 2 + leftRightPadding;
		float bottom = y + 2 * topBottomPadding + metrics.height;
		left   = roundf (left   / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;
		top    = roundf (top    / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;
		right  = roundf (right  / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;
		bottom = roundf (bottom / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;

		D2D1_ROUNDED_RECT rr = { { left, top, right, bottom }, 4, 4 };
		ID2D1SolidColorBrushPtr brush;
		rt->CreateSolidColorBrush (GetD2DSystemColor(COLOR_INFOBK), &brush);
		rt->FillRoundedRectangle (&rr, brush);

		brush->SetColor (GetD2DSystemColor(COLOR_INFOTEXT));
		rt->DrawRoundedRectangle (&rr, brush, lineWidthDips);

		rt->DrawTextLayout ({ rr.rect.left + leftRightPadding, rr.rect.top + topBottomPadding }, tl, brush);
	}

	void RenderBridges (ID2D1RenderTarget* dc, const std::set<STP_MST_CONFIG_ID>& configIds) const
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

	void RenderWires (ID2D1RenderTarget* dc) const
	{
		D2D1_MATRIX_3X2_F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform(GetZoomTransform());

		for (const unique_ptr<Wire>& w : _project->GetWires())
		{
			bool hasLoop;
			bool forwarding = _project->IsWireForwarding(w.get(), _pw->GetSelectedVlanNumber(), &hasLoop);
			w->Render (dc, _drawingObjects, forwarding, hasLoop);
		}

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

	void RenderHover (ID2D1RenderTarget* dc) const
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

	virtual void Render(ID2D1RenderTarget* rt) const override final
	{
		std::set<STP_MST_CONFIG_ID> configIds;
		for (auto& bridge : _project->GetBridges())
		{
			auto stpb = bridge->GetStpBridge();
			if (STP_GetStpVersion(stpb) >= STP_VERSION_MSTP)
				configIds.insert (*STP_GetMstConfigId(stpb));
		}

		rt->Clear(GetD2DSystemColor(COLOR_WINDOW));

		RenderLegend(rt);

		RenderBridges (rt, configIds);

		RenderWires (rt);

		for (Object* o : _selection->GetObjects())
		{
			if (auto ro = dynamic_cast<RenderableObject*>(o))
				ro->RenderSelection(this, rt, _drawingObjects);
		}

		if (!configIds.empty())
			RenderConfigIdList (rt, configIds);

		if (_htResult.object != nullptr)
			RenderHover(rt);

		RenderHint (rt, GetClientWidthDips() / 2, GetClientHeightDips(), L"Rotate mouse wheel for zooming, press wheel and drag for panning.", true, true);

		if (_project->IsSimulationPaused())
			RenderHint (rt, GetClientWidthDips() / 2, 10, L"Simulation is paused. Right-click to resume.");

		if (_state != nullptr)
			_state->Render(rt);
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
			UINT modifierKeysDown = (UINT) wParam;
			auto result = ProcessMouseButtonUp (button, modifierKeysDown, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
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
				for (Object* o : _selection->GetObjects())
				{
					auto b = dynamic_cast<Bridge*>(o);
					if (b != nullptr)
					{
						if (enable && !STP_IsBridgeStarted(b->GetStpBridge()))
							STP_StartBridge(b->GetStpBridge(), GetMessageTime());
						else if (!enable && STP_IsBridgeStarted(b->GetStpBridge()))
							STP_StopBridge(b->GetStpBridge(), GetMessageTime());
					}
				}

				_project->SetChangedFlag(true);
			}
			else if (wParam == ID_PAUSE_SIMULATION)
			{
				_project->PauseSimulation();
				return 0;
			}
			else if (wParam == ID_RESUME_SIMULATION)
			{
				_project->ResumeSimulation();
				return 0;
			}

			return 0;
		}

		return base::WindowProc (hwnd, uMsg, wParam, lParam);
	}

	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const override final
	{
		auto& bridges = _project->GetBridges();
		for (auto it = bridges.rbegin(); it != bridges.rend(); it++)
		{
			auto ht = it->get()->HitTest(this, dLocation, tolerance);
			if (ht.object != nullptr)
			{
				auto port = dynamic_cast<Port*>(ht.object);
				if ((port != nullptr) && (ht.code == Port::HTCodeCP))
					return port;
				else
					return nullptr;
			}
		}

		return nullptr;
	}

	HTResult HitTestObjects (D2D1_POINT_2F dLocation, float tolerance) const
	{
		auto& wires = _project->GetWires();
		for (auto it = wires.rbegin(); it != wires.rend(); it++)
		{
			auto ht = it->get()->HitTest (this, dLocation, tolerance);
			if (ht.object != nullptr)
				return ht;
		}

		auto& bridges = _project->GetBridges();
		for (auto it = bridges.rbegin(); it != bridges.rend(); it++)
		{
			auto ht = it->get()->HitTest(this, dLocation, tolerance);
			if (ht.object != nullptr)
				return ht;
		}

		return { };
	}

	std::optional<LRESULT> ProcessKeyOrSysKeyDown (UINT virtualKey, UINT modifierKeys)
	{
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
			if (any_of(_selection->GetObjects().begin(), _selection->GetObjects().end(), [](Object* o) { return o->Is<Port>(); }))
			{
				TaskDialog (GetHWnd(), nullptr, _app->GetAppName(), nullptr, L"Ports cannot be deleted.", 0, nullptr, nullptr);
				return 0;
			}

			if (any_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [](Object* o) { return o->Is<Port>(); }))
			{
				TaskDialog (_pw->GetHWnd(), nullptr, _app->GetAppName(), L"Can't Delete Ports", L"The Simulator does not yet support deleting ports.", 0, TD_INFORMATION_ICON, nullptr);
				return 0;
			}

			while (!_selection->GetObjects().empty())
			{
				if (auto b = dynamic_cast<Bridge*>(_selection->GetObjects().front()))
				{
					auto it = find_if (_project->GetBridges().begin(), _project->GetBridges().end(), [b](const unique_ptr<Bridge>& pb) { return pb.get() == b; });
					if (it != _project->GetBridges().end())
					{
						size_t bi = it - _project->GetBridges().begin();
						_project->RemoveBridge(bi);
						_project->SetChangedFlag(true);
					}
				}
				else if (auto w = dynamic_cast<Wire*>(_selection->GetObjects().front()))
				{
					auto it = find_if (_project->GetWires().begin(), _project->GetWires().end(), [w](const unique_ptr<Wire>& pw) { return pw.get() == w; });
					if (it != _project->GetWires().end())
					{
						size_t wi = it - _project->GetWires().begin();
						_project->RemoveWire(wi);
						_project->SetChangedFlag(true);
					}
				}
			}

			return 0;
		}

		return nullopt;
	}

	std::optional<LRESULT> ProcessKeyOrSysKeyUp (UINT virtualKey, UINT modifierKeys)
	{
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

		MouseLocation mouseLocation;
		mouseLocation.pt = pt;
		mouseLocation.d = GetDipLocationFromPixelLocation(pt);
		mouseLocation.w = GetWLocationFromDLocation(mouseLocation.d);

		if (_state != nullptr)
		{
			_state->OnMouseDown (button, modifierKeysDown, mouseLocation);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return 0;
		}

		auto ht = HitTestObjects (mouseLocation.d, SnapDistance);
		if (ht.object == nullptr)
			_selection->Clear();
		else
		{
			if (modifierKeysDown & MK_CONTROL)
			{
				if (_selection->Contains(ht.object))
					_selection->Remove(ht.object);
				else if (!_selection->GetObjects().empty() && (typeid(*_selection->GetObjects()[0]) == typeid(*ht.object)))
					_selection->Add(ht.object);
				else
					_selection->Select(ht.object);
			}
			else
			{
				if (!_selection->Contains(ht.object))
					_selection->Select(ht.object);
			}
		}

		if (ht.object == nullptr)
		{
			// TODO: area selection
			//stateForMoveThreshold =
		}
		else
		{
			unique_ptr<EditState> stateMoveThreshold;
			unique_ptr<EditState> stateButtonUp;

			if (dynamic_cast<Bridge*>(ht.object) != nullptr)
			{
				if (button == MouseButton::Left)
					stateMoveThreshold = CreateStateMoveBridges (MakeEditStateDeps());
			}
			else if (dynamic_cast<Port*>(ht.object) != nullptr)
			{
				auto port = dynamic_cast<Port*>(ht.object);

				if (ht.code == Port::HTCodeInnerOuter)
				{
					if ((button == MouseButton::Left) && (_selection->GetObjects().size() == 1) && (dynamic_cast<Port*>(_selection->GetObjects()[0]) != nullptr))
						stateMoveThreshold = CreateStateMovePort (MakeEditStateDeps());
				}
				else if (ht.code == Port::HTCodeCP)
				{
					auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
					if (alreadyConnectedWire.first == nullptr)
					{
						stateMoveThreshold = CreateStateCreateWire(MakeEditStateDeps());
						stateButtonUp = CreateStateCreateWire(MakeEditStateDeps());
					}
				}
			}
			else if (dynamic_cast<Wire*>(ht.object) != nullptr)
			{
				auto wire = static_cast<Wire*>(ht.object);
				if (ht.code >= 0)
				{
					stateMoveThreshold = CreateStateMoveWirePoint(MakeEditStateDeps(), wire, ht.code);
					stateButtonUp = CreateStateMoveWirePoint (MakeEditStateDeps(), wire, ht.code);
				}
			}

			auto state = CreateStateBeginningDrag(MakeEditStateDeps(), ht.object, button, modifierKeysDown, mouseLocation, ::GetCursor(), move(stateMoveThreshold), move(stateButtonUp));
			EnterState(move(state));
		}

		return 0;
	}

	std::optional<LRESULT> ProcessMouseButtonUp (MouseButton button, UINT modifierKeysDown, POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_state != nullptr)
		{
			_state->OnMouseUp (button, modifierKeysDown, { pt, dLocation, wLocation });
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};
		}

		if (button == MouseButton::Right)
			return nullopt; // return "not handled", to cause our called to pass the message to DefWindowProc, which will generate WM_CONTEXTMENU

		return 0;
	}

	virtual void EnterState (std::unique_ptr<EditState>&& state) override final
	{
		_state = move(state);
		_htResult = { nullptr };
	}

	void ProcessWmSetCursor (POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_state != nullptr)
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

		if (_state != nullptr)
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
		{
			hMenu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_EMPTY_SPACE));
			::EnableMenuItem (hMenu, ID_PAUSE_SIMULATION, _project->IsSimulationPaused() ? MF_DISABLED : MF_ENABLED);
			::EnableMenuItem (hMenu, ID_RESUME_SIMULATION, _project->IsSimulationPaused() ? MF_ENABLED : MF_DISABLED);
		}
		else if (dynamic_cast<Bridge*>(_selection->GetObjects().front()) != nullptr)
			hMenu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_BRIDGE));
		else if (dynamic_cast<Port*>(_selection->GetObjects().front()) != nullptr)
			hMenu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_PORT));
		else
			assert(false); // not implemented

		TrackPopupMenuEx (GetSubMenu(hMenu, 0), 0, pt.x, pt.y, GetHWnd(), nullptr);
		return 0;
	}

	virtual const DrawingObjects& GetDrawingObjects() const override final { return _drawingObjects; }

	virtual D2D1::Matrix3x2F GetZoomTransform() const override final { return base::GetZoomTransform(); }

	EditStateDeps MakeEditStateDeps()
	{
		return EditStateDeps { _pw, this, _project, _selection };
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
