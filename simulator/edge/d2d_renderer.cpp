
// This file is part of the "edge" library, available at https://github.com/adigostin/edge
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "d2d_renderer.h"
#include "utility_functions.h"
#include "text_layout.h"

using namespace D2D1;

#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "D3D11.lib")
#pragma comment (lib, "Dxgi.lib")

namespace edge
{
	d2d_renderer::d2d_renderer (win32_window_i* window, ID3D11DeviceContext* d3d_dc, IDWriteFactory* dwrite_factory)
		: _window(window)
		, _d3d_dc(d3d_dc)
		, _dwrite_factory(dwrite_factory)
	{
		com_ptr<ID3D11Device> device;
		d3d_dc->GetDevice(&device);
		auto hr = device->QueryInterface(IID_PPV_ARGS(&_d3d_device)); rassert(SUCCEEDED(hr));

		hr = device->QueryInterface(IID_PPV_ARGS(&_dxgi_device)); rassert(SUCCEEDED(hr));

		hr = _dxgi_device->GetAdapter(&_dxgi_adapter); rassert(SUCCEEDED(hr));

		hr = _dxgi_adapter->GetParent(IID_PPV_ARGS(&_dxgi_factory)); rassert(SUCCEEDED(hr));

		DXGI_SWAP_CHAIN_DESC1 desc;
		desc.Width = std::max (8l, window->client_width_pixels());
		desc.Height = std::max (8l, window->client_height_pixels());
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
		hr = _dxgi_factory->CreateSwapChainForHwnd (_d3d_device, window->hwnd(), &desc, nullptr, nullptr, &_swap_chain); rassert(SUCCEEDED(hr));
		_forceFullPresentation = true;

		create_d2d_dc();

		QueryPerformanceFrequency(&_performance_counter_frequency);

		hr = dwrite_factory->CreateTextFormat (L"Courier New", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, 11, L"en-US", &_debug_text_format); rassert(SUCCEEDED(hr));

		_window->window_proc().add_handler<&d2d_renderer::on_window_proc>(this);
	}

	d2d_renderer::~d2d_renderer()
	{
		_window->window_proc().remove_handler<&d2d_renderer::on_window_proc>(this);
		release_d2d_dc();
	}

	void d2d_renderer::create_d2d_dc()
	{
		rassert (_d2d_dc == nullptr);

		com_ptr<IDXGISurface2> dxgiSurface;
		auto hr = _swap_chain->GetBuffer (0, IID_PPV_ARGS(&dxgiSurface)); rassert(SUCCEEDED(hr));

		D2D1_CREATION_PROPERTIES cps = { };
		cps.debugLevel = D2D1_DEBUG_LEVEL_ERROR;
		cps.options = D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS;
		hr = D2D1CreateDeviceContext (dxgiSurface, &cps, &_d2d_dc); rassert(SUCCEEDED(hr));

		_d2d_dc->SetTextAntialiasMode (D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
	}

	void d2d_renderer::release_d2d_dc()
	{
		rassert (_d2d_dc != nullptr);
		_d2d_dc = nullptr;
	}

	void d2d_renderer::render_no_handlers(ID2D1DeviceContext* dc) const
	{
		dc->Clear({ 0, 0, 0, 1 });

		dc->SetTransform(_window->dpi_transform());

		com_ptr<ID2D1SolidColorBrush> brush;
		static constexpr D2D1_COLOR_F red = { 1, 0, 0, 1 };
		dc->CreateSolidColorBrush (&red, nullptr, &brush);

		auto pw = _window->pixel_width();

		auto rect = inflate(_window->client_rect(), -pw);
		dc->DrawRectangle (&rect, brush, 2 * pw);
		dc->DrawLine ({ rect.left, rect.top }, { rect.right, rect.bottom }, brush, 2 * pw);
		dc->DrawLine ({ rect.left, rect.bottom }, { rect.right, rect.top }, brush, 2 * pw);
	}

	void d2d_renderer::process_wm_size (SIZE client_size_pixels, D2D1_SIZE_F client_size_dips)
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
				_em.event_invoker<dc_releasing_e>()(_d2d_dc);
				release_d2d_dc();
				auto hr = _swap_chain->ResizeBuffers (0, width, height, DXGI_FORMAT_UNKNOWN, 0); rassert(SUCCEEDED(hr));
				create_d2d_dc();
				_em.event_invoker<dc_recreated_e>()(_d2d_dc);
			}
		}
	}

	std::optional<LRESULT> d2d_renderer::on_window_proc (HWND, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		if (msg == WM_SIZE)
		{
			SIZE client_size_pixels = { LOWORD(lparam), HIWORD(lparam) };
			D2D1_SIZE_F client_size_dips = _window->sizep_to_sized(client_size_pixels);
			process_wm_size(client_size_pixels, client_size_dips);
			return std::nullopt;
		}

		if (msg == WM_ERASEBKGND)
			return 0; // 0 means the window remains marked for erasing, so the fErase member of the PAINTSTRUCT structure will be TRUE.

		if (msg == WM_PAINT)
		{
			process_wm_paint();
			return 0;
		}

		if (msg == WM_SETFOCUS)
		{
			process_wm_set_focus();
			return std::nullopt;
		}

		if (msg == WM_KILLFOCUS)
		{
			process_wm_kill_focus();
			return std::nullopt;
		}

		return std::nullopt;
	}

	void d2d_renderer::process_wm_paint()
	{
		HRESULT hr;

		// Call this before calculating the update rects, to allow listeners to invalidate stuff before rendering.
		_em.event_invoker<before_render_e>()(_d2d_dc);

		LARGE_INTEGER start_time;
		BOOL bRes = QueryPerformanceCounter(&start_time); rassert(bRes);

		D2D1_RECT_F frameDurationAndFpsRect;
		auto _clientSizeDips = _window->client_size();
		frameDurationAndFpsRect.left   = round (_clientSizeDips.width - 75) + 0.5f;
		frameDurationAndFpsRect.top    = round (_clientSizeDips.height - 28) + 0.5f;
		frameDurationAndFpsRect.right  = _clientSizeDips.width - 0.5f;
		frameDurationAndFpsRect.bottom = _clientSizeDips.height - 0.5f;
		if ((int) _debug_flags & (int) debug_flag::render_frame_durations_and_fps)
			_window->invalidate (frameDurationAndFpsRect);
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
		::BeginPaint(_window->hwnd(), &ps); // this will also hide the caret, if shown.

		_painting = true;

		_d2d_dc->BeginDraw();
		_d2d_dc->SetTransform(IdentityMatrix());
		/*
		if (!updateEntireClientArea)
		{
			#pragma region Create D2D geometry from _updateRects and push clip layer
			hr = _d2d_factory->CreatePathGeometry(&_updateGeometry); rassert_hr(hr);

			ComPtr<ID2D1GeometrySink> sink;
			hr = _updateGeometry->Open(&sink); rassert_hr(hr);

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

			hr = sink->Close(); rassert_hr(hr);

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
		auto invoker = _em.event_invoker<render_e>();
		if (invoker.has_handlers())
			invoker(_d2d_dc);
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

			std::wstringstream ss;
			ss << std::setw(4) << (int)round(fps()) << L" FPS\r\n " << std::setw(3) << (int)round(average_render_duration()) << L" ms";
			auto tl = text_layout_with_metrics (_dwrite_factory, _debug_text_format, ss.str());

			D2D1_POINT_2F origin;
			origin.x = frameDurationAndFpsRect.right - 4 - tl.width();
			origin.y = (frameDurationAndFpsRect.top + frameDurationAndFpsRect.bottom) / 2 - tl.height() / 2;
			_d2d_dc->DrawTextLayout (origin, tl, foreBrush);
		}
		/*
		if (!updateEntireClientArea
			&& !((int)_debug_flags & (int)debug_flag::full_clear))
		{
			_d2d_dc->PopLayer();
		}
		*/

		if (_caret_on && (::GetFocus() == _window->hwnd()) && _caret_blink_on)
		{
			_d2d_dc->SetTransform(_window->dpi_transform() * _caret_bounds.second);
			com_ptr<ID2D1SolidColorBrush> b;
			_d2d_dc->CreateSolidColorBrush(_caret_color, &b);
			_d2d_dc->FillRectangle (&_caret_bounds.first, b);
		}

		hr = _d2d_dc->EndDraw(); rassert(SUCCEEDED(hr));

		DXGI_PRESENT_PARAMETERS pp = {};
		hr = _swap_chain->Present1(0, 0, &pp); rassert(SUCCEEDED(hr));

		::EndPaint(_window->hwnd(), &ps); // this will show the caret in case BeginPaint above hid it.

		#pragma region Calculate performance data.
		LARGE_INTEGER timeNow;
		bRes = QueryPerformanceCounter(&timeNow);
		rassert(bRes);

		render_perf_info perfInfo;
		perfInfo.start_time = start_time;
		perfInfo.duration = (float)(timeNow.QuadPart - start_time.QuadPart) / (float)_performance_counter_frequency.QuadPart * 1000.0f;

		perf_info_queue.push_back(perfInfo);
		if (perf_info_queue.size() > 16)
			perf_info_queue.pop_front();
		#pragma endregion

		_em.event_invoker<after_render_e>()(_d2d_dc);

		rassert(_painting);
		_painting = false;
	}

	float d2d_renderer::fps ()
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

	float d2d_renderer::average_render_duration()
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

	#pragma region Caret methods
	void d2d_renderer::process_wm_set_focus()
	{
		if (_caret_on && _caret_blink_on)
			invalidate_caret();
	}

	void d2d_renderer::process_wm_kill_focus()
	{
		if (_caret_on && _caret_blink_on)
			invalidate_caret();
	}

	void d2d_renderer::invalidate_caret()
	{
		auto points = corners(_caret_bounds.first);
		auto matrix = D2D1::Matrix3x2F::ReinterpretBaseType(&_caret_bounds.second);
		for (auto& point : points)
			point = matrix->TransformPoint(point);
		auto bounds = polygon_bounds(points);
		_window->invalidate(bounds);
	}

	UINT_PTR d2d_renderer::timer_id_from_window() const
	{
		// Let's generate a timer ID that should be unique throughout the process.
		// This would be a pointer to some private data of this class within this object.
		return (UINT_PTR)this + offsetof(d2d_renderer, _caret_on);
	}

	//static
	d2d_renderer* d2d_renderer::window_from_timer_id (UINT_PTR timer_id)
	{
		return (d2d_renderer*)(timer_id - offsetof(d2d_renderer, _caret_on));
	}

	// This is allowed to be called repeatedly, first to show the caret, and then to move it.
	void d2d_renderer::show_caret (const D2D1_RECT_F& bounds, const D2D1_COLOR_F& color, const D2D1_MATRIX_3X2_F* transform)
	{
		rassert (!_painting);

		auto new_bounds = std::make_pair (bounds, (transform != nullptr) ? *transform : IdentityMatrix());

		if ((_caret_bounds != new_bounds) || (_caret_color != color))
		{
			if (::GetFocus() == _window->hwnd())
				invalidate_caret();

			_caret_bounds = new_bounds;
			_caret_color = color;

			if (::GetFocus() == _window->hwnd())
				invalidate_caret();
		}

		UINT_PTR timer_res = ::SetTimer (_window->hwnd(), timer_id_from_window(), GetCaretBlinkTime(), on_blink_timer);
		rassert (timer_res == timer_id_from_window());
		_caret_on = true;
		_caret_blink_on = true;
	}

	void d2d_renderer::hide_caret()
	{
		rassert (!_painting); // "This function may not be called during paiting.

		rassert(_caret_on);
		BOOL bres = ::KillTimer(_window->hwnd(), timer_id_from_window()); rassert(bres);
		_caret_on = false;

		if ((::GetFocus() == _window->hwnd()) && _caret_blink_on)
			invalidate_caret();
	}

	// static
	void CALLBACK d2d_renderer::on_blink_timer (HWND Arg1, UINT Arg2, UINT_PTR Arg3, DWORD Arg4)
	{
		auto window = window_from_timer_id(Arg3);
		rassert(window->_window->hwnd() == Arg1);
		rassert(window->_caret_on);
		window->_caret_blink_on = !window->_caret_blink_on;
		window->invalidate_caret();
	}
	#pragma endregion
}
