
#include "pch.h"
#include "Simulator.h"
#include "Resource.h"
#include "edit_states/edit_state.h"
#include "Bridge.h"
#include "Port.h"
#include "Wire.h"
#include "win32/zoomable_window.h"
#include "win32/utility_functions.h"

using namespace std;
using namespace D2D1;
using namespace edge;

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

#pragma warning (disable: 4250)

class edit_area : public zoomable_window, public edit_area_i
{
	typedef zoomable_window base;

	using HTResult = renderable_object::HTResult;

	ISimulatorApp*  const _app;
	IProjectWindow* const _pw;
	IProject*       const _project;
	ISelection*     const _selection;
	com_ptr<IDWriteTextFormat> _legendFont;
	struct drawing_resources _drawing_resources;
	unique_ptr<edit_state> _state;
	HTResult _htResult = { nullptr, 0 };

public:
	edit_area (ISimulatorApp* app,
			  IProjectWindow* pw,
			  IProject* project,
			  ISelection* selection,
			  HWND hWndParent,
			  const RECT& rect,
			  ID3D11DeviceContext1* d3d_dc,
			  IDWriteFactory* dWriteFactory)
		: base(app->GetHInstance(), WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, rect, hWndParent, 0, d3d_dc, dWriteFactory)
		, _app(app)
		, _pw(pw)
		, _project(project)
		, _selection(selection)
	{
		HRESULT hr;

		auto dc = base::d2d_dc();
		_drawing_resources._dWriteFactory = dWriteFactory;
		hr = dwrite_factory()->CreateTextFormat (L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_drawing_resources._regularTextFormat); assert(SUCCEEDED(hr));
		hr = dwrite_factory()->CreateTextFormat (L"Tahoma", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"en-US", &_drawing_resources._smallTextFormat); assert(SUCCEEDED(hr));
		dwrite_factory()->CreateTextFormat (L"Tahoma", nullptr,  DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_CONDENSED, 11, L"en-US", &_legendFont); assert(SUCCEEDED(hr));

		com_ptr<ID2D1Factory> factory;
		dc->GetFactory(&factory);

		D2D1_STROKE_STYLE_PROPERTIES ssprops = {};
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleSelectionRect); assert(SUCCEEDED(hr));

		ssprops = { };
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleNoForwardingWire); assert(SUCCEEDED(hr));

		ssprops = { };
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleForwardingWire); assert(SUCCEEDED(hr));

		_selection->GetChangedEvent().add_handler (&OnSelectionChanged, this);
		_project->GetBridgeRemovingEvent().add_handler (&OnBridgeRemoving, this);
		_project->GetWireRemovingEvent().add_handler (&OnWireRemoving, this);
		_project->GetInvalidateEvent().add_handler (&OnProjectInvalidate, this);
		_pw->GetSelectedVlanNumerChangedEvent().add_handler (&OnSelectedVlanChanged, this);
	}

	virtual ~edit_area()
	{
		_pw->GetSelectedVlanNumerChangedEvent().remove_handler (&OnSelectedVlanChanged, this);
		_project->GetInvalidateEvent().remove_handler (&OnProjectInvalidate, this);
		_project->GetWireRemovingEvent().remove_handler (&OnWireRemoving, this);
		_project->GetBridgeRemovingEvent().remove_handler (&OnBridgeRemoving, this);
		_selection->GetChangedEvent().remove_handler (&OnSelectionChanged, this);
	}

	static void OnSelectedVlanChanged (void* callbackArg, IProjectWindow* pw, unsigned int vlanNumber)
	{
		auto area = static_cast<edit_area*>(callbackArg);
		::InvalidateRect (area->hwnd(), nullptr, FALSE);
	}

	static void OnBridgeRemoving (void* callbackArg, IProject* project, size_t index, Bridge* b)
	{
		auto area = static_cast<edit_area*>(callbackArg);
		area->_htResult = { nullptr, 0 };
	}

	static void OnWireRemoving (void* callbackArg, IProject* project, size_t index, Wire* w)
	{
		auto area = static_cast<edit_area*>(callbackArg);
		area->_htResult = { nullptr, 0 };
	}

	static void OnProjectInvalidate (void* callbackArg, IProject*)
	{
		auto area = static_cast<edit_area*>(callbackArg);
		::InvalidateRect (area->hwnd(), nullptr, FALSE);
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto ea = static_cast<edit_area*>(callbackArg);
		::InvalidateRect (ea->hwnd(), nullptr, FALSE);
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
		float maxLineWidth = 0;
		float maxLineHeight = 0;
		vector<com_ptr<IDWriteTextLayout>> layouts;
		for (auto& info : LegendInfo)
		{
			com_ptr<IDWriteTextLayout> tl;
			auto hr = dwrite_factory()->CreateTextLayout (info.text, (UINT32) wcslen(info.text), _legendFont, 1000, 1000, &tl); assert(SUCCEEDED(hr));

			DWRITE_TEXT_METRICS metrics;
			tl->GetMetrics (&metrics);

			if (metrics.width > maxLineWidth)
				maxLineWidth = metrics.width;

			if (metrics.height > maxLineHeight)
				maxLineHeight = metrics.height;

			layouts.push_back(move(tl));
		}

		float textX = client_width() - (5 + maxLineWidth + 5 + Port::ExteriorHeight + 5);
		float lineX = textX - 3;
		float bitmapX = client_width() - (5 + Port::ExteriorHeight + 5);
		float rowHeight = 2 + max (maxLineHeight, Port::ExteriorWidth);
		float y = client_height() - _countof(LegendInfo) * rowHeight;

		auto lineWidth = GetDipSizeFromPixelSize({ 0, 1 }).height;

		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
		com_ptr<ID2D1SolidColorBrush> brush;
		dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_INFOBK), &brush);
		brush->SetOpacity (0.8f);
		dc->FillRectangle (D2D1_RECT_F { lineX, y, client_width(), client_height() }, brush);
		dc->DrawLine ({ lineX, y }, { lineX, client_height() }, _drawing_resources._brushWindowText, lineWidth);
		dc->SetAntialiasMode (oldaa);

		Matrix3x2F oldTransform;
		dc->GetTransform (&oldTransform);

		for (size_t i = 0; i < _countof(LegendInfo); i++)
		{
			auto& info = LegendInfo[i];

			auto oldaa = dc->GetAntialiasMode();
			dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
			dc->DrawLine (Point2F (lineX, y), Point2F (client_width(), y), _drawing_resources._brushWindowText, lineWidth);
			dc->SetAntialiasMode(oldaa);

			dc->DrawTextLayout (Point2F (textX, y + 1), layouts[i], _drawing_resources._brushWindowText);

			// Rotate 270 degrees and then translate.
			Matrix3x2F trans (0.0f, -1.0f, 1.0f, 0.0f, bitmapX, y + rowHeight / 2);
			trans.SetProduct (oldTransform, trans);
			dc->SetTransform (&trans);

			Port::RenderExteriorStpPort (dc, _drawing_resources, info.role, info.learning, info.forwarding, info.operEdge);

			dc->SetTransform (&oldTransform);

			y += rowHeight;
		}
	}

	void RenderConfigIdList (ID2D1RenderTarget* dc, const std::set<STP_MST_CONFIG_ID>& configIds) const
	{
		size_t colorIndex = 0;

		float maxLineWidth = 0;
		float lineHeight = 0;
		vector<pair<text_layout, D2D1_COLOR_F>> lines;
		for (const STP_MST_CONFIG_ID& configId : configIds)
		{
			stringstream ss;
			ss << configId.ConfigurationName << " -- " << (configId.RevisionLevelLow | (configId.RevisionLevelHigh << 8)) << " -- "
				<< uppercase << setfill('0') << hex
				<< setw(2) << configId.ConfigurationDigest[0] << setw(2) << configId.ConfigurationDigest[1] << ".."
				<< setw(2) << configId.ConfigurationDigest[14] << setw(2) << configId.ConfigurationDigest[15];
			string line = ss.str();
			auto tl = text_layout::create (dwrite_factory(), _legendFont, line.c_str());

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

		auto title = text_layout::create (dwrite_factory(), _legendFont, "MST Config IDs:");

		float y = client_height() - lines.size() * (lineHeight + 2 * UpDownPadding) - title.metrics.height - 2 * UpDownPadding;

		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		float lineWidth = GetDipSizeFromPixelSize({ 0, 1 }).height;
		float lineX = LeftRightPadding + coloredRectWidth + LeftRightPadding + maxLineWidth + LeftRightPadding;
		com_ptr<ID2D1SolidColorBrush> brush;
		dc->CreateSolidColorBrush (GetD2DSystemColor(COLOR_INFOBK), &brush);
		brush->SetOpacity (0.8f);
		dc->FillRectangle ({ 0, y, lineX, client_height() }, brush);
		dc->DrawLine ({ 0, y }, { lineX, y }, _drawing_resources._brushWindowText, lineWidth);
		dc->DrawLine ({ lineX, y }, { lineX, client_height() }, _drawing_resources._brushWindowText, lineWidth);
		dc->SetAntialiasMode(oldaa);

		dc->DrawTextLayout ({ LeftRightPadding, y + UpDownPadding }, title.layout, _drawing_resources._brushWindowText);
		y += (title.metrics.height + 2 * UpDownPadding);

		for (auto& p : lines)
		{
			com_ptr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (p.second, &brush);
			D2D1_RECT_F rect = { LeftRightPadding, y + UpDownPadding, LeftRightPadding + coloredRectWidth, y + UpDownPadding + lineHeight };
			dc->FillRectangle (&rect, brush);
			D2D1_POINT_2F pt = { LeftRightPadding + coloredRectWidth + LeftRightPadding, y + UpDownPadding };
			dc->DrawTextLayout (pt, p.first.layout, _drawing_resources._brushWindowText);
			y += (lineHeight + 2 * UpDownPadding);
		}
	}

	virtual void RenderSnapRect (ID2D1RenderTarget* rt, D2D1_POINT_2F wLocation) const override final
	{
		auto oldaa = rt->GetAntialiasMode();
		rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		auto cpd = pointw_to_pointd(wLocation);
		auto rect = RectF (cpd.x - SnapDistance, cpd.y - SnapDistance, cpd.x + SnapDistance, cpd.y + SnapDistance);
		rt->DrawRectangle (rect, _drawing_resources._brushHighlight, 2);

		rt->SetAntialiasMode(oldaa);
	}

	virtual void RenderHint (ID2D1RenderTarget* rt,
							 D2D1_POINT_2F dLocation,
							 const wchar_t* text,
							 DWRITE_TEXT_ALIGNMENT ha,
							 DWRITE_PARAGRAPH_ALIGNMENT va,
							 bool smallFont) const override final
	{
		float leftRightPadding = 3;
		float topBottomPadding = 1.5f;
		auto textFormat = smallFont ? _drawing_resources._smallTextFormat.get() : _drawing_resources._regularTextFormat.get();
		com_ptr<IDWriteTextLayout> tl;
		auto hr = _drawing_resources._dWriteFactory->CreateTextLayout(text, (UINT32) wcslen(text), textFormat, 10000, 10000, &tl); assert(SUCCEEDED(hr));
		DWRITE_TEXT_METRICS metrics;
		hr = tl->GetMetrics(&metrics); assert(SUCCEEDED(hr));

		float pixelWidthDips = GetDipSizeFromPixelSize ({ 1, 0 }).width;
		float lineWidthDips = roundf(1.0f / pixelWidthDips) * pixelWidthDips;

		float left = dLocation.x - leftRightPadding;
		if (ha == DWRITE_TEXT_ALIGNMENT_CENTER)
			left -= metrics.width / 2;
		else if (ha == DWRITE_TEXT_ALIGNMENT_TRAILING)
			left -= metrics.width;

		float top = dLocation.y;
		if (va == DWRITE_PARAGRAPH_ALIGNMENT_FAR)
			top -= (topBottomPadding * 2 + metrics.height + lineWidthDips * 2);
		else if (va == DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
			top -= (topBottomPadding + metrics.height + lineWidthDips);
		
		float right = left + 2 * leftRightPadding + metrics.width;
		float bottom = top + 2 * topBottomPadding + metrics.height;
		left   = roundf (left   / pixelWidthDips) * pixelWidthDips - lineWidthDips / 2;
		top    = roundf (top    / pixelWidthDips) * pixelWidthDips - lineWidthDips / 2;
		right  = roundf (right  / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;
		bottom = roundf (bottom / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;

		D2D1_ROUNDED_RECT rr = { { left, top, right, bottom }, 4, 4 };
		com_ptr<ID2D1SolidColorBrush> brush;
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
			if (STP_GetStpVersion(bridge->stp_bridge()) >= STP_VERSION_MSTP)
			{
				auto it = find (configIds.begin(), configIds.end(), *STP_GetMstConfigId(bridge->stp_bridge()));
				if (it != configIds.end())
				{
					size_t colorIndex = (std::distance (configIds.begin(), it)) % _countof(RegionColors);
					color = RegionColors[colorIndex];
				}
			}

			bridge->Render (dc, _drawing_resources, _pw->selected_vlan_number(), color);
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
			bool forwarding = _project->IsWireForwarding(w.get(), _pw->selected_vlan_number(), &hasLoop);
			w->Render (dc, _drawing_resources, forwarding, hasLoop);
		}

		dc->SetTransform(oldtr);

		if (_project->GetBridges().empty())
		{
			RenderHint (dc, { client_width() / 2, client_height() / 2 }, L"No bridges created. Right-click to create some.", DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
		}
		else if (_project->GetBridges().size() == 1)
		{
			RenderHint (dc, { client_width() / 2, client_height() / 2 }, L"Right-click to add more bridges.", DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
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
				auto dl = pointw_to_pointd(wl);
				RenderHint (dc, { dl.x, dl.y }, text, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, false);
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

	void create_render_resources (ID2D1DeviceContext* dc) final
	{
		base::create_render_resources(dc);

		HRESULT hr;
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::PaleGreen), &_drawing_resources._poweredFillBrush     ); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._unpoweredBrush       ); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._brushDiscardingPort  ); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gold),      &_drawing_resources._brushLearningPort    ); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green),     &_drawing_resources._brushForwarding      ); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._brushNoForwardingWire); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Red),       &_drawing_resources._brushLoop            ); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Blue),      &_drawing_resources._brushTempWire        ); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &_drawing_resources._brushWindowText); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOW    ), &_drawing_resources._brushWindow    ); assert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_HIGHLIGHT ), &_drawing_resources._brushHighlight ); assert(SUCCEEDED(hr));
	}

	void release_render_resources (ID2D1DeviceContext* dc) final
	{
		_drawing_resources._poweredFillBrush      = nullptr;
		_drawing_resources._unpoweredBrush        = nullptr;
		_drawing_resources._brushDiscardingPort   = nullptr;
		_drawing_resources._brushLearningPort     = nullptr;
		_drawing_resources._brushForwarding       = nullptr;
		_drawing_resources._brushNoForwardingWire = nullptr;
		_drawing_resources._brushLoop             = nullptr;
		_drawing_resources._brushTempWire         = nullptr;
		_drawing_resources._brushWindowText       = nullptr;
		_drawing_resources._brushWindow           = nullptr;
		_drawing_resources._brushHighlight        = nullptr;

		base::release_render_resources(dc);
	}

	void render(ID2D1DeviceContext* rt) const final
	{
		std::set<STP_MST_CONFIG_ID> configIds;
		for (auto& bridge : _project->GetBridges())
		{
			auto stpb = bridge->stp_bridge();
			if (STP_GetStpVersion(stpb) >= STP_VERSION_MSTP)
				configIds.insert (*STP_GetMstConfigId(stpb));
		}

		rt->Clear(GetD2DSystemColor(COLOR_WINDOW));

		RenderLegend(rt);

		RenderBridges (rt, configIds);

		RenderWires (rt);

		for (object* o : _selection->GetObjects())
		{
			if (auto ro = dynamic_cast<renderable_object*>(o))
				ro->RenderSelection(this, rt, _drawing_resources);
		}

		if (!configIds.empty())
			RenderConfigIdList (rt, configIds);

		if (_htResult.object != nullptr)
			RenderHover(rt);

		RenderHint (rt, { client_width() / 2, client_height() },
					L"Rotate mouse wheel for zooming, press wheel and drag for panning.",
					DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_FAR, true);

		if (_project->IsSimulationPaused())
			RenderHint (rt, { client_width() / 2, 10 },
						L"Simulation is paused. Right-click to resume.",
						DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);

		if (_state != nullptr)
			_state->Render(rt);
	}

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

	std::optional<LRESULT> window_proc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) final
	{
		if ((uMsg == WM_LBUTTONDOWN) || (uMsg == WM_RBUTTONDOWN))
		{
			auto button = (uMsg == WM_LBUTTONDOWN) ? MouseButton::Left : MouseButton::Right;
			UINT modifierKeysDown = (UINT) wParam;
			auto result = ProcessMouseButtonDown (button, modifierKeysDown, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return result ? result.value() : base::window_proc (hwnd, uMsg, wParam, lParam);
		}
		else if ((uMsg == WM_LBUTTONUP) || (uMsg == WM_RBUTTONUP))
		{
			auto button = (uMsg == WM_LBUTTONUP) ? MouseButton::Left : MouseButton::Right;
			UINT modifierKeysDown = (UINT) wParam;
			auto result = ProcessMouseButtonUp (button, modifierKeysDown, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return result ? result.value() : base::window_proc (hwnd, uMsg, wParam, lParam);
		}
		else if (uMsg == WM_SETCURSOR)
		{
			if (((HWND) wParam == hwnd) && (LOWORD (lParam) == HTCLIENT))
			{
				// Let's check the result because GetCursorPos fails when the input desktop is not the current desktop
				// (happens for example when the monitor goes to sleep and then the lock screen is displayed).
				POINT pt;
				if (::GetCursorPos (&pt))
				{
					if (ScreenToClient (hwnd, &pt))
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
			base::window_proc (hwnd, uMsg, wParam, lParam);
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
				for (object* o : _selection->GetObjects())
				{
					auto b = dynamic_cast<Bridge*>(o);
					if (b != nullptr)
					{
						if (enable && !STP_IsBridgeStarted(b->stp_bridge()))
							STP_StartBridge(b->stp_bridge(), GetMessageTime());
						else if (!enable && STP_IsBridgeStarted(b->stp_bridge()))
							STP_StopBridge(b->stp_bridge(), GetMessageTime());
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

		return base::window_proc (hwnd, uMsg, wParam, lParam);
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

	void DeleteSelection()
	{
		if (any_of(_selection->GetObjects().begin(), _selection->GetObjects().end(), [](object* o) { return o->is<Port>(); }))
		{
			TaskDialog (hwnd(), nullptr, _app->GetAppName(), nullptr, L"Ports cannot be deleted.", 0, nullptr, nullptr);
			return;
		}

		if (any_of (_selection->GetObjects().begin(), _selection->GetObjects().end(), [](object* o) { return o->is<Port>(); }))
		{
			TaskDialog (_pw->hwnd(), nullptr, _app->GetAppName(), L"Can't Delete Ports", L"The Simulator does not yet support deleting ports.", 0, TD_INFORMATION_ICON, nullptr);
			return;
		}

		std::set<Bridge*> bridgesToRemove;
		std::set<Wire*> wiresToRemove;
		std::unordered_map<Wire*, std::vector<size_t>> pointsToDisconnect;

		for (object* o : _selection->GetObjects())
		{
			if (auto w = dynamic_cast<Wire*>(o); w != nullptr)
				wiresToRemove.insert(w);
			else if (auto b = dynamic_cast<Bridge*>(o); b != nullptr)
				bridgesToRemove.insert(b);
			else
				assert(false);
		}

		for (auto& w : _project->GetWires())
		{
			if (wiresToRemove.find(w.get()) != wiresToRemove.end())
				continue;

			for (size_t pi = 0; pi < w->GetPoints().size(); pi++)
			{
				if (!std::holds_alternative<ConnectedWireEnd>(w->GetPoints()[pi]))
					continue;

				auto port = std::get<ConnectedWireEnd>(w->GetPoints()[pi]);
				if (bridgesToRemove.find(port->bridge()) == bridgesToRemove.end())
					continue;

				// point is connected to bridge being removed.
				pointsToDisconnect[w.get()].push_back(pi);
			}
		}

		for (auto it = pointsToDisconnect.begin(); it != pointsToDisconnect.end(); )
		{
			Wire* wire = it->first;
			bool anyPointRemainsConnected = any_of (wire->GetPoints().begin(), wire->GetPoints().end(),
				[&bridgesToRemove](auto& pt) { return std::holds_alternative<ConnectedWireEnd>(pt)
				&& (bridgesToRemove.count(std::get<ConnectedWireEnd>(pt)->bridge()) == 0); });
			
			auto it1 = it;
			it++;

			if (!anyPointRemainsConnected)
			{
				wiresToRemove.insert(wire);
				pointsToDisconnect.erase(it1);
			}
		}

		if (!bridgesToRemove.empty() || !wiresToRemove.empty() || !pointsToDisconnect.empty())
		{
			for (auto& p : pointsToDisconnect)
				for (auto pi : p.second)
					p.first->SetPoint(pi, p.first->GetPointCoords(pi));

			for (auto w : wiresToRemove)
				_project->RemoveWire(w);

			for (auto b : bridgesToRemove)
				_project->RemoveBridge(b);

			_project->SetChangedFlag(true);
		}
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
			DeleteSelection();
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
		::SetFocus(hwnd());
		if (::GetFocus() != hwnd())
			// Some validation code (maybe in the Properties Window) must have failed and taken focus back.
			return nullopt;

		MouseLocation mouseLocation;
		mouseLocation.pt = pt;
		mouseLocation.d = pointp_to_pointd(pt);
		mouseLocation.w = pointd_to_pointw(mouseLocation.d);

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
			unique_ptr<edit_state> stateMoveThreshold;
			unique_ptr<edit_state> stateButtonUp;

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
		auto dLocation = pointp_to_pointd(pt);
		auto wLocation = pointd_to_pointw(dLocation);

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

	virtual void EnterState (std::unique_ptr<edit_state>&& state) override final
	{
		_state = move(state);
		_htResult = { nullptr };
	}

	void ProcessWmSetCursor (POINT pt)
	{
		auto dLocation = pointp_to_pointd(pt);
		auto wLocation = pointd_to_pointw(dLocation);

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
				::InvalidateRect(hwnd(), nullptr, 0);
			}
		}
	}

	void ProcessWmMouseMove (POINT pt)
	{
		auto dLocation = pointp_to_pointd(pt);
		auto wLocation = pointd_to_pointw(dLocation);

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
		//D2D1_POINT_2F dipLocation = pointp_to_pointd(pt);
		//_elementsAtContextMenuLocation.clear();
		//GetElementsAt(_project->GetInnerRootElement(), { dipLocation.x, dipLocation.y }, _elementsAtContextMenuLocation);

		HMENU hMenu = nullptr;
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

		if (hMenu != nullptr)
			TrackPopupMenuEx (GetSubMenu(hMenu, 0), 0, pt.x, pt.y, hwnd, nullptr);

		return 0;
	}

	virtual const struct drawing_resources& drawing_resources() const override final { return _drawing_resources; }

	virtual D2D1::Matrix3x2F GetZoomTransform() const override final { return base::GetZoomTransform(); }

	EditStateDeps MakeEditStateDeps()
	{
		return EditStateDeps { _pw, this, _project, _selection };
	}
};

template<typename... Args>
static std::unique_ptr<edit_area_i> Create (Args... args)
{
	return std::make_unique<edit_area>(std::forward<Args>(args)...);
}

extern const edit_area_factory_t edit_area_factory = &Create;