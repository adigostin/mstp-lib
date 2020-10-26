
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "resource.h"
#include "edit_states/edit_state.h"
#include "bridge.h"
#include "port.h"
#include "wire.h"
#include "zoomer.h"
#include "window.h"
#include "d2d_renderer.h"

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

class edit_window : public event_manager, public edit_window_i
{
	using ht_result = renderable_object::ht_result;

	simulator_app_i*  const _app;
	project_window_i* const _pw;
	project_i*        const _project;
	selection_i*      const _selection;
	edge::window       _window;   // this one first, to be constructed first
	edge::d2d_renderer _renderer; // this one second, cause it uses _window
	edge::zoomer       _zoomer;   // this one last, cause it uses _renderer
	com_ptr<IDWriteTextFormat> _legendFont;
	struct drawing_resources _drawing_resources;
	std::unique_ptr<edit_state> _state;
	ht_result _htResult = { nullptr, 0 };

public:
	edit_window (const edit_window_create_params& cps)
		: _window(WS_EX_CLIENTEDGE, WS_CHILD | WS_VISIBLE, cps.hWndParent, cps.rect)
		, _renderer(this, cps.d3d_dc, cps.dWriteFactory)
		, _zoomer(this)
		, _app(cps.app)
		, _pw(cps.pw)
		, _project(cps.project)
		, _selection(cps.selection)
	{
		HRESULT hr;

		_drawing_resources._dWriteFactory = cps.dWriteFactory;
		hr = cps.dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_drawing_resources._regularTextFormat); rassert(SUCCEEDED(hr));
		hr = cps.dWriteFactory->CreateTextFormat (L"Tahoma", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"en-US", &_drawing_resources._smallTextFormat); rassert(SUCCEEDED(hr));
		hr = cps.dWriteFactory->CreateTextFormat (L"Tahoma", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"en-US", &_drawing_resources._smallBoldTextFormat); rassert(SUCCEEDED(hr));
		cps.dWriteFactory->CreateTextFormat (L"Tahoma", nullptr,  DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_CONDENSED, 11, L"en-US", &_legendFont); rassert(SUCCEEDED(hr));

		_selection->changed().add_handler<&edit_window::on_selection_changed>(this);
		_project->property_changing().add_handler<&edit_window::on_project_property_changing>(this);
		_project->invalidated().add_handler<&edit_window::on_project_invalidated>(this);
		_pw->selected_vlan_number_changed().add_handler<&edit_window::on_selected_vlan_changed>(this);

		_window.window_proc().add_handler<&edit_window::on_window_proc>(this);

		_renderer.render().add_handler<&edit_window::on_render>(this);
		create_render_resources(_renderer.dc());
		_renderer.dc_releasing().add_handler<&edit_window::release_render_resources>(this);
		_renderer.dc_recreated().add_handler<&edit_window::create_render_resources>(this);
	}

	virtual ~edit_window()
	{
		_renderer.dc_recreated().remove_handler<&edit_window::create_render_resources>(this);
		_renderer.dc_releasing().remove_handler<&edit_window::release_render_resources>(this);
		release_render_resources(_renderer.dc());
		_renderer.render().remove_handler<&edit_window::on_render>(this);

		_window.window_proc().remove_handler<&edit_window::on_window_proc>(this);

		_pw->selected_vlan_number_changed().remove_handler<&edit_window::on_selected_vlan_changed>(this);
		_project->invalidated().remove_handler<&edit_window::on_project_invalidated>(this);
		_project->property_changing().remove_handler<&edit_window::on_project_property_changing>(this);
		_selection->changed().remove_handler<&edit_window::on_selection_changed>(this);
	}

	// win32_window_i
	virtual HWND hwnd() const override { return _window.hwnd(); }
	virtual window_proc_e::subscriber window_proc() override { return _window.window_proc(); }

	// d2d_window_i
	virtual d2d_renderer& renderer() override final { return _renderer; }
	virtual void show_caret (const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color, const D2D1_MATRIX_3X2_F* transform = nullptr) override { _renderer.show_caret(bounds, color, transform); }
	virtual void hide_caret() override { _renderer.hide_caret(); }

	// zoomable_window_i
	virtual D2D1_POINT_2F aimpoint() const override { return _zoomer.aimpoint(); }
	virtual float zoom() const override { return _zoomer.zoom(); }
	virtual zoom_transform_changed_e::subscriber zoom_transform_changed() override { return _zoomer.zoom_transform_changed(); }
	virtual void zoom_to (const D2D1_RECT_F& rect, float min_margin, float min_zoom, float max_zoom, bool smooth) override { _zoomer.zoom_to(rect, min_margin, min_zoom, max_zoom, smooth); }

	void on_selected_vlan_changed (project_window_i* pw, unsigned int vlanNumber)
	{
		invalidate();
	}

	void on_project_property_changing (object* project_obj, const property_change_args& args)
	{
		auto project = dynamic_cast<project_i*>(project_obj);

		if ((args.property == project->bridges_prop())
			&& (args.type == collection_property_change_type::remove)
			&& (_htResult.object == project->bridges()[args.index].get()))
		{
			_htResult = { nullptr, 0 };
			::InvalidateRect (hwnd(), nullptr, FALSE);
		}
		else if ((args.property == project->wires_prop())
			&& (args.type == collection_property_change_type::remove)
			&& (_htResult.object == project->wires()[args.index].get()))
		{
			_htResult = { nullptr, 0 };
			::InvalidateRect (hwnd(), nullptr, FALSE);
		}
	}

	void on_project_invalidated (project_i*)
	{
		invalidate();
	}

	void on_selection_changed (selection_i* selection)
	{
		invalidate();
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
		{ L"Disabled",               STP_PORT_ROLE_DISABLED,   false, false, false },

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
	};

	void render_legend (ID2D1DeviceContext* dc) const
	{
		float maxLineWidth = 0;
		float maxLineHeight = 0;
		std::vector<text_layout> layouts;
		for (auto& info : LegendInfo)
		{
			auto tl = text_layout (_renderer.dwrite_factory(), _legendFont, info.text);

			DWRITE_TEXT_METRICS metrics;
			tl->GetMetrics (&metrics);

			if (metrics.width > maxLineWidth)
				maxLineWidth = metrics.width;

			if (metrics.height > maxLineHeight)
				maxLineHeight = metrics.height;

			layouts.push_back(std::move(tl));
		}

		float textX = client_width() - (5 + maxLineWidth + 5 + port::ExteriorHeight + 5);
		float lineX = textX - 3;
		float bitmapX = client_width() - (5 + port::ExteriorHeight + 5);
		float rowHeight = 2 + std::max (maxLineHeight, port::ExteriorWidth);
		float y = client_height() - _countof(LegendInfo) * rowHeight;

		auto lineWidth = pixel_width();

		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
		auto tooltip_back_brush = _app->make_brush(dc, theme_color::tooltip_back, 0.8f);
		dc->FillRectangle (D2D1_RECT_F { lineX, y, client_width(), client_height() }, tooltip_back_brush);
		auto tooltip_fore_brush = _app->make_brush(dc, theme_color::tooltip_fore);
		dc->DrawLine ({ lineX, y }, { lineX, client_height() }, tooltip_fore_brush, lineWidth);
		dc->SetAntialiasMode (oldaa);

		Matrix3x2F oldtr;
		dc->GetTransform (&oldtr);

		for (size_t i = 0; i < _countof(LegendInfo); i++)
		{
			auto& info = LegendInfo[i];

			auto oldaa = dc->GetAntialiasMode();
			dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
			dc->DrawLine (Point2F (lineX, y), Point2F (client_width(), y), tooltip_fore_brush, lineWidth);
			dc->SetAntialiasMode(oldaa);

			dc->DrawTextLayout (Point2F (textX, y + 1), layouts[i], tooltip_fore_brush);

			// Rotate 270 degrees and then translate.
			Matrix3x2F tr (0, -1, 1, 0, bitmapX, y + rowHeight / 2);
			dc->SetTransform (tr * oldtr);
			port::RenderExteriorStpPort (dc, _drawing_resources, info.role, info.learning, info.forwarding, info.operEdge);
			dc->SetTransform (&oldtr);

			y += rowHeight;
		}
	}

	void render_config_id_list (ID2D1DeviceContext* dc, const std::set<STP_MST_CONFIG_ID>& configIds) const
	{
		size_t colorIndex = 0;

		float maxLineWidth = 0;
		float lineHeight = 0;
		std::vector<std::pair<text_layout, D2D1_COLOR_F>> lines;
		for (const STP_MST_CONFIG_ID& configId : configIds)
		{
			std::stringstream ss;
			ss << configId.ConfigurationName << " -- " << (configId.RevisionLevelLow | (configId.RevisionLevelHigh << 8)) << " -- "
				<< std::uppercase << std::setfill('0') << std::hex
				<< std::setw(2) << (int)configId.ConfigurationDigest[0] << std::setw(2) << (int)configId.ConfigurationDigest[1] << ".."
				<< std::setw(2) << (int)configId.ConfigurationDigest[14] << std::setw(2) << (int)configId.ConfigurationDigest[15];
			auto tl = text_layout_with_metrics (_renderer.dwrite_factory(), _legendFont, ss.str());

			if (tl.width() > maxLineWidth)
				maxLineWidth = tl.width();

			if (tl.height() > lineHeight)
				lineHeight = tl.height();

			lines.push_back ({ std::move(tl), RegionColors[colorIndex] });
			colorIndex = (colorIndex + 1) % _countof(RegionColors);
		}

		float LeftRightPadding = 3;
		float UpDownPadding = 2;
		float coloredRectWidth = lineHeight * 2;

		auto title = text_layout_with_metrics (_renderer.dwrite_factory(), _legendFont, "MST Regions:");

		float y = client_height() - lines.size() * (lineHeight + 2 * UpDownPadding) - title.height() - 2 * UpDownPadding;

		auto fore_brush = _app->make_brush(dc, theme_color::foreground);

		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		float lineWidth = pixel_width();
		float lineX = LeftRightPadding + coloredRectWidth + LeftRightPadding + maxLineWidth + LeftRightPadding;
		auto brush = _app->make_brush(dc, theme_color::tooltip_back, 0.8f);
		dc->FillRectangle ({ 0, y, lineX, client_height() }, brush);
		dc->DrawLine ({ 0, y }, { lineX, y }, fore_brush, lineWidth);
		dc->DrawLine ({ lineX, y }, { lineX, client_height() }, fore_brush, lineWidth);
		dc->SetAntialiasMode(oldaa);

		dc->DrawTextLayout ({ LeftRightPadding, y + UpDownPadding }, title.layout(), fore_brush);
		y += (title.height() + 2 * UpDownPadding);

		for (auto& p : lines)
		{
			com_ptr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (p.second, &brush);
			D2D1_RECT_F rect = { LeftRightPadding, y + UpDownPadding, LeftRightPadding + coloredRectWidth, y + UpDownPadding + lineHeight };
			dc->FillRectangle (&rect, brush);
			D2D1_POINT_2F pt = { LeftRightPadding + coloredRectWidth + LeftRightPadding, y + UpDownPadding };
			dc->DrawTextLayout (pt, p.first.layout(), fore_brush);
			y += (lineHeight + 2 * UpDownPadding);
		}
	}

	virtual void RenderSnapRect (ID2D1DeviceContext* dc, D2D1_POINT_2F wLocation) const override final
	{
		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		auto cpd = pointw_to_pointd(wLocation);
		auto rect = RectF (cpd.x - SnapDistance, cpd.y - SnapDistance, cpd.x + SnapDistance, cpd.y + SnapDistance);
		dc->DrawRectangle (rect, _drawing_resources._brushHighlight, 2);

		dc->SetAntialiasMode(oldaa);
	}

	virtual void render_hint (ID2D1DeviceContext* dc,
							 D2D1_POINT_2F dLocation,
							 std::string_view text,
							 DWRITE_TEXT_ALIGNMENT ha,
							 DWRITE_PARAGRAPH_ALIGNMENT va,
							 bool smallFont) const override final
	{
		float leftRightPadding = 3;
		float topBottomPadding = 1.5f;
		auto textFormat = smallFont ? _drawing_resources._smallTextFormat.get() : _drawing_resources._regularTextFormat.get();
		auto tl = edge::text_layout_with_metrics (_drawing_resources._dWriteFactory, textFormat, text);

		float pixelWidthDips = pixel_width();
		float lineWidthDips = roundf(1.0f / pixelWidthDips) * pixelWidthDips;

		float left = dLocation.x - leftRightPadding;
		if (ha == DWRITE_TEXT_ALIGNMENT_CENTER)
			left -= tl.width() / 2;
		else if (ha == DWRITE_TEXT_ALIGNMENT_TRAILING)
			left -= tl.width();

		float top = dLocation.y;
		if (va == DWRITE_PARAGRAPH_ALIGNMENT_FAR)
			top -= (topBottomPadding * 2 + tl.height() + lineWidthDips * 2);
		else if (va == DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
			top -= (topBottomPadding + tl.height() + lineWidthDips);

		float right = left + 2 * leftRightPadding + tl.width();
		float bottom = top + 2 * topBottomPadding + tl.height();
		left   = roundf (left   / pixelWidthDips) * pixelWidthDips - lineWidthDips / 2;
		top    = roundf (top    / pixelWidthDips) * pixelWidthDips - lineWidthDips / 2;
		right  = roundf (right  / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;
		bottom = roundf (bottom / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;

		auto back_brush = _app->make_brush(dc, theme_color::tooltip_back);
		auto fore_brush = _app->make_brush(dc, theme_color::tooltip_fore);
		D2D1_ROUNDED_RECT rr = { { left, top, right, bottom }, 4, 4 };
		dc->FillRoundedRectangle (&rr, back_brush);
		dc->DrawRoundedRectangle (&rr, fore_brush, lineWidthDips);
		dc->DrawTextLayout ({ rr.rect.left + leftRightPadding, rr.rect.top + topBottomPadding }, tl.layout(), fore_brush);
	}

	void render_bridges (ID2D1DeviceContext* dc, const std::set<STP_MST_CONFIG_ID>& configIds) const
	{
		Matrix3x2F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform (zoom_transform() * oldtr);

		for (auto& bridge : _project->bridges())
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

			bridge->render (dc, _drawing_resources, _pw->selected_vlan_number(), color);
		}

		dc->SetTransform(oldtr);
	}

	void render_wires (ID2D1DeviceContext* dc) const
	{
		Matrix3x2F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform (zoom_transform() * oldtr);

		for (auto& w : _project->wires())
		{
			bool hasLoop;
			bool forwarding = _project->IsWireForwarding(w.get(), _pw->selected_vlan_number(), &hasLoop);
			w->render (dc, _drawing_resources, forwarding, hasLoop);
		}

		dc->SetTransform(oldtr);

		// TODO: move this out of this function
		if (_project->bridges().empty())
		{
			render_hint (dc, { client_width() / 2, client_height() / 2 }, "No bridges created. Right-click to create some.", DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
		}
		else if (_project->bridges().size() == 1)
		{
			render_hint (dc, { client_width() / 2, client_height() / 2 }, "Right-click to add more bridges.", DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
		}
		else
		{
			bool anyPortConnected = false;
			for (auto& b : _project->bridges())
				anyPortConnected |= any_of (b->ports().begin(), b->ports().end(),
											[this](const std::unique_ptr<port>& p) { return _project->GetWireConnectedToPort(p.get()).first != nullptr; });

			if (!anyPortConnected)
			{
				bridge* b = _project->bridges().front().get();
				auto text = "No port connected. You can connect\r\nports by drawing wires with the mouse.";
				auto wl = D2D1_POINT_2F { b->left() + b->width() / 2, b->bottom() + port::ExteriorHeight * 1.5f };
				auto dl = pointw_to_pointd(wl);
				render_hint (dc, { dl.x, dl.y }, text, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, false);
			}
		}
	}

	void render_hover (ID2D1DeviceContext* dc) const
	{
		if (dynamic_cast<port*>(_htResult.object) != nullptr)
		{
			if (_htResult.code == port::HTCodeCP)
				RenderSnapRect (dc, static_cast<port*>(_htResult.object)->GetCPLocation());
		}
		else if (dynamic_cast<wire*>(_htResult.object) != nullptr)
		{
			if (_htResult.code >= 0)
				RenderSnapRect (dc, static_cast<wire*>(_htResult.object)->point_coords(_htResult.code));
		}
	}

	void create_render_resources (ID2D1DeviceContext* dc)
	{
		HRESULT hr;
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::PaleGreen), &_drawing_resources._poweredFillBrush     ); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._unpoweredBrush       ); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._brushDiscardingPort  ); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gold),      &_drawing_resources._brushLearningPort    ); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green),     &_drawing_resources._brushForwarding      ); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._brushNoForwardingWire); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Red),       &_drawing_resources._brushLoop            ); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Blue),      &_drawing_resources._brushTempWire        ); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (_app->color_d2d(theme_color::foreground), &_drawing_resources._brushWindowText); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (_app->color_d2d(theme_color::background), &_drawing_resources._brushWindow    ); rassert(SUCCEEDED(hr));
		hr = dc->CreateSolidColorBrush (_app->color_d2d(theme_color::selected_back_focused), &_drawing_resources._brushHighlight ); rassert(SUCCEEDED(hr));

		com_ptr<ID2D1Factory> factory;
		dc->GetFactory(&factory);

		D2D1_STROKE_STYLE_PROPERTIES ssprops = {};
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleSelectionRect); rassert(SUCCEEDED(hr));

		ssprops = { };
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleNoForwardingWire); rassert(SUCCEEDED(hr));

		ssprops = { };
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleForwardingWire); rassert(SUCCEEDED(hr));
	}

	void release_render_resources (ID2D1DeviceContext* dc)
	{
		_drawing_resources._strokeStyleForwardingWire = nullptr;
		_drawing_resources._strokeStyleNoForwardingWire = nullptr;
		_drawing_resources._strokeStyleSelectionRect = nullptr;

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
	}

	void on_render (ID2D1DeviceContext* dc)
	{
		std::set<STP_MST_CONFIG_ID> configIds;
		for (auto& bridge : _project->bridges())
		{
			auto stpb = bridge->stp_bridge();
			if (STP_GetStpVersion(stpb) >= STP_VERSION_MSTP)
				configIds.insert (*STP_GetMstConfigId(stpb));
		}

		dc->Clear(_app->color_d2d(theme_color::background));

		dc->SetTransform(dpi_transform());

		render_legend(dc);

		render_bridges (dc, configIds);

		render_wires (dc);

		for (object* o : _selection->objects())
		{
			if (auto ro = dynamic_cast<renderable_object*>(o))
				ro->render_selection(this, dc, _drawing_resources);
		}

		if (!configIds.empty())
			render_config_id_list (dc, configIds);

		if (_htResult.object != nullptr)
			render_hover(dc);

		render_hint (dc, { client_width() / 2, client_height() },
					"Rotate mouse wheel for zooming, press wheel and drag for panning.",
					DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_FAR, true);

		if (_project->simulation_paused())
			render_hint (dc, { client_width() / 2, 10 },
						"Simulation is paused. Right-click to resume.",
						DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);

		if (_state != nullptr)
			_state->render(dc);
	}

	std::optional<LRESULT> on_window_proc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if ((msg == WM_LBUTTONDOWN) || (msg == WM_RBUTTONDOWN))
		{
			auto button = (msg == WM_LBUTTONDOWN) ? mouse_button::left : mouse_button::right;
			auto handled = process_mouse_button_down (hwnd, button, (modifier_key)wparam, { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			if (handled)
				return 0;
			return std::nullopt;
		}

		if ((msg == WM_LBUTTONUP) || (msg == WM_RBUTTONUP))
		{
			auto button = (msg == WM_LBUTTONUP) ? mouse_button::left : mouse_button::right;
			auto handled = process_mouse_button_up (button, (modifier_key)wparam, { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			if (handled)
				return 0;
			return std::nullopt;
		}

		if (msg == WM_SETCURSOR)
		{
			if (((HWND) wparam == hwnd) && (LOWORD (lparam) == HTCLIENT))
			{
				// Let's check the result because GetCursorPos fails when the input desktop is not the current desktop
				// (happens for example when the monitor goes to sleep and then the lock screen is displayed).
				POINT pt;
				if (::GetCursorPos (&pt))
				{
					if (ScreenToClient (hwnd, &pt))
					{
						::SetCursor(cursor_at(pt));
						return TRUE;
					}
				}
			}

			return std::nullopt;
		}

		if (msg == WM_MOUSEMOVE)
		{
			process_mouse_move (hwnd, (modifier_key)wparam, { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			return std::nullopt;
		}

		if (msg == WM_CONTEXTMENU)
		{
			ProcessWmContextMenu (hwnd, POINT{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			return 0;
		}

		if ((msg == WM_KEYDOWN) || (msg == WM_SYSKEYDOWN))
		{
			auto handled = process_key_or_syskey_down ((UINT) wparam, get_modifier_keys());
			if (handled)
				return 0;
			return std::nullopt;
		}

		if ((msg == WM_KEYUP) || (msg == WM_SYSKEYUP))
		{
			auto handled = process_key_or_syskey_up ((UINT) wparam, get_modifier_keys());
			if (handled)
				return 0;
			return std::nullopt;
		}

		if (msg == WM_COMMAND)
		{
			if (wparam == ID_NEW_BRIDGE)
			{
				EnterState (create_state_create_bridge(make_edit_state_deps()));
			}
			else if ((wparam == ID_BRIDGE_ENABLE_STP) || (wparam == ID_BRIDGE_DISABLE_STP))
			{
				bool enable = (wparam == ID_BRIDGE_ENABLE_STP);
				for (object* o : _selection->objects())
				{
					if (auto b = dynamic_cast<bridge*>(o))
						b->set_stp_enabled(enable);
				}

				_project->SetChangedFlag(true);
			}
			else if (wparam == ID_PAUSE_SIMULATION)
			{
				_project->pause_simulation();
			}
			else if (wparam == ID_RESUME_SIMULATION)
			{
				_project->resume_simulation();
			}

			return 0;
		}

		return std::nullopt;
	}

	virtual port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const override final
	{
		auto& bridges = _project->bridges();
		for (auto it = bridges.rbegin(); it != bridges.rend(); it++)
		{
			auto ht = it->get()->hit_test(this, dLocation, tolerance);
			if (ht.object != nullptr)
			{
				auto port = dynamic_cast<class port*>(ht.object);
				if ((port != nullptr) && (ht.code == port::HTCodeCP))
					return port;
				else
					return nullptr;
			}
		}

		return nullptr;
	}

	ht_result hit_test_objects (D2D1_POINT_2F pd, float tolerance) const
	{
		auto& wires = _project->wires();
		for (auto it = wires.rbegin(); it != wires.rend(); it++)
		{
			auto ht = it->get()->hit_test (this, pd, tolerance);
			if (ht.object != nullptr)
				return ht;
		}

		auto& bridges = _project->bridges();
		for (auto it = bridges.rbegin(); it != bridges.rend(); it++)
		{
			auto ht = it->get()->hit_test(this, pd, tolerance);
			if (ht.object != nullptr)
				return ht;
		}

		return { };
	}

	void delete_selection()
	{
		static constexpr auto is_port = [](const object* o) { return o->type()->is_same_or_derived_from(&port::_type); };
		if (any_of(_selection->objects().begin(), _selection->objects().end(), is_port))
		{
			MessageBoxA (hwnd(), "Ports cannot be deleted.", _app->app_name(), 0);
			return;
		}

		std::set<bridge*> bridgesToRemove;
		std::set<wire*> wiresToRemove;
		std::unordered_map<wire*, std::vector<size_t>> pointsToDisconnect;

		for (object* o : _selection->objects())
		{
			if (auto w = dynamic_cast<wire*>(o); w != nullptr)
				wiresToRemove.insert(w);
			else if (auto b = dynamic_cast<bridge*>(o); b != nullptr)
				bridgesToRemove.insert(b);
			else
				rassert(false);
		}

		for (auto& w : _project->wires())
		{
			if (wiresToRemove.find(w.get()) != wiresToRemove.end())
				continue;

			for (size_t pi = 0; pi < w->points().size(); pi++)
			{
				if (!std::holds_alternative<connected_wire_end>(w->points()[pi]))
					continue;

				auto port = std::get<connected_wire_end>(w->points()[pi]);
				if (bridgesToRemove.find(port->bridge()) == bridgesToRemove.end())
					continue;

				// point is connected to bridge being removed.
				pointsToDisconnect[w.get()].push_back(pi);
			}
		}

		for (auto it = pointsToDisconnect.begin(); it != pointsToDisconnect.end(); )
		{
			wire* wire = it->first;
			bool anyPointRemainsConnected = any_of (wire->points().begin(), wire->points().end(),
				[&bridgesToRemove, this](auto& pt) { return std::holds_alternative<connected_wire_end>(pt)
					&& (bridgesToRemove.count(std::get<connected_wire_end>(pt)->bridge()) == 0); });

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
					p.first->set_point(pi, p.first->point_coords(pi));

			for (auto w : wiresToRemove)
				_project->wire_collection_i::remove(w);

			for (auto b : bridgesToRemove)
				_project->bridge_collection_i::remove(b);

			_project->SetChangedFlag(true);
		}
	}

	handled process_key_or_syskey_down (uint32_t vkey, modifier_key mks)
	{
		if (_state)
		{
			handled h = _state->process_key_or_syskey_down (vkey, mks);
			if (_state->completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			}

			return h;
		}

		if (vkey == VK_DELETE)
		{
			delete_selection();
			return handled(true);
		}

		return handled(false);
	}

	handled process_key_or_syskey_up (uint32_t vkey, modifier_key mks)
	{
		if (_state)
		{
			handled h = _state->process_key_or_syskey_up (vkey, mks);
			if (_state->completed())
			{
				_state = nullptr;
				SetCursor (LoadCursor (nullptr, IDC_ARROW));
			}

			return h;
		}

		return handled(false);
	}

	handled process_mouse_button_down (HWND hwnd, mouse_button button, modifier_key mks, POINT pp)
	{
		auto pd = pointp_to_pointd(pp);

		::SetFocus(hwnd);
		if (::GetFocus() != hwnd)
			// Some validation code (maybe in the Properties Window) must have failed and taken focus back.
			return handled(true);

		mouse_location ml;
		ml.pt = pp;
		ml.d = pointp_to_pointd(pp);
		ml.w = pointd_to_pointw(ml.d);

		if (_state)
		{
			auto handled = _state->process_mouse_button_down (button, mks, ml);
			if (_state->completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return handled;
		}

		auto ht = hit_test_objects (ml.d, SnapDistance);
		if (ht.object == nullptr)
			_selection->clear();
		else
		{
			if ((UINT)mks & MK_CONTROL)
			{
				if (_selection->contains(ht.object))
					_selection->remove(ht.object);
				else if (!_selection->objects().empty() && (typeid(*_selection->objects()[0]) == typeid(*ht.object)))
					_selection->add(ht.object);
				else
					_selection->select(ht.object);
			}
			else
			{
				if (!_selection->contains(ht.object))
					_selection->select(ht.object);
			}
		}

		if (button == mouse_button::left)
		{
			if (ht.object == nullptr)
			{
				// TODO: area selection
				//stateForMoveThreshold =
				return handled(true);
			}
			else
			{
				std::unique_ptr<edit_state> stateMoveThreshold;
				std::unique_ptr<edit_state> stateButtonUp;

				if (dynamic_cast<bridge*>(ht.object) != nullptr)
				{
					if (button == mouse_button::left)
						stateMoveThreshold = create_state_move_bridges (make_edit_state_deps());
				}
				else if (dynamic_cast<port*>(ht.object) != nullptr)
				{
					auto port = dynamic_cast<class port*>(ht.object);

					if (ht.code == port::HTCodeInnerOuter)
					{
						if ((button == mouse_button::left) && (_selection->objects().size() == 1) && (dynamic_cast<class port*>(_selection->objects()[0]) != nullptr))
							stateMoveThreshold = create_state_move_port (make_edit_state_deps());
					}
					else if (ht.code == port::HTCodeCP)
					{
						auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
						if (alreadyConnectedWire.first == nullptr)
						{
							stateMoveThreshold = create_state_create_wire(make_edit_state_deps());
							stateButtonUp = create_state_create_wire(make_edit_state_deps());
						}
					}
				}
				else if (dynamic_cast<wire*>(ht.object) != nullptr)
				{
					auto w = static_cast<wire*>(ht.object);
					if (ht.code >= 0)
					{
						stateMoveThreshold = CreateStateMoveWirePoint(make_edit_state_deps(), w, ht.code);
						stateButtonUp = CreateStateMoveWirePoint (make_edit_state_deps(), w, ht.code);
					}
				}

				auto state = CreateStateBeginningDrag(make_edit_state_deps(), ht.object, button, mks, ml, ::GetCursor(), std::move(stateMoveThreshold), std::move(stateButtonUp));
				EnterState(std::move(state));
				return handled(true);
			}
		}

		return handled(false);
	}

	handled process_mouse_button_up (mouse_button button, modifier_key mks, POINT pp)
	{
		auto pd = pointp_to_pointd(pp);
		auto wLocation = pointd_to_pointw(pd);

		if (_state != nullptr)
		{
			auto handled = _state->process_mouse_button_up (button, mks, { pp, pd, wLocation });
			if (_state->completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return handled;
		}

		if (button == mouse_button::right)
			return handled(false); // return "not handled", to cause our caller to pass the message to DefWindowProc, which will generate WM_CONTEXTMENU

		return handled(true);
	}

	virtual void EnterState (std::unique_ptr<edit_state>&& state) override final
	{
		_state = std::move(state);
		_htResult = { nullptr };
	}

	HCURSOR cursor_at (POINT pp) const
	{
		auto pd = pointp_to_pointd(pp);
		auto wLocation = pointd_to_pointw(pd);

		if (_state != nullptr)
			return _state->cursor();

		auto ht = hit_test_objects (pd, SnapDistance);

		LPCWSTR idc = IDC_ARROW;
		if (dynamic_cast<port*>(ht.object))
		{
			if (ht.code == port::HTCodeCP)
				idc = IDC_CROSS;
		}
		else if (dynamic_cast<wire*>(ht.object))
		{
			if (ht.code >= 0)
				// wire point
				idc = IDC_CROSS;
			else
				// wire line
				idc = IDC_ARROW;
		}

		return LoadCursor(nullptr, idc);
	}

	void process_mouse_move (HWND hwnd, modifier_key mks, POINT pp)
	{
		auto pd = pointp_to_pointd(pp);
		auto pw = pointd_to_pointw(pd);

		if (_state != nullptr)
		{
			_state->process_mouse_move ({ pp, pd, pw });
			if (_state->completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			}

			return;
		}

		auto ht = hit_test_objects (pd, SnapDistance);
		if (_htResult != ht)
		{
			_htResult = ht;
			invalidate();
		}
	}

	void ProcessWmContextMenu (HWND hwnd, POINT pt)
	{
		//D2D1_POINT_2F dipLocation = pointp_to_pointd(pt);
		//_elementsAtContextMenuLocation.clear();
		//GetElementsAt(_project->GetInnerRootElement(), { dipLocation.x, dipLocation.y }, _elementsAtContextMenuLocation);

		HMENU menu = nullptr;
		if (_selection->objects().empty())
		{
			menu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_EMPTY_SPACE));
			::EnableMenuItem (menu, ID_PAUSE_SIMULATION, _project->simulation_paused() ? MF_DISABLED : MF_ENABLED);
			::EnableMenuItem (menu, ID_RESUME_SIMULATION, _project->simulation_paused() ? MF_ENABLED : MF_DISABLED);
		}
		else if (dynamic_cast<bridge*>(_selection->objects().front()) != nullptr)
		{
			menu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_BRIDGE));
			static const auto is_bridge_with_stp_enabled = [](object* o) { return dynamic_cast<bridge*>(o) && static_cast<bridge*>(o)->stp_enabled(); };
			static const auto is_bridge_with_stp_disabled = [](object* o) { return dynamic_cast<bridge*>(o) && !static_cast<bridge*>(o)->stp_enabled(); };
			bool any_enabled = std::any_of (_selection->objects().begin(), _selection->objects().end(), is_bridge_with_stp_enabled);
			bool any_disabled = std::any_of (_selection->objects().begin(), _selection->objects().end(), is_bridge_with_stp_disabled);
			::EnableMenuItem (menu, ID_BRIDGE_DISABLE_STP, any_enabled ? MF_ENABLED : MF_DISABLED);
			::EnableMenuItem (menu, ID_BRIDGE_ENABLE_STP, any_disabled ? MF_ENABLED : MF_DISABLED);
		}

		if (menu)
			TrackPopupMenuEx (GetSubMenu(menu, 0), 0, pt.x, pt.y, hwnd, nullptr);
	}

	virtual const struct drawing_resources& drawing_resources() const override final { return _drawing_resources; }

	virtual void zoom_all() override
	{
		if (!_project->bridges().empty() || !_project->wires().empty())
		{
			auto r = _project->bridges().empty() ? _project->wires()[0]->extent() : _project->bridges()[0]->extent();
			for (auto& b : _project->bridges())
				r = union_rects(r, b->extent());
			for (auto& w : _project->wires())
				r = union_rects(r, w->extent());

			this->zoom_to (r, 20, 0, 1.5f, false);
		}
	}

	edit_state_deps make_edit_state_deps()
	{
		return edit_state_deps { _pw, this, _project, _selection };
	}
};

extern std::unique_ptr<edit_window_i> edit_window_factory (const edit_window_create_params& create_params)
{
	return std::make_unique<edit_window>(create_params);
};
