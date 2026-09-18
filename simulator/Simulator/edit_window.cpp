
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "resource.h"
#include "edit_states/edit_state.h"
#include "dispids.h"

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

class edit_window : public IEditWindow
	, IPropertyChangeSink
	, IInvalidateSink
	, ID2DRenderEventsSink
	, IVlanSelectionEvents
	, IObjectCollectionChangeEvents
{
	using ht_result = std::pair<ISelectableObject*, int32_t>;

	ULONG _refCount = 0;
	ULONG _sig = 0xAA550002;
	ISimulatorApp*  _app;
	com_ptr<ID2DThemeColorProvider> _tcp;
	IProjectWindow* _pw;
	com_ptr<IVlanSelection> _vlanSel;
	IStpProject*      _project;
	com_ptr<ISelection> _selection;
	wil::unique_hwnd _hWnd;
	com_ptr<ID2DRenderer> _renderer;
	wistd::unique_ptr<IZoomer>   _zoomer;
	com_ptr<IDWriteTextFormat> _legendFont;
	struct drawing_resources _drawing_resources;
	std::unique_ptr<edit_state> _state;
	ht_result _htResult = { nullptr, 0 };
	WeakRefToThis _weakRefToThis;
	AdviseSinkToken _selectionChangeToken;
	AdviseSinkToken _projectPropChangeToken;
	AdviseSinkToken _renderSink;
	AdviseSinkToken _projectInvalidateToken;
	AdviseSinkToken _vlanSelectionToken;
	std::unordered_map<IUnknown*, AdviseSinkToken> _bridgeWiresInvalidateTokens;

public:
	HRESULT InitInstance (const EditWindowCreateParams& cps)
	{
		HRESULT hr;

		_app = cps.app;
		_pw = cps.pw;
		_project = cps.project;
		_selection = cps.selection;

		hr = _app->GetThemeColorProvider()->QueryInterface(IID_PPV_ARGS(&_tcp)); RETURN_IF_FAILED(hr);

		static const WNDCLASS wnd_class = {
			.style = CS_DBLCLKS | CS_HREDRAW | CS_VREDRAW,
			.lpfnWndProc = WndProcStatic,
			.hInstance = (HINSTANCE)&__ImageBase,
			.hCursor = ::LoadCursor(nullptr, IDC_ARROW),
			.lpszClassName = L"edit_window",
		};

		auto atom = RegisterClass(&wnd_class);

		int x = cps.rect.left;
		int y = cps.rect.top;
		int w = cps.rect.right - cps.rect.left;
		int h = cps.rect.bottom - cps.rect.top;
		_hWnd.reset (CreateWindowEx (WS_EX_CLIENTEDGE, wnd_class.lpszClassName, L"", WS_CHILD | WS_VISIBLE,
									 x, y, w, h, cps.hWndParent, nullptr, (HINSTANCE)&__ImageBase, nullptr));
		RETURN_LAST_ERROR_IF_NULL(_hWnd);
		SetWindowLongPtr (_hWnd.get(), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		hr = MakeD2DRenderer(_hWnd.get(), _app->GetD3DDC(), _app->GetDWriteFactory(), _app->GetD2DFactory(), &_renderer); RETURN_IF_FAILED(hr);
		hr = MakeZoomer(_hWnd.get(), _zoomer); RETURN_IF_FAILED(hr);

		hr = _weakRefToThis.InitInstance(static_cast<IEditWindow*>(this)); LOG_IF_FAILED(hr);

		_drawing_resources._dWriteFactory = _app->GetDWriteFactory();
		hr = _app->GetDWriteFactory()->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 12, L"en-US", &_drawing_resources._regularTextFormat); RETURN_IF_FAILED(hr);
		hr = _app->GetDWriteFactory()->CreateTextFormat (L"Tahoma", nullptr, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"en-US", &_drawing_resources._smallTextFormat); RETURN_IF_FAILED(hr);
		hr = _app->GetDWriteFactory()->CreateTextFormat (L"Tahoma", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL, 9.5f, L"en-US", &_drawing_resources._smallBoldTextFormat); RETURN_IF_FAILED(hr);
		_app->GetDWriteFactory()->CreateTextFormat (L"Tahoma", nullptr,  DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_CONDENSED, 11, L"en-US", &_legendFont); RETURN_IF_FAILED(hr);

		hr = AdviseSink<IObjectCollectionChangeEvents>(_selection, _weakRefToThis, &_selectionChangeToken); RETURN_IF_FAILED(hr);
		hr = AdviseSink<IPropertyChangeSink>(_project, _weakRefToThis, &_projectPropChangeToken); RETURN_IF_FAILED(hr);
		hr = AdviseSink<ID2DRenderEventsSink>(_renderer, _weakRefToThis, &_renderSink); RETURN_IF_FAILED(hr);
		hr = AdviseSink<IInvalidateSink>(_project, _weakRefToThis, &_projectInvalidateToken); RETURN_IF_FAILED(hr);

		com_ptr<IVlanSelection> vlanSel;
		hr = _pw->GetVlanSelection(&vlanSel); RETURN_IF_FAILED(hr);
		hr = AdviseSink<IVlanSelectionEvents>(vlanSel, _weakRefToThis, &_vlanSelectionToken); RETURN_IF_FAILED(hr);

		for (ULONG i = 0; i < _project->BridgeCount(); i++)
		{
			auto b = _project->BridgeAt(i);
			auto punk = wil::try_com_query_nothrow<IUnknown>(b);
			AdviseSinkToken token;
			hr = AdviseSink<IInvalidateSink>(punk, _weakRefToThis, &token); RETURN_IF_FAILED(hr);
			_bridgeWiresInvalidateTokens[punk] = std::move(token);
		}

		for (ULONG i = 0; i < _project->WireCount(); i++)
		{
			auto* w = _project->WireAt(i);
			auto punk = wil::try_com_query_nothrow<IUnknown>(w);
			AdviseSinkToken token;
			hr = AdviseSink<IInvalidateSink>(punk, _weakRefToThis, &token); RETURN_IF_FAILED(hr);
			_bridgeWiresInvalidateTokens[punk] = std::move(token);
		}

		hr = create_render_resources(_renderer->dc()); RETURN_IF_FAILED(hr);
		return S_OK;
	}

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<IEditWindow>(this, riid, ppvObject)
			|| TryQI<IPropertyChangeSink>(this, riid, ppvObject)
			|| TryQI<IInvalidateSink>(this, riid, ppvObject)
			|| TryQI<ID2DRenderEventsSink>(this, riid, ppvObject)
			|| TryQI<IVlanSelectionEvents>(this, riid, ppvObject)
			|| TryQI<IObjectCollectionChangeEvents>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IUnknown* AsUnknown() { return static_cast<IEditWindow*>(this); }

	virtual HWND hWnd() const override { return _hWnd.get(); }
	virtual ID2DRenderer* renderer() override final { return _renderer.get(); }
	virtual IZoomer* zoomer() override final { return _zoomer.get(); }

	#pragma region IVlanSelectionEvents
	virtual HRESULT STDMETHODCALLTYPE OnVlanSelectionChanging (DWORD dwOld) override
	{
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnVlanSelectionChanged (DWORD dwNew) override
	{
		::InvalidateRect(_hWnd.get(), 0, 0);
		return S_OK;
	}
	#pragma endregion

	#pragma region IPropertyChangeSink
	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (IUnknown* obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		if (dispID == dispidBridges)
		{
			_ASSERT(args->propertyType == PropertyType::Collection);
			if (args->collectionChangeArgs.changeType == CollectionChangeType::Remove)
			{
				// Removing bridge
				if (_htResult.first == _project->BridgeAt(args->collectionChangeArgs.setInsertRemoveArgs.index))
				{
					_htResult = { nullptr, 0 };
					::InvalidateRect(_hWnd.get(), 0, 0);
				}
			}
		}
		else if (dispID == dispidWires)
		{
			_ASSERT(args->propertyType == PropertyType::Collection);
			if (args->collectionChangeArgs.changeType == CollectionChangeType::Remove)
			{
				// Removing wire
				if (_htResult.first == _project->WireAt(args->collectionChangeArgs.setInsertRemoveArgs.index))
				{
					_htResult = { nullptr, 0 };
					::InvalidateRect(_hWnd.get(), 0, 0);
				}
			}
		}

		if (dispID == dispidBridges || dispID == dispidWires)
		{
			_ASSERT(args->propertyType == PropertyType::Collection);
			if (args->collectionChangeArgs.changeType == CollectionChangeType::Remove)
			{
				// Removing bridge or wire.
				IUnknown* child;
				if (dispID == dispidBridges)
					child = _project->BridgeAt(args->collectionChangeArgs.setInsertRemoveArgs.index);
				else
					child = _project->WireAt(args->collectionChangeArgs.setInsertRemoveArgs.index);
				auto it = _bridgeWiresInvalidateTokens.find(child);
				WI_ASSERT(it != _bridgeWiresInvalidateTokens.end());
				_bridgeWiresInvalidateTokens.erase(it);
			}
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (IUnknown* obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		HRESULT hr;

		if (dispID == dispidBridges || dispID == dispidWires)
		{
			_ASSERT(args->propertyType == PropertyType::Collection);
			if (args->collectionChangeArgs.changeType == CollectionChangeType::Insert)
			{
				// Inserted bridge or wire.
				IUnknown* child;
				if (dispID == dispidBridges)
					child = _project->BridgeAt(args->collectionChangeArgs.setInsertRemoveArgs.index);
				else
					child = _project->WireAt(args->collectionChangeArgs.setInsertRemoveArgs.index);
				auto it = _bridgeWiresInvalidateTokens.find(child);
				_ASSERT(it == _bridgeWiresInvalidateTokens.end());
				AdviseSinkToken token;
				hr = AdviseSink<IInvalidateSink>(child, _weakRefToThis, &token); RETURN_IF_FAILED(hr);
				_bridgeWiresInvalidateTokens[child] = std::move(token);
			}
		}

		return S_OK;
	}
	#pragma endregion

	#pragma region IInvalidateSink
	virtual HRESULT STDMETHODCALLTYPE OnInvalidate (const RECT* rectw) noexcept override
	{
		if (!rectw)
		{
			::InvalidateRect(_hWnd.get(), 0, 0);
		}
		else
		{
			D2D1_POINT_2F tl = _zoomer->pointw_to_pointd({ (float)rectw->left, (float)rectw->top });
			D2D1_POINT_2F br = _zoomer->pointw_to_pointd({ (float)rectw->right, (float)rectw->bottom });
			RECT rc = rectd_to_rectp({ tl.x, tl.y, br.x, br.y }, dpi(_hWnd.get()), 1);
			::InvalidateRect(_hWnd.get(), &rc, 0);
		}
		return S_OK;
	}
	#pragma endregion

	#pragma region IObjectCollectionChangeEvents
	virtual HRESULT OnCollectionChanging (IUnknown* sender, const struct ObjectCollectionChangeArgs* args) override
	{
		return S_OK;
	}

	virtual HRESULT OnCollectionChanged (IUnknown* sender, const struct ObjectCollectionChangeArgs* args) override
	{
		::InvalidateRect(_hWnd.get(), 0, 0);
		return S_OK;
	}
	#pragma endregion

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

	HRESULT RenderLegend (HWND hWnd, ID2D1DeviceContext* dc, D2D1_SIZE_F clientSize) const
	{
		HRESULT hr;

		LONG maxLineWidth = 0;
		LONG maxLineHeight = 0;
		std::vector<com_ptr<IDWriteTextLayout>> layouts;
		for (auto& info : LegendInfo)
		{
			com_ptr<IDWriteTextLayout> tl;
			hr = _renderer->dwrite_factory()->CreateTextLayout(info.text, (UINT32)wcslen(info.text),
				_legendFont.get(), 10000, 10000, &tl); RETURN_IF_FAILED(hr);

			DWRITE_TEXT_METRICS metrics;
			tl->GetMetrics (&metrics);

			if (metrics.width > maxLineWidth)
				maxLineWidth = (LONG)std::ceil(metrics.width);

			if (metrics.height > maxLineHeight)
				maxLineHeight = (LONG)std::ceil(metrics.height);

			layouts.push_back(std::move(tl));
		}

		float textX = clientSize.width - (5 + maxLineWidth + 5 + PortExteriorHeight + 5);
		float lineX = textX - 3;
		float bitmapX = clientSize.width - (5 + PortExteriorHeight + 5);
		LONG rowHeight = 2 + std::max (maxLineHeight, PortExteriorWidth);
		float y = clientSize.height - _countof(LegendInfo) * rowHeight;

		auto lineWidth = edge::pixel_width(hWnd);

		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode (D2D1_ANTIALIAS_MODE_ALIASED);
		auto tooltip_back_brush = _tcp->make_brush(dc, theme_color::tooltip_back, 0.8f);
		dc->FillRectangle (D2D1_RECT_F { lineX, y, clientSize.width, clientSize.height }, tooltip_back_brush);
		auto tooltip_fore_brush = _tcp->make_brush(dc, theme_color::tooltip_fore);
		dc->DrawLine ({ lineX, y }, { lineX, clientSize.height }, tooltip_fore_brush, lineWidth);
		dc->SetAntialiasMode (oldaa);

		Matrix3x2F oldtr;
		dc->GetTransform (&oldtr);

		for (size_t i = 0; i < _countof(LegendInfo); i++)
		{
			auto& info = LegendInfo[i];

			auto oldaa = dc->GetAntialiasMode();
			dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
			dc->DrawLine (Point2F (lineX, y), Point2F (clientSize.width, y), tooltip_fore_brush, lineWidth);
			dc->SetAntialiasMode(oldaa);

			dc->DrawTextLayout (Point2F (textX, y + 1), layouts[i], tooltip_fore_brush);

			// Rotate 270 degrees and then translate.
			Matrix3x2F tr (0, -1, 1, 0, bitmapX, y + rowHeight / 2);
			dc->SetTransform (tr * oldtr);
			RenderExteriorStpPort (dc, _drawing_resources, info.role, info.learning, info.forwarding, info.operEdge);
			dc->SetTransform (&oldtr);

			y += rowHeight;
		}

		return S_OK;
	}

	HRESULT RenderConfigIdList (ID2D1DeviceContext* dc, D2D1_SIZE_F clientSize, const std::set<STP_MST_CONFIG_ID>& configIds) const
	{
		HRESULT hr;

		size_t colorIndex = 0;
		float maxLineWidth = 0;
		float lineHeight = 0;
		std::vector<std::pair<TextLayoutWithMetrics, D2D1_COLOR_F>> lines;
		for (const STP_MST_CONFIG_ID& configId : configIds)
		{
			std::wstringstream ss;
			ss << configId.ConfigurationName << " -- " << (configId.RevisionLevelLow | (configId.RevisionLevelHigh << 8)) << " -- "
				<< std::uppercase << std::setfill(L'0') << std::hex
				<< std::setw(2) << (int)configId.ConfigurationDigest[0] << std::setw(2) << (int)configId.ConfigurationDigest[1] << ".."
				<< std::setw(2) << (int)configId.ConfigurationDigest[14] << std::setw(2) << (int)configId.ConfigurationDigest[15];
			TextLayoutWithMetrics tl;
			hr = CreateTextLayoutWithMetrics(_renderer->dwrite_factory(), _legendFont, ss.str().c_str(), -1, 0, tl); RETURN_IF_FAILED(hr);

			if (tl.metrics.width > maxLineWidth)
				maxLineWidth = tl.metrics.width;

			if (tl.metrics.height > lineHeight)
				lineHeight = tl.metrics.height;

			lines.push_back ({ std::move(tl), RegionColors[colorIndex] });
			colorIndex = (colorIndex + 1) % _countof(RegionColors);
		}

		float LeftRightPadding = 3;
		float UpDownPadding = 2;
		float coloredRectWidth = lineHeight * 2;

		TextLayoutWithMetrics title;
		hr = CreateTextLayoutWithMetrics(_renderer->dwrite_factory(), _legendFont, L"MST Regions:", -1, 0, title); RETURN_IF_FAILED(hr);

		float y = clientSize.height - lines.size() * (lineHeight + 2 * UpDownPadding) - title.metrics.height - 2 * UpDownPadding;

		auto fore_brush = _tcp->make_brush(dc, theme_color::foreground);

		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		float lineWidth = edge::pixel_width(_hWnd.get());
		float lineX = LeftRightPadding + coloredRectWidth + LeftRightPadding + maxLineWidth + LeftRightPadding;
		auto brush = _tcp->make_brush(dc, theme_color::tooltip_back, 0.8f);
		dc->FillRectangle ({ 0, y, lineX, clientSize.height }, brush);
		dc->DrawLine ({ 0, y }, { lineX, y }, fore_brush, lineWidth);
		dc->DrawLine ({ lineX, y }, { lineX, clientSize.height }, fore_brush, lineWidth);
		dc->SetAntialiasMode(oldaa);

		dc->DrawTextLayout ({ LeftRightPadding, y + UpDownPadding }, title, fore_brush);
		y += (title.metrics.height + 2 * UpDownPadding);

		for (auto& p : lines)
		{
			com_ptr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (p.second, &brush);
			D2D1_RECT_F rect = { LeftRightPadding, y + UpDownPadding, LeftRightPadding + coloredRectWidth, y + UpDownPadding + lineHeight };
			dc->FillRectangle (&rect, brush);
			D2D1_POINT_2F pt = { LeftRightPadding + coloredRectWidth + LeftRightPadding, y + UpDownPadding };
			dc->DrawTextLayout (pt, p.first, fore_brush);
			y += (lineHeight + 2 * UpDownPadding);
		}

		return S_OK;
	}

	virtual void RenderSnapRect (ID2D1DeviceContext* dc, POINT wLocation) const override final
	{
		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		auto cpd = _zoomer->pointw_to_pointd(point_to_pointf(wLocation));
		float sd = SnapDistance * dpi(_hWnd.get()) / 96;
		auto rect = RectF (cpd.x - sd, cpd.y - sd, cpd.x + sd, cpd.y + sd);
		dc->DrawRectangle (rect, _drawing_resources._brushHighlight, sd / 3);

		dc->SetAntialiasMode(oldaa);
	}

	virtual HRESULT STDMETHODCALLTYPE RenderHint (ID2D1DeviceContext* dc, D2D1_POINT_2F dLocation, const wchar_t* text,
		DWRITE_TEXT_ALIGNMENT ha, DWRITE_PARAGRAPH_ALIGNMENT va, bool smallFont) const noexcept override
	{
		HRESULT hr;

		float leftRightPadding = 3;
		float topBottomPadding = 1.5f;
		auto textFormat = smallFont ? _drawing_resources._smallTextFormat.get() : _drawing_resources._regularTextFormat.get();
		TextLayoutWithMetrics tl;
		hr = CreateTextLayoutWithMetrics(_drawing_resources._dWriteFactory, textFormat, text, -1, 0, tl); RETURN_IF_FAILED(hr);

		float pixelWidthDips = edge::pixel_width(_hWnd.get());
		float lineWidthDips = roundf(1.0f / pixelWidthDips) * pixelWidthDips;

		float left = dLocation.x - leftRightPadding;
		if (ha == DWRITE_TEXT_ALIGNMENT_CENTER)
			left -= tl.metrics.width / 2;
		else if (ha == DWRITE_TEXT_ALIGNMENT_TRAILING)
			left -= tl.metrics.width;

		float top = dLocation.y;
		if (va == DWRITE_PARAGRAPH_ALIGNMENT_FAR)
			top -= (topBottomPadding * 2 + tl.metrics.height + lineWidthDips * 2);
		else if (va == DWRITE_PARAGRAPH_ALIGNMENT_CENTER)
			top -= (topBottomPadding + tl.metrics.height + lineWidthDips);

		float right = left + 2 * leftRightPadding + tl.metrics.width;
		float bottom = top + 2 * topBottomPadding + tl.metrics.height;
		left   = roundf (left   / pixelWidthDips) * pixelWidthDips - lineWidthDips / 2;
		top    = roundf (top    / pixelWidthDips) * pixelWidthDips - lineWidthDips / 2;
		right  = roundf (right  / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;
		bottom = roundf (bottom / pixelWidthDips) * pixelWidthDips + lineWidthDips / 2;

		auto back_brush = _tcp->make_brush(dc, theme_color::tooltip_back);
		auto fore_brush = _tcp->make_brush(dc, theme_color::tooltip_fore);
		D2D1_ROUNDED_RECT rr = { { left, top, right, bottom }, 4, 4 };
		dc->FillRoundedRectangle (&rr, back_brush);
		dc->DrawRoundedRectangle (&rr, fore_brush, lineWidthDips);
		dc->DrawTextLayout ({ rr.rect.left + leftRightPadding, rr.rect.top + topBottomPadding }, tl, fore_brush);

		return S_OK;
	}

	void render_bridges (ID2D1DeviceContext* dc, DWORD vlan, const std::set<STP_MST_CONFIG_ID>& configIds) const
	{
		HRESULT hr;

		Matrix3x2F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform (_zoomer->zoom_transform() * oldtr);

		for (ULONG i = 0; i <_project->BridgeCount(); i++)
		{
			IBridge* bridge = _project->BridgeAt(i);
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

			hr = bridge->Render (dc, _drawing_resources, vlan, color); LOG_IF_FAILED(hr);
		}

		dc->SetTransform(oldtr);
	}

	void render_wires (ID2D1DeviceContext* dc, DWORD vlan, D2D1_SIZE_F clientSize ) const
	{
		Matrix3x2F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform (_zoomer->zoom_transform() * oldtr);

		for (ULONG i = 0; i < _project->WireCount(); i++)
		{
			auto* w = _project->WireAt(i);
			bool hasLoop;
			bool forwarding = _project->IsWireForwarding(w, vlan, &hasLoop);
			w->render (dc, _drawing_resources, forwarding, hasLoop);
		}

		dc->SetTransform(oldtr);

		// TODO: move this out of this function
		if (_project->BridgeCount() == 0)
		{
			RenderHint (dc, { clientSize.width / 2, clientSize.height / 2 }, L"No bridges created. Right-click to create some.", DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
		}
		else if (_project->BridgeCount() == 1)
		{
			RenderHint (dc, { clientSize.width / 2, clientSize.height / 2 }, L"Right-click to add more bridges.", DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_CENTER, false);
		}
		else
		{
			bool anyPortConnected = false;
			for (ULONG bi = 0; bi < _project->BridgeCount() && !anyPortConnected; bi++)
			{
				IBridge* b = _project->BridgeAt(bi);
				for (ULONG pi = 0; pi < b->PortCount() && !anyPortConnected; pi++)
				{
					IPort* p = b->PortAt(pi);
					anyPortConnected |= (_project->GetWireConnectedToPort(p).first != nullptr);
				}
			}

			if (!anyPortConnected)
			{
				IBridge* b = _project->BridgeAt(0);
				auto text = L"No port connected. You can connect\r\nports by drawing wires with the mouse.";
				auto wl = D2D1_POINT_2F { (float)b->left() + (float)b->width() / 2, (float)b->bottom() + PortExteriorHeight * 1.5f };
				auto dl = _zoomer->pointw_to_pointd(wl);
				RenderHint (dc, { dl.x, dl.y }, text, DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, false);
			}
		}
	}

	void render_hover (ID2D1DeviceContext* dc) const
	{
		if (auto p = wil::try_com_query_nothrow<IPort>(_htResult.first))
		{
			if (_htResult.second == IPort::HTCodeCP)
				RenderSnapRect (dc, p->GetCPLocation());
		}
		else if (auto w = wil::try_com_query_nothrow<IWire>(_htResult.first))
		{
			if (_htResult.second >= 0)
				RenderSnapRect (dc, w->point_coords(_htResult.second));
		}
	}

	HRESULT create_render_resources (ID2D1DeviceContext* dc)
	{
		HRESULT hr;
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::PaleGreen), &_drawing_resources._poweredFillBrush     ); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._unpoweredBrush       ); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._brushDiscardingPort  ); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gold),      &_drawing_resources._brushLearningPort    ); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green),     &_drawing_resources._brushForwarding      ); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray),      &_drawing_resources._brushNoForwardingWire); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Red),       &_drawing_resources._brushLoop            ); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Blue),      &_drawing_resources._brushTempWire        ); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::foreground), &_drawing_resources._brushWindowText); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::background), &_drawing_resources._brushWindow    ); RETURN_IF_FAILED(hr);
		hr = dc->CreateSolidColorBrush (_tcp->color_d2d(theme_color::selected_back_focused), &_drawing_resources._brushHighlight ); RETURN_IF_FAILED(hr);

		com_ptr<ID2D1Factory> factory;
		dc->GetFactory(&factory);

		D2D1_STROKE_STYLE_PROPERTIES ssprops = {};
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleSelectionRect); RETURN_IF_FAILED(hr);

		ssprops = { };
		ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleNoForwardingWire); RETURN_IF_FAILED(hr);

		ssprops = { };
		ssprops.startCap = D2D1_CAP_STYLE_ROUND;
		ssprops.endCap = D2D1_CAP_STYLE_ROUND;
		hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &_drawing_resources._strokeStyleForwardingWire); RETURN_IF_FAILED(hr);
		
		return S_OK;
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

	#pragma region ID2DRenderEventsSink
	virtual HRESULT STDMETHODCALLTYPE OnD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override
	{
		HRESULT hr;

		std::set<STP_MST_CONFIG_ID> configIds;
		for (ULONG i = 0; i < _project->BridgeCount(); i++)
		{
			auto stpb = _project->BridgeAt(i)->stp_bridge();
			if (STP_GetStpVersion(stpb) >= STP_VERSION_MSTP)
				configIds.insert (*STP_GetMstConfigId(stpb));
		}

		com_ptr<IVlanSelection> vlanSel;
		hr = _pw->GetVlanSelection(&vlanSel); RETURN_IF_FAILED(hr);
		DWORD vlan;
		hr = vlanSel->GetSelectedVlan(&vlan); RETURN_IF_FAILED(hr);

		auto dpi = edge::dpi(hWnd);
		RECT clientRectPixels;
		::GetClientRect(hWnd, &clientRectPixels);
		D2D1_SIZE_F clientSize = { clientRectPixels.right * 96.0f / dpi, clientRectPixels.bottom * 96.0f / dpi };

		dc->Clear(_tcp->color_d2d(theme_color::background));

		D2D1_MATRIX_3X2_F dpiTransform = { dpi / 96.0f, 0, 0, dpi / 96.0f, 0, 0 };
		dc->SetTransform(dpiTransform);

		RenderLegend(hWnd, dc, clientSize);

		render_bridges (dc, vlan, configIds);

		render_wires (dc, vlan, clientSize);

		for (IDispatch* o : *_selection)
		{
			auto so = wil::com_query_failfast<ISelectableObject>(o);
			so->render_selection(dc, _zoomer.get(), _drawing_resources);
		}

		if (!configIds.empty())
			RenderConfigIdList (dc, clientSize, configIds);

		if (_htResult.first)
			render_hover(dc);

		RenderHint (dc, { clientSize.width / 2, clientSize.height },
					L"Rotate mouse wheel for zooming, press wheel and drag for panning.",
					DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_FAR, true);

		if (_project->simulation_paused())
			RenderHint (dc, { clientSize.width / 2, 10 },
						L"Simulation is paused. Right-click to resume.",
						DWRITE_TEXT_ALIGNMENT_CENTER, DWRITE_PARAGRAPH_ALIGNMENT_NEAR, true);

		if (_state != nullptr)
			_state->render(dc);

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnBeforeD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnAfterD2DRender (HWND hWnd, ID2D1DeviceContext* dc) override { return S_OK; }

	virtual HRESULT STDMETHODCALLTYPE OnD2DDCReleasing (ID2D1DeviceContext* dc) override
	{
		release_render_resources(dc);		
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnD2DDCRecreated (ID2D1DeviceContext* dc) override
	{
		return create_render_resources(dc);
	}
	#pragma endregion

	static LRESULT CALLBACK WndProcStatic (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		if (auto w = reinterpret_cast<edit_window*>(GetWindowLongPtr (hWnd, GWLP_USERDATA)))
			return w->WndProc(hWnd, uMsg, wParam, lParam);
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}

	LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if ((msg == WM_LBUTTONDOWN) || (msg == WM_RBUTTONDOWN))
		{
			auto button = (msg == WM_LBUTTONDOWN) ? mouse_button::left : mouse_button::right;
			auto handled = OnMouseButtonDown (hwnd, button, (UINT)wparam, { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			if (handled)
				return 0;
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		if ((msg == WM_LBUTTONUP) || (msg == WM_RBUTTONUP))
		{
			auto button = (msg == WM_LBUTTONUP) ? mouse_button::left : mouse_button::right;
			auto handled = OnMouseButtonUp (button, (UINT)wparam, { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			if (handled)
				return 0;
			return DefWindowProc(hwnd, msg, wparam, lparam);
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
						::SetCursor(cursor_at(hwnd, pt));
						return TRUE;
					}
				}
			}

			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		if (msg == WM_MOUSEMOVE)
		{
			OnMouseMove (hwnd, (UINT)wparam, { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) });
			return DefWindowProc(hwnd, msg, wparam, lparam);
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
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		if ((msg == WM_KEYUP) || (msg == WM_SYSKEYUP))
		{
			auto handled = process_key_or_syskey_up ((UINT) wparam, get_modifier_keys());
			if (handled)
				return 0;
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		if (msg == WM_COMMAND)
		{
			if (wparam == ID_NEW_BRIDGE)
			{
				EnterState (create_state_create_bridge(make_edit_state_deps()));
				return 0;
			}
			else if ((wparam == ID_BRIDGE_ENABLE_STP) || (wparam == ID_BRIDGE_DISABLE_STP))
			{
				bool enable = (wparam == ID_BRIDGE_ENABLE_STP);
				for (IDispatch* o : *_selection)
				{
					if (auto b = wil::try_com_query_nothrow<IBridge>(o))
						b->set_stp_enabled(enable);
				}

				_project->SetChangedFlag(true);
				return 0;
			}
			else if (wparam == ID_PAUSE_SIMULATION)
			{
				_project->pause_simulation();
				return 0;
			}
			else if (wparam == ID_RESUME_SIMULATION)
			{
				_project->resume_simulation();
				return 0;
			}

			return DefWindowProc(hwnd, msg, wparam, lparam);
		}

		return DefWindowProc(hwnd, msg, wparam, lparam);
	}

	virtual IPort* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const override final
	{
		for (ULONG bi = _project->BridgeCount() - 1; bi != -1; bi--)
		{
			IBridge* b = _project->BridgeAt(bi);
			for (ULONG pi = 0; pi < b->PortCount(); pi++)
			{
				IPort* port = b->PortAt(pi);
				if (auto htcode = port->hit_test(_zoomer->zoom_transform(), dLocation, tolerance))
				{
					if (htcode == IPort::HTCodeCP)
						return port;
					else
						return nullptr;
				}
			}
		}

		return nullptr;
	}

	ht_result hit_test_objects (D2D1_POINT_2F pd, float tolerance) const
	{
		for (ULONG i = _project->WireCount(); i-- > 0; )
		{
			auto* w = _project->WireAt(i);
			auto htcode = w->hit_test (_zoomer->zoom_transform(), pd, tolerance);
			if (htcode)
				return { w, htcode };
		}

		for (ULONG bi = _project->BridgeCount() - 1; bi != -1; bi--)
		{
			IBridge* b = _project->BridgeAt(bi);
			for (ULONG pi = 0; pi < b->PortCount(); pi++)
			{
				IPort* p = b->PortAt(pi);
				if (uint8_t htcode = p->hit_test(_zoomer->zoom_transform(), pd, tolerance))
					return { p, htcode };
			}

			if (uint8_t htcode = b->hit_test(_zoomer->zoom_transform(), pd, tolerance))
				return { b, htcode };
		}

		return { };
	}

	void delete_selection()
	{
		static constexpr auto is_port = [](IDispatch* o) { return wil::try_com_query_nothrow<IPort>(o) != nullptr; };
		if (_selection->any(is_port))
		{
			MessageBoxA (_hWnd.get(), "Ports cannot be deleted.", nullptr, 0);
			return;
		}

		std::set<IBridge*> bridgesToRemove;
		std::set<IWire*> wiresToRemove;
		std::unordered_map<IWire*, std::vector<size_t>> pointsToDisconnect;

		for (IDispatch* o : *_selection)
		{
			if (auto w = wil::try_com_query_nothrow<IWire>(o); w != nullptr)
				wiresToRemove.insert(w);
			else if (auto b = wil::try_com_query_nothrow<IBridge>(o); b != nullptr)
				bridgesToRemove.insert(b);
			else
				_ASSERT(false);
		}

		for (ULONG i = 0; i < _project->WireCount(); i++)
		{
			auto* w = _project->WireAt(i);
			if (wiresToRemove.find(w) != wiresToRemove.end())
				continue;

			for (size_t pi = 0; pi < w->points().size(); pi++)
			{
				if (!std::holds_alternative<connected_wire_end>(w->points()[pi]))
					continue;

				auto port = std::get<connected_wire_end>(w->points()[pi]);
				if (bridgesToRemove.find(port->bridge()) == bridgesToRemove.end())
					continue;

				// point is connected to bridge being removed.
				pointsToDisconnect[w].push_back(pi);
			}
		}

		for (auto it = pointsToDisconnect.begin(); it != pointsToDisconnect.end(); )
		{
			IWire* wire = it->first;
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
			{
				ULONG i = 0;
				while (i < _project->WireCount() && _project->WireAt(i) != w)
					i++;
				FAIL_FAST_IF(i == _project->WireCount());
				_project->RemoveWire(i);
			}

			for (auto b : bridgesToRemove)
			{
				ULONG i = 0;
				while (i < _project->BridgeCount() && _project->BridgeAt(i) != b)
					i++;
				FAIL_FAST_IF(i == _project->BridgeCount());
				_project->RemoveBridge(i);
			}
			_project->SetChangedFlag(true);
		}
	}

	handled process_key_or_syskey_down (uint32_t vkey, UINT mks)
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

	handled process_key_or_syskey_up (uint32_t vkey, UINT mks)
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

	static HRESULT SameType (IDispatch* a, IDispatch* b)
	{
		HRESULT hr;
		
		com_ptr<ITypeInfo> tia;
		hr = a->GetTypeInfo(0, LANG_INVARIANT, &tia); RETURN_IF_FAILED(hr);
		TYPEATTR* attra;
		hr = tia->GetTypeAttr(&attra); RETURN_IF_FAILED(hr);
		auto releasea = wil::scope_exit([&tia,attra] { tia->ReleaseTypeAttr(attra); });
		
		com_ptr<ITypeInfo> tib;
		hr = b->GetTypeInfo(0, LANG_INVARIANT, &tib); RETURN_IF_FAILED(hr);
		TYPEATTR* attrb;
		hr = tib->GetTypeAttr(&attrb); RETURN_IF_FAILED(hr);
		auto releaseb = wil::scope_exit([&tib,attrb] { tib->ReleaseTypeAttr(attrb); });

		return (attra->guid == attrb->guid) ? S_OK : S_FALSE;
	}

	handled OnMouseButtonDown (HWND hwnd, mouse_button button, UINT mks, POINT pp)
	{
		uint32_t dpi = edge::dpi(hwnd);
		auto pd = edge::pointp_to_pointd(pp, dpi);

		::SetFocus(hwnd);
		if (::GetFocus() != hwnd)
			// Some validation code (maybe in the Properties Window) must have failed and taken focus back.
			return handled(true);

		mouse_location ml;
		ml.pt = pp;
		ml.d = pointp_to_pointd(pp, dpi);
		ml.w = pointf_to_point(_zoomer->pointd_to_pointw(ml.d));

		if (_state)
		{
			auto handled = _state->OnMouseButtonDown (button, mks, ml);
			if (_state->completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return handled;
		}

		float sd = SnapDistance * dpi / 96;
		auto ht = hit_test_objects (ml.d, sd);
		if (!ht.first)
			_selection->Clear();
		else
		{
			auto htf = wil::try_com_query_nothrow<IDispatch>(ht.first);
			if (mks & MK_CONTROL)
			{
				if (_selection->contains(htf))
					_selection->Remove(htf);
				else if (!_selection->empty() && SameType(_selection->front(), htf) == S_OK)
					_selection->Add(htf);
				else
					_selection->Select(htf);
			}
			else
			{
				if (!_selection->contains(htf))
					_selection->Select(htf);
			}
		}

		if (button == mouse_button::left)
		{
			if (!ht.first)
			{
				// TODO: area selection
				//stateForMoveThreshold =
				return handled(true);
			}
			else
			{
				std::unique_ptr<edit_state> stateMoveThreshold;
				std::unique_ptr<edit_state> stateButtonUp;

				if (wil::try_com_query_nothrow<IBridge>(ht.first))
				{
					if (button == mouse_button::left)
						stateMoveThreshold = create_state_move_bridges (make_edit_state_deps());
				}
				else if (auto port = wil::try_com_query_nothrow<IPort>(ht.first))
				{
					if (ht.second == IPort::HTCodeInnerOuter)
					{
						if ((button == mouse_button::left) 
							&& (_selection->size() == 1) 
							&& wil::try_com_query_nothrow<IPort>(_selection->front()))
						{
							stateMoveThreshold = create_state_move_port (make_edit_state_deps());
						}
					}
					else if (ht.second == IPort::HTCodeCP)
					{
						auto alreadyConnectedWire = _project->GetWireConnectedToPort(port);
						if (alreadyConnectedWire.first == nullptr)
						{
							stateMoveThreshold = create_state_create_wire(make_edit_state_deps());
							stateButtonUp = create_state_create_wire(make_edit_state_deps());
						}
					}
				}
				else if (auto w = wil::try_com_query_nothrow<IWire>(ht.first))
				{
					if (ht.second >= 0)
					{
						stateMoveThreshold = CreateStateMoveWirePoint(make_edit_state_deps(), w, ht.second);
						stateButtonUp = CreateStateMoveWirePoint (make_edit_state_deps(), w, ht.second);
					}
				}

				auto state = CreateStateBeginningDrag(make_edit_state_deps(), ht.first, button, mks, ml, ::GetCursor(), std::move(stateMoveThreshold), std::move(stateButtonUp));
				EnterState(std::move(state));
				return handled(true);
			}
		}

		return handled(false);
	}

	handled OnMouseButtonUp (mouse_button button, UINT mks, POINT pp)
	{
		uint32_t dpi = edge::dpi(_hWnd.get());
		auto pd = edge::pointp_to_pointd(pp, dpi);
		auto wLocation = _zoomer->pointd_to_pointw(pd);

		if (_state != nullptr)
		{
			auto handled = _state->OnMouseButtonUp (button, mks, { pp, pd, pointf_to_point(wLocation) });
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
		_htResult = { nullptr, 0 };
	}

	HCURSOR cursor_at (HWND hwnd, POINT pp) const
	{
		uint32_t dpi = edge::dpi(hwnd);
		auto pd = edge::pointp_to_pointd(pp, dpi);
		auto wLocation = _zoomer->pointd_to_pointw(pd);

		if (_state != nullptr)
			return _state->cursor();

		float sd = SnapDistance * dpi / 96;
		auto ht = hit_test_objects (pd, sd);

		LPCWSTR idc = IDC_ARROW;
		if (wil::try_com_copy_nothrow<IPort>(ht.first))
		{
			if (ht.second == IPort::HTCodeCP)
				idc = IDC_CROSS;
		}
		else if (wil::try_com_copy_nothrow<IWire>(ht.first))
		{
			if (ht.second >= 0)
				// wire point
				idc = IDC_CROSS;
			else
				// wire line
				idc = IDC_ARROW;
		}

		return LoadCursor(nullptr, idc);
	}

	void OnMouseMove (HWND hwnd, UINT mks, POINT pp)
	{
		uint32_t dpi = edge::dpi(hwnd);
		auto pd = pointp_to_pointd(pp, dpi);
		auto pw = _zoomer->pointd_to_pointw(pd);

		if (_state != nullptr)
		{
			_state->OnMouseMove ({ pp, pd, pointf_to_point(pw) });
			if (_state->completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			}

			return;
		}

		float sd = SnapDistance * dpi / 96;
		auto ht = hit_test_objects (pd, sd);
		if (_htResult != ht)
		{
			_htResult = ht;
			::InvalidateRect(_hWnd.get(), 0, 0);
		}
	}

	void ProcessWmContextMenu (HWND hwnd, POINT pt)
	{
		//D2D1_POINT_2F dipLocation = _window->pointp_to_pointd(pt);
		//_elementsAtContextMenuLocation.clear();
		//GetElementsAt(_project->GetInnerRootElement(), { dipLocation.x, dipLocation.y }, _elementsAtContextMenuLocation);

		HMENU menu = nullptr;
		if (_selection->empty())
		{
			menu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_EMPTY_SPACE));
			::EnableMenuItem (menu, ID_PAUSE_SIMULATION, _project->simulation_paused() ? MF_DISABLED : MF_ENABLED);
			::EnableMenuItem (menu, ID_RESUME_SIMULATION, _project->simulation_paused() ? MF_ENABLED : MF_DISABLED);
		}
		else if (wil::try_com_query_nothrow<IBridge>(_selection->front()))
		{
			menu = LoadMenu (GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_CONTEXT_MENU_BRIDGE));
			static const auto is_bridge_with_stp_enabled = [](IDispatch* o) {
				if (auto b = wil::try_com_query_nothrow<IBridge>(o); b && STP_IsBridgeStarted(b->stp_bridge()))
					return true;
				return false;
			};
			static const auto is_bridge_with_stp_disabled = [](IDispatch* o) {
				if (auto b = wil::try_com_query_nothrow<IBridge>(o); b && !STP_IsBridgeStarted(b->stp_bridge()))
					return true;
				return false;
			};
			bool any_enabled = _selection->any(is_bridge_with_stp_enabled);
			bool any_disabled = _selection->any(is_bridge_with_stp_disabled);
			::EnableMenuItem (menu, ID_BRIDGE_DISABLE_STP, any_enabled ? MF_ENABLED : MF_DISABLED);
			::EnableMenuItem (menu, ID_BRIDGE_ENABLE_STP, any_disabled ? MF_ENABLED : MF_DISABLED);
		}

		if (menu)
			TrackPopupMenuEx (GetSubMenu(menu, 0), 0, pt.x, pt.y, hwnd, nullptr);
	}

	virtual const struct drawing_resources& drawing_resources() const override final { return _drawing_resources; }

	virtual void zoom_all() override
	{
		if (_project->BridgeCount() || _project->WireCount())
		{
			auto r = _project->BridgeCount() ? _project->BridgeAt(0)->extentf() : _project->WireAt(0)->extentf();
			for (ULONG bi = 0; bi < _project->BridgeCount(); bi++)
				r = union_rects(r, _project->BridgeAt(bi)->extentf());
			for (ULONG i = 0; i < _project->WireCount(); i++)
				r = union_rects(r, _project->WireAt(i)->extentf());

			_zoomer->zoom_to (r, 20, 0, 1.5f, false);
		}
	}

	edit_state_deps make_edit_state_deps()
	{
		com_ptr<IVlanSelection> vlanSel;
		auto hr = _pw->GetVlanSelection(&vlanSel); LOG_IF_FAILED(hr);
		return edit_state_deps { _app, _pw, this, _project, vlanSel, _selection };
	}
};

HRESULT edit_window_factory (const EditWindowCreateParams& create_params, IEditWindow** ppEditWindow)
{
	auto p = com_ptr (new (std::nothrow) edit_window()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(create_params); RETURN_IF_FAILED(hr);
	*ppEditWindow = p.detach();
	return S_OK;
};
