
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "edge_d2d.h"

using namespace D2D1;
using namespace edge;

#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "D3D11.lib")
#pragma comment (lib, "Dxgi.lib")

class d2d_renderer : public ID2DRenderer, IConnectionPointContainer
{
	ULONG _refCount = 0;
	HWND _hWnd;
	bool _painting = false;
	bool _forceFullPresentation;
	com_ptr<IDWriteFactory> _dwrite_factory;
	com_ptr<ID2D1Factory1> _d2d_factory;
	com_ptr<ID3D11Device1> _d3d_device;
	com_ptr<ID3D11DeviceContext> _d3d_dc;
	com_ptr<IDXGIDevice2> _dxgi_device;
	com_ptr<IDXGIAdapter> _dxgi_adapter;
	com_ptr<IDXGIFactory2> _dxgi_factory;
	com_ptr<IDXGISwapChain1> _swap_chain;
	com_ptr<ID2D1DeviceContext> _d2d_dc;
	bool _caret_on = false;
	bool _caret_blink_on = false;
	bool _subclassed = false;
	std::pair<D2D1_RECT_F, D2D1_MATRIX_3X2_F> _caret_bounds;
	D2D1_COLOR_F _caret_color;

	struct render_perf_info
	{
		LARGE_INTEGER start_time;
		float duration;
	};

	LARGE_INTEGER _performance_counter_frequency;
	std::deque<render_perf_info> perf_info_queue;

	debug_flag _debug_flags = (debug_flag)0;
	com_ptr<IDWriteTextFormat> _debug_text_format;
	com_ptr<ConnectionPointImpl<ID2DRenderEventsSink>> _renderEventsCP;

public:
	HRESULT InitInstance (HWND hWnd, ID3D11DeviceContext* d3d_dc, IDWriteFactory* dwrite_factory, ID2D1Factory1* d2d_factory)
	{
		HRESULT hr;

		_hWnd = hWnd;
		_d3d_dc = d3d_dc;
		_dwrite_factory = dwrite_factory;
		_d2d_factory = d2d_factory;

		hr = MakeConnectionPoint<ID2DRenderEventsSink>(this, &_renderEventsCP); RETURN_IF_FAILED(hr);

		com_ptr<ID3D11Device> device;
		d3d_dc->GetDevice(&device);
		hr = device->QueryInterface(IID_PPV_ARGS(&_d3d_device)); RETURN_IF_FAILED(hr);

		hr = device->QueryInterface(IID_PPV_ARGS(&_dxgi_device)); RETURN_IF_FAILED(hr);

		hr = _dxgi_device->GetAdapter(&_dxgi_adapter); RETURN_IF_FAILED(hr);

		hr = _dxgi_adapter->GetParent(IID_PPV_ARGS(&_dxgi_factory)); RETURN_IF_FAILED(hr);

		RECT clientRect;
		::GetClientRect(_hWnd, &clientRect);
		DXGI_SWAP_CHAIN_DESC1 desc;
		desc.Width = std::max (8l, clientRect.right);
		desc.Height = std::max (8l, clientRect.bottom);
		desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
		desc.Stereo = FALSE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
		desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
		desc.Flags = 0;
		hr = _dxgi_factory->CreateSwapChainForHwnd (_d3d_device, _hWnd, &desc, nullptr, nullptr, &_swap_chain); RETURN_IF_FAILED(hr);
		_forceFullPresentation = true;

		create_d2d_dc();

		QueryPerformanceFrequency(&_performance_counter_frequency);

		hr = dwrite_factory->CreateTextFormat (L"Courier New", nullptr, DWRITE_FONT_WEIGHT_BOLD,
											   DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, 
											   11, L"en-US", &_debug_text_format); RETURN_IF_FAILED(hr);

		_subclassed = SetWindowSubclass (_hWnd, SubclassProc, 0, reinterpret_cast<DWORD_PTR>(this));
		_ASSERT(_subclassed);
		
		return S_OK;
	}

	~d2d_renderer()
	{
		if (_subclassed)
			RemoveWindowSubclass (_hWnd, SubclassProc, 0);
		
		release_d2d_dc();
	}

	IUnknown* AsIUnknown() { return static_cast<ID2DRenderer*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<ID2DRenderer>(this, riid, ppvObject)
			|| TryQI<IUnknown>(AsIUnknown(), riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(ID2DRenderEventsSink))
			return wil::com_query_to_nothrow(_renderEventsCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	void create_d2d_dc()
	{
		HRESULT hr;

		com_ptr<IDXGISurface2> dxgiSurface;
		hr = _swap_chain->GetBuffer (0, IID_PPV_ARGS(&dxgiSurface)); _ASSERT(SUCCEEDED(hr));

		static const D2D1_RENDER_TARGET_PROPERTIES props = {
			.type = D2D1_RENDER_TARGET_TYPE_HARDWARE,
			.pixelFormat = { DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE },
			.dpiX = 96,
			.dpiY = 96,
			.usage = D2D1_RENDER_TARGET_USAGE_NONE,
			.minLevel = D2D1_FEATURE_LEVEL_DEFAULT
		};
		com_ptr<ID2D1RenderTarget> rt;
		hr = _d2d_factory->CreateDxgiSurfaceRenderTarget (dxgiSurface, &props, &rt); _ASSERT(SUCCEEDED(hr));

		rt->QueryInterface(&_d2d_dc); _ASSERT(SUCCEEDED(hr));

		_d2d_dc->SetTextAntialiasMode (D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
	}

	void release_d2d_dc()
	{
		_ASSERT (_d2d_dc);
		_d2d_dc = nullptr;
	}

	void render_no_handlers(ID2D1DeviceContext* dc) const
	{
		dc->Clear({ 0, 0, 0, 1 });

		dc->SetTransform(edge::dpi_transform(_hWnd));

		com_ptr<ID2D1SolidColorBrush> brush;
		static constexpr D2D1_COLOR_F red = { 1, 0, 0, 1 };
		dc->CreateSolidColorBrush (&red, nullptr, &brush);

		auto pw = edge::pixel_width(_hWnd);

		auto rect = inflate(client_rect(_hWnd), -pw);
		dc->DrawRectangle (&rect, brush, 2 * pw);
		dc->DrawLine ({ rect.left, rect.top }, { rect.right, rect.bottom }, brush, 2 * pw);
		dc->DrawLine ({ rect.left, rect.bottom }, { rect.right, rect.top }, brush, 2 * pw);
	}

	void process_wm_size (SIZE client_size_pixels, D2D1_SIZE_F client_size_dips)
	{
		if (_swap_chain)
		{
			// Direct2D extends this to 8x8 and gives a warning. Let's extend it ourselves to avoid getting the warning.
			UINT width = std::max (8u, (UINT)client_size_pixels.cx);
			UINT height = std::max (8u, (UINT)client_size_pixels.cy);
			DXGI_SWAP_CHAIN_DESC1 desc1;
			_swap_chain->GetDesc1(&desc1);
			if ((desc1.Width != width) || (desc1.Height != height))
			{
				_renderEventsCP->Notify([this](ID2DRenderEventsSink* sink) { return sink->OnD2DDCReleasing(_d2d_dc); });
				release_d2d_dc();
				auto hr = _swap_chain->ResizeBuffers (0, width, height, DXGI_FORMAT_UNKNOWN, 0); _ASSERT(SUCCEEDED(hr));
				create_d2d_dc();
				_renderEventsCP->Notify([this](ID2DRenderEventsSink* sink) { return sink->OnD2DDCRecreated(_d2d_dc); });
			}
		}
	}

	static LRESULT CALLBACK SubclassProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
	{
		if (uMsg == WM_SIZE)
		{
			SIZE client_size_pixels = { LOWORD(lParam), HIWORD(lParam) };
			uint32_t dpi = edge::dpi(hWnd);
			D2D1_SIZE_F client_size_dips = { client_size_pixels.cx * 96.0f / dpi, client_size_pixels.cy * 96.0f / dpi };
			reinterpret_cast<d2d_renderer*>(dwRefData)->process_wm_size(client_size_pixels, client_size_dips);
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_ERASEBKGND)
			return 0; // 0 means the window remains marked for erasing, so the fErase member of the PAINTSTRUCT structure will be TRUE.

		if (uMsg == WM_PAINT)
		{
			reinterpret_cast<d2d_renderer*>(dwRefData)->ProcessWmPaint (hWnd);
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_SETFOCUS)
		{
			reinterpret_cast<d2d_renderer*>(dwRefData)->process_wm_set_focus();
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_KILLFOCUS)
		{
			reinterpret_cast<d2d_renderer*>(dwRefData)->process_wm_kill_focus();
			return DefSubclassProc (hWnd, uMsg, wParam, lParam);
		}

		return DefSubclassProc (hWnd, uMsg, wParam, lParam);
	}

	void ProcessWmPaint (HWND hwnd)
	{
		HRESULT hr;

		// Call this before calculating the update rects, to allow listeners to invalidate stuff before rendering.
		_renderEventsCP->Notify([this] (ID2DRenderEventsSink* sink) { return sink->OnBeforeD2DRender(_hWnd, _d2d_dc); });

		LARGE_INTEGER start_time;
		BOOL bRes = QueryPerformanceCounter(&start_time); _ASSERT(bRes);

		D2D1_RECT_F frameDurationAndFpsRect;
		auto _clientSizeDips = edge::client_size(hwnd);
		frameDurationAndFpsRect.left   = round (_clientSizeDips.width - 75) + 0.5f;
		frameDurationAndFpsRect.top    = round (_clientSizeDips.height - 28) + 0.5f;
		frameDurationAndFpsRect.right  = _clientSizeDips.width - 0.5f;
		frameDurationAndFpsRect.bottom = _clientSizeDips.height - 0.5f;
		if ((int) _debug_flags & (int) debug_flag::render_frame_durations_and_fps)
			edge::invalidate(frameDurationAndFpsRect, hwnd);
		/*
		#pragma region Calculate _updateRects
		{
			::GetUpdateRgn(hwnd(), _updateRegion, FALSE);

			DWORD requiredLength = GetRegionData(_updateRegion, 0, NULL);
			assert(requiredLength > 0);

			if (requiredLength > _updateRegionDataLength)
			{
				auto newRegionData = (RGNDATA*)realloc(_updateRegionData, requiredLength);
				assert(newRegionData != NULL);

				_updateRegionData = newRegionData;
				_updateRegionDataLength = requiredLength;
			}

			requiredLength = GetRegionData(_updateRegion, _updateRegionDataLength, _updateRegionData);
			assert(requiredLength > 0);

			DWORD numberOfRects = (requiredLength - sizeof(RGNDATAHEADER)) / sizeof(RECT);
			RECT* rects = (RECT*)_updateRegionData->Buffer;

			// It seems that Windows creates an update region with a huge number of rectangles (100-1000).
			// This number can be greatly reduced by combining adjacent rectangles. Let's do this.
			// We take advantage of the fact that the update region contains many rectangles
			// arranged like in the figure below, ordered first by "top", and we create a list of fewer rectangles
			// covering the same areas.
			//      ------------
			//      |          |
			//      +----------+
			//      |          |
			//          ...
			//      |          |
			//      ------------

			_updateRects.clear();

			DWORD combinedCount = 0;
			for (DWORD i = 0; i < numberOfRects; i++)
			{
				RECT& ri = rects[i];

				if (ri.left == ri.right)
				{
					// This is a rect that we already combined with another one;
					// we have set its "right" to be equal to its "left" to know that it's to be ignored.
					continue;
				}

				// Let's look for another rect that lies just below this one.
				for (DWORD j = i + 1; j < numberOfRects; j++)
				{
					RECT& rj = rects[j];
					if ((ri.bottom == rj.top) && (ri.left == rj.left) && (ri.right == rj.right))
					{
						// So we have two rectangles with the same X and same Width, on top of each other.
						// Enlarge the first, mark the second for ignoring, and keep looking.
						ri.bottom = rj.bottom;
						rj.right = rj.left;
						combinedCount++;
					}
					else if (ri.bottom < rj.top)
					{
						// We've gone too far down on the Y axis, no point continuing.
						break;
					}
				}

				// OK, ri can no longer be combined. Let's add it to our list.
				_updateRects.push_back(ri);
			}
		}
		#pragma endregion
		bool updateEntireClientArea = ((_updateRects.size() == 1) && (_updateRects[0] == _clientRect));
		*/
		// -------------------------------------------------
		// draw the stuff

		// Problem: If an assertion fails in code called from this function, the C++ runtime will try to display
		// the assertion message box. It seems that Windows, while processing WM_PAINT, displays message boxes
		// only if the application has called BeginPaint; if the application has not called BeginPaint, Windows
		// will not display the message box, will make sounds when clicking on the application window, and will
		// wait for the user to press Alt before finally displaying it (go figure!)

		PAINTSTRUCT ps;
		::BeginPaint(_hWnd, &ps); // this will also hide the caret, if shown.

		_painting = true;

		_d2d_dc->BeginDraw();
		_d2d_dc->SetTransform(IdentityMatrix());
		/*
		if (!updateEntireClientArea)
		{
			#pragma region Create D2D geometry from _updateRects and push clip layer
			hr = _d2d_factory->CreatePathGeometry(&_updateGeometry); _ASSERT(hr);

			ComPtr<ID2D1GeometrySink> sink;
			hr = _updateGeometry->Open(&sink); _ASSERT(hr);

			for (const auto& pixelRect : _updateRects)
			{
				D2D1_RECT_F rect =
				{
					pixelRect.left   * 96.0f / dpiX,
					pixelRect.top    * 96.0f / dpiY,
					pixelRect.right  * 96.0f / dpiX,
					pixelRect.bottom * 96.0f / dpiY
				};

				sink->BeginFigure(Point2F(rect.left, rect.top), D2D1_FIGURE_BEGIN_FILLED);
				sink->AddLine(Point2F(rect.right, rect.top));
				sink->AddLine(Point2F(rect.right, rect.bottom));
				sink->AddLine(Point2F(rect.left, rect.bottom));
				sink->EndFigure(D2D1_FIGURE_END_CLOSED);
			}

			hr = sink->Close(); _ASSERT(hr);

			if (!((int)_debug_flags & (int)debug_flag::full_clear))
			{
				D2D1_LAYER_PARAMETERS1 layerParams = {};
				layerParams.contentBounds = InfiniteRect();
				layerParams.geometricMask = _updateGeometry;
				layerParams.maskAntialiasMode = D2D1_ANTIALIAS_MODE_ALIASED;
				layerParams.maskTransform = IdentityMatrix();
				layerParams.opacity = 1.0f;
				layerParams.opacityBrush = nullptr;
				layerParams.layerOptions = D2D1_LAYER_OPTIONS1_IGNORE_ALPHA | D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
				_d2d_dc->PushLayer(&layerParams, nullptr);
				// Note AG: without D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND Direct2D calls ClearView,
				// which some graphic drivers implement in software, which is extremely slow. (Intel integrated for example.)
			}
			#pragma endregion
		}
		*/
		if (!_renderEventsCP->empty())
			_renderEventsCP->Notify([hwnd, dc=_d2d_dc.get()](ID2DRenderEventsSink* sink) { return sink->OnD2DRender(hwnd, dc); });
		else
			this->render_no_handlers(_d2d_dc);
		/*
		_updateGeometry = nullptr;

		if ((int)_debug_flags & (int)debug_flag::render_update_rects)
		{
			ComPtr<ID2D1SolidColorBrush> debugBrush;
			_d2d_dc->CreateSolidColorBrush(ColorF(ColorF::Red), &debugBrush);

			for (auto& rect : _updateRects)
				_d2d_dc->DrawRectangle(RectF(rect.left + 0.5f, rect.top + 0.5f, rect.right - 0.5f, rect.bottom - 0.5f), debugBrush);

			debugBrush->SetColor({ (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, 0.25f });

			for (auto& rect : _updateRects)
				_d2d_dc->FillRectangle(RectF(rect.left + 0.5f, rect.top + 0.5f, rect.right - 0.5f, rect.bottom - 0.5f), debugBrush);
		}
		*/
		if ((int)_debug_flags & (int)debug_flag::render_frame_durations_and_fps)
		{
			com_ptr<ID2D1SolidColorBrush> backBrush;
			_d2d_dc->CreateSolidColorBrush (ColorF::ColorF (ColorF::Yellow, 0.5f), &backBrush);
			_d2d_dc->FillRectangle(frameDurationAndFpsRect, backBrush);

			com_ptr<ID2D1SolidColorBrush> foreBrush;
			_d2d_dc->CreateSolidColorBrush (ColorF::ColorF (ColorF::Black), &foreBrush);

			wil::unique_process_heap_string ss;
			wil::str_printf_nothrow(ss, L"%4u FPS\r\n%3u ms", (int)round(fps()), (int)round(average_render_duration()));
			TextLayoutWithMetrics tl;
			hr = CreateTextLayoutWithMetrics(_dwrite_factory, _debug_text_format, ss.get(), -1, 0, tl); _ASSERT(SUCCEEDED(hr));

			D2D1_POINT_2F origin;
			origin.x = frameDurationAndFpsRect.right - 4 - tl.metrics.width;
			origin.y = (frameDurationAndFpsRect.top + frameDurationAndFpsRect.bottom) / 2 - tl.metrics.height / 2;
			_d2d_dc->DrawTextLayout (origin, tl, foreBrush);
		}
		/*
		if (!updateEntireClientArea
			&& !((int)_debug_flags & (int)debug_flag::full_clear))
		{
			_d2d_dc->PopLayer();
		}
		*/

		if (_caret_on && (::GetFocus() == _hWnd) && _caret_blink_on)
		{
			uint32_t dpi = edge::dpi(hwnd);
			_d2d_dc->SetTransform(edge::dpi_transform(dpi) * _caret_bounds.second);
			com_ptr<ID2D1SolidColorBrush> b;
			_d2d_dc->CreateSolidColorBrush(_caret_color, &b);
			_d2d_dc->FillRectangle (&_caret_bounds.first, b);
		}

		hr = _d2d_dc->EndDraw(); _ASSERT(SUCCEEDED(hr));

		DXGI_PRESENT_PARAMETERS pp = {};
		hr = _swap_chain->Present1(0, 0, &pp); _ASSERT(SUCCEEDED(hr));

		::EndPaint(_hWnd, &ps); // this will show the caret in case BeginPaint above hid it.

		#pragma region Calculate performance data.
		LARGE_INTEGER timeNow;
		bRes = QueryPerformanceCounter(&timeNow);
		_ASSERT(bRes);

		render_perf_info perfInfo;
		perfInfo.start_time = start_time;
		perfInfo.duration = (float)(timeNow.QuadPart - start_time.QuadPart) / (float)_performance_counter_frequency.QuadPart * 1000.0f;

		perf_info_queue.push_back(perfInfo);
		if (perf_info_queue.size() > 16)
			perf_info_queue.pop_front();
		#pragma endregion

		_renderEventsCP->Notify([this] (ID2DRenderEventsSink* sink) { return sink->OnAfterD2DRender(_hWnd, _d2d_dc); });

		_ASSERT(_painting);
		_painting = false;
	}

	virtual HWND HWnd() const override { return _hWnd; }

	virtual ID2D1DeviceContext* dc() const override { return _d2d_dc; }

	virtual ID2D1Factory1* d2d_factory() const override { return _d2d_factory; };

	virtual ID3D11DeviceContext* d3d_dc() const override { return _d3d_dc; }

	virtual IDWriteFactory* dwrite_factory() const override { return _dwrite_factory; }

	virtual float fps() const override
	{
		if (perf_info_queue.empty())
			return 0;

		LARGE_INTEGER start_time = perf_info_queue.cbegin()->start_time;

		LARGE_INTEGER timeNow;
		QueryPerformanceCounter (&timeNow);

		float seconds = (float) (timeNow.QuadPart - start_time.QuadPart) / (float) _performance_counter_frequency.QuadPart;

		float fps = (float) perf_info_queue.size() / seconds;

		return fps;
	}

	virtual float average_render_duration() const override
	{
		if (perf_info_queue.empty())
			return 0;

		float sum = 0;
		for (const auto& entry : perf_info_queue)
		{
			sum += entry.duration;
		}

		float avg = sum / perf_info_queue.size();

		return avg;
	}

	virtual void set_debug_flag (debug_flag flag) override
	{
		if (!(_debug_flags & flag))
		{
			_debug_flags = (debug_flag)(_debug_flags | flag);
			::InvalidateRect(_hWnd, nullptr, FALSE);
		}
	}
	
	virtual void clear_debug_flag (debug_flag flag) override
	{
		if (_debug_flags & flag)
		{
			_debug_flags = (debug_flag)(_debug_flags & ~flag);
			::InvalidateRect(_hWnd, nullptr, FALSE);
		}
	}

	virtual debug_flag debug_flags() const override { return _debug_flags; }

	#pragma region Caret methods
	void process_wm_set_focus()
	{
		if (_caret_on && _caret_blink_on)
			invalidate_caret();
	}

	void process_wm_kill_focus()
	{
		if (_caret_on && _caret_blink_on)
			invalidate_caret();
	}

	void invalidate_caret()
	{
		auto points = corners(_caret_bounds.first);
		auto matrix = D2D1::Matrix3x2F::ReinterpretBaseType(&_caret_bounds.second);
		for (auto& point : points)
			point = matrix->TransformPoint(point);
		auto bounds = polygon_bounds(points);
		edge::invalidate(bounds, _hWnd);
	}

	UINT_PTR timer_id_from_window() const
	{
		// Let's generate a timer ID that should be unique throughout the process.
		// This would be a pointer to some private data of this class within this object.
		return (UINT_PTR)this + offsetof(d2d_renderer, _caret_on);
	}

	static d2d_renderer* window_from_timer_id (UINT_PTR timer_id)
	{
		return (d2d_renderer*)(timer_id - offsetof(d2d_renderer, _caret_on));
	}

	virtual void show_caret (const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color, const D2D1_MATRIX_3X2_F* transform) override
	{
		_ASSERT (!_painting);

		auto new_bounds = std::make_pair (bounds, (transform != nullptr) ? *transform : IdentityMatrix());

		if ((_caret_bounds != new_bounds) || (_caret_color != color))
		{
			if (::GetFocus() == _hWnd)
				invalidate_caret();

			_caret_bounds = new_bounds;
			_caret_color = color;

			if (::GetFocus() == _hWnd)
				invalidate_caret();
		}

		UINT_PTR timer_res = ::SetTimer (_hWnd, timer_id_from_window(), GetCaretBlinkTime(), on_blink_timer);
		_ASSERT (timer_res == timer_id_from_window());
		_caret_on = true;
		_caret_blink_on = true;
	}

	virtual void hide_caret() override
	{
		_ASSERT (!_painting); // "This function may not be called during paiting.

		_ASSERT(_caret_on);
		BOOL bres = ::KillTimer(_hWnd, timer_id_from_window()); _ASSERT(bres);
		_caret_on = false;

		if ((::GetFocus() == _hWnd) && _caret_blink_on)
			invalidate_caret();
	}

	static void CALLBACK on_blink_timer (HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4)
	{
		auto window = window_from_timer_id(Arg3);
		_ASSERT(window->_hWnd == Arg1);
		_ASSERT(window->_caret_on);
		window->_caret_blink_on = !window->_caret_blink_on;
		window->invalidate_caret();
	}
	#pragma endregion
};

namespace edge
{
	HRESULT MakeD2DRenderer (HWND hWnd, ID3D11DeviceContext* d3d_dc, IDWriteFactory* dwrite_factory,
							 ID2D1Factory1* d2d_factory, ID2DRenderer** ppRenderer)
	{
		auto p = com_ptr(new (std::nothrow) d2d_renderer()); RETURN_IF_NULL_ALLOC(p);
		auto hr = p->InitInstance(hWnd, d3d_dc, dwrite_factory, d2d_factory); RETURN_IF_FAILED(hr);
		*ppRenderer = p.detach();
		return S_OK;
	}
}
