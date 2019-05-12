
#include "pch.h"
#include "d2d_window.h"
#include "utility_functions.h"

using namespace D2D1;

#pragma comment (lib, "d2d1.lib")
#pragma comment (lib, "dwrite.lib")
#pragma comment (lib, "D3D11.lib")
#pragma comment (lib, "Dxgi.lib")

namespace edge
{
	d2d_window::d2d_window (HINSTANCE hInstance, DWORD exStyle, DWORD style,
						  const RECT& rect, HWND hWndParent, int child_control_id,
						  ID3D11DeviceContext1* deviceContext, IDWriteFactory* dwrite_factory)
		: window(hInstance, exStyle, style, rect, hWndParent, child_control_id)
		, _d3dDeviceContext(deviceContext)
		, _dwrite_factory(dwrite_factory)
	{
		com_ptr<ID3D11Device> device;
		deviceContext->GetDevice(&device);
		auto hr = device->QueryInterface(IID_PPV_ARGS(&_d3dDevice)); assert(SUCCEEDED(hr));

		hr = device->QueryInterface(IID_PPV_ARGS(&_dxgiDevice)); assert(SUCCEEDED(hr));

		hr = _dxgiDevice->GetAdapter(&_dxgiAdapter); assert(SUCCEEDED(hr));

		hr = _dxgiAdapter->GetParent(IID_PPV_ARGS(&_dxgiFactory)); assert(SUCCEEDED(hr));

		DXGI_SWAP_CHAIN_DESC1 desc;
		desc.Width = std::max (8l, client_width_pixels());
		desc.Height = std::max (8l, client_height_pixels());
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
		hr = _dxgiFactory->CreateSwapChainForHwnd (_d3dDevice, hwnd(), &desc, nullptr, nullptr, &_swapChain); assert(SUCCEEDED(hr));
		_forceFullPresentation = true;

		CreateD2DDeviceContext();

		if (auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "GetDpiForWindow"))
		{
			auto proc = reinterpret_cast<UINT(WINAPI*)(HWND)>(proc_addr);
			_dpi = proc(hwnd());
		}
		else
		{
			HDC tempDC = GetDC(hwnd());
			_dpi = GetDeviceCaps (tempDC, LOGPIXELSX);
			ReleaseDC (hwnd(), tempDC);
		}

		_clientSizeDips.width = client_width_pixels() * 96.0f / _dpi;
		_clientSizeDips.height = client_height_pixels() * 96.0f / _dpi;

		QueryPerformanceFrequency(&_performanceCounterFrequency);

		hr = dwrite_factory->CreateTextFormat (L"Courier New", nullptr, DWRITE_FONT_WEIGHT_BOLD, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_CONDENSED, 11.0f * 96 / _dpi, L"en-US", &_debugTextFormat); assert(SUCCEEDED(hr));
	}

	void d2d_window::CreateD2DDeviceContext()
	{
		assert (_d2dDeviceContext == nullptr);
		assert (_d2dFactory       == nullptr);

		com_ptr<IDXGISurface2> dxgiSurface;
		auto hr = _swapChain->GetBuffer (0, IID_PPV_ARGS(&dxgiSurface)); assert(SUCCEEDED(hr));

		D2D1_CREATION_PROPERTIES cps = { };
		cps.debugLevel = D2D1_DEBUG_LEVEL_ERROR;
		cps.options = D2D1_DEVICE_CONTEXT_OPTIONS_ENABLE_MULTITHREADED_OPTIMIZATIONS;
		hr = D2D1CreateDeviceContext (dxgiSurface, &cps, &_d2dDeviceContext); assert(SUCCEEDED(hr));

		com_ptr<ID2D1Factory> factory;
		_d2dDeviceContext->GetFactory(&factory);
		hr = factory->QueryInterface(&_d2dFactory); assert(SUCCEEDED(hr));

		_d2dDeviceContext->SetTextAntialiasMode (D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
	}

	void d2d_window::ReleaseD2DDeviceContext()
	{
		assert (_d2dDeviceContext != nullptr);
		assert (_d2dFactory != nullptr);
		_d2dDeviceContext = nullptr;
		_d2dFactory = nullptr;
	}

	std::optional<LRESULT> d2d_window::window_proc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		auto resultBaseClass = base::window_proc(hwnd, uMsg, wParam, lParam);

		if (uMsg == WM_SIZE)
		{
			if (_swapChain != nullptr)
			{
				ReleaseD2DDeviceContext();
				auto hr = _swapChain->ResizeBuffers (0, 0, 0, DXGI_FORMAT_UNKNOWN, 0); assert(SUCCEEDED(hr));
				CreateD2DDeviceContext();
			}

			_clientSizeDips.width = client_width_pixels() * 96.0f / _dpi;
			_clientSizeDips.height = client_height_pixels() * 96.0f / _dpi;

			return 0;
		}

		if (uMsg == 0x02E3) // WM_DPICHANGED_AFTERPARENT
		{
			auto proc_addr = GetProcAddress(GetModuleHandleA("User32.dll"), "GetDpiForWindow");
			auto proc = reinterpret_cast<UINT(WINAPI*)(HWND)>(proc_addr);
			_dpi = proc(hwnd);
			_clientSizeDips.width = client_width_pixels() * 96.0f / _dpi;
			_clientSizeDips.height = client_height_pixels() * 96.0f / _dpi;
			invalidate();
			return 0;
		}

		if (uMsg == WM_ERASEBKGND)
			return 0; // 0 means the window remains marked for erasing, so the fErase member of the PAINTSTRUCT structure will be TRUE.

		if (uMsg == WM_PAINT)
		{
			process_wm_paint();
			return 0;
		}

		if (uMsg == WM_SETFOCUS)
		{
			process_wm_set_focus();
			return 0;
		}

		if (uMsg == WM_KILLFOCUS)
		{
			process_wm_kill_focus();
			return 0;
		}

		if (uMsg == WM_BLINK)
		{
			process_wm_blink();
			return 0;
		}

		return std::nullopt;
	}

	void d2d_window::process_wm_paint()
	{
		HRESULT hr;

		if (_painting)
		{
			// We get here when we're called recursively. The only such case I've seen so far is when
			// an assertion fails in code called from this function. We don't want to restart painting
			// cause we'll end up with a stack overflow, so let's return without attempting anything "smart".
			return;
		}

		// Call this before calculating the update rects, to allow derived classed to invalidate stuff.
		this->create_render_resources (_d2dDeviceContext);

		LARGE_INTEGER startTime;
		BOOL bRes = QueryPerformanceCounter(&startTime); assert(bRes);
	
		D2D1_RECT_F frameDurationAndFpsRect;
		frameDurationAndFpsRect.left   = round (_clientSizeDips.width - 75) + 0.5f;
		frameDurationAndFpsRect.top    = round (_clientSizeDips.height - 28) + 0.5f;
		frameDurationAndFpsRect.right  = _clientSizeDips.width - 0.5f;
		frameDurationAndFpsRect.bottom = _clientSizeDips.height - 0.5f;
		if ((int) _debugFlags & (int) DebugFlag::RenderFrameDurationsAndFPS)
			this->invalidate (frameDurationAndFpsRect);
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
		::BeginPaint(hwnd(), &ps); // this will also hide the caret, if shown.

		_painting = true;

		_d2dDeviceContext->BeginDraw();
		_d2dDeviceContext->SetTransform(IdentityMatrix());
		/*
		if (!updateEntireClientArea)
		{
			#pragma region Create D2D geometry from _updateRects and push clip layer
			hr = _d2dFactory->CreatePathGeometry(&_updateGeometry); rassert_hr(hr);

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

			if (!((int)_debugFlags & (int)DebugFlag::FullClear))
			{
				D2D1_LAYER_PARAMETERS1 layerParams = {};
				layerParams.contentBounds = InfiniteRect();
				layerParams.geometricMask = _updateGeometry;
				layerParams.maskAntialiasMode = D2D1_ANTIALIAS_MODE_ALIASED;
				layerParams.maskTransform = IdentityMatrix();
				layerParams.opacity = 1.0f;
				layerParams.opacityBrush = nullptr;
				layerParams.layerOptions = D2D1_LAYER_OPTIONS1_IGNORE_ALPHA | D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND;
				_d2dDeviceContext->PushLayer(&layerParams, nullptr);
				// Note AG: without D2D1_LAYER_OPTIONS1_INITIALIZE_FROM_BACKGROUND Direct2D calls ClearView,
				// which some graphic drivers implement in software, which is extremely slow. (Intel integrated for example.)
			}
			#pragma endregion
		}
		*/
		this->render (_d2dDeviceContext);
		/*
		_updateGeometry = nullptr;

		if ((int)_debugFlags & (int)DebugFlag::RenderUpdateRects)
		{
			ComPtr<ID2D1SolidColorBrush> debugBrush;
			_d2dDeviceContext->CreateSolidColorBrush(ColorF(ColorF::Red), &debugBrush);

			for (auto& rect : _updateRects)
				_d2dDeviceContext->DrawRectangle(RectF(rect.left + 0.5f, rect.top + 0.5f, rect.right - 0.5f, rect.bottom - 0.5f), debugBrush);

			debugBrush->SetColor({ (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, (float)rand() / RAND_MAX, 0.25f });

			for (auto& rect : _updateRects)
				_d2dDeviceContext->FillRectangle(RectF(rect.left + 0.5f, rect.top + 0.5f, rect.right - 0.5f, rect.bottom - 0.5f), debugBrush);
		}
		*/
		if ((int)_debugFlags & (int)DebugFlag::RenderFrameDurationsAndFPS)
		{
			com_ptr<ID2D1SolidColorBrush> backBrush;
			_d2dDeviceContext->CreateSolidColorBrush (ColorF::ColorF (ColorF::Yellow, 0.5f), &backBrush);
			_d2dDeviceContext->FillRectangle(frameDurationAndFpsRect, backBrush);

			com_ptr<ID2D1SolidColorBrush> foreBrush;
			_d2dDeviceContext->CreateSolidColorBrush (ColorF::ColorF (ColorF::Black), &foreBrush);

			wchar_t text[50];
			auto text_len = swprintf_s (text, L"%4d FPS\r\n %3d ms", (int)round(GetFPS()), (int)round(GetAverageRenderDuration()));
			com_ptr<IDWriteTextLayout> tl;
			hr = _dwrite_factory->CreateTextLayout (text, (UINT32)wcslen(text), _debugTextFormat, 10000, 10000, &tl); assert(SUCCEEDED(hr));

			DWRITE_TEXT_METRICS metrics;
			hr = tl->GetMetrics (&metrics); assert(SUCCEEDED(hr));

			D2D1_POINT_2F origin;
			origin.x = frameDurationAndFpsRect.right - 4 - metrics.width;
			origin.y = (frameDurationAndFpsRect.top + frameDurationAndFpsRect.bottom) / 2 - metrics.height / 2;
			_d2dDeviceContext->DrawTextLayout (origin, tl, foreBrush);
		}
		/*
		if (!updateEntireClientArea
			&& !((int)_debugFlags & (int)DebugFlag::FullClear))
		{
			_d2dDeviceContext->PopLayer();
		}
		*/

		if ((_caret_blink_timer != nullptr) && (::GetFocus() == hwnd()) && _caret_blink_on)
		{
			_d2dDeviceContext->SetTransform(dpi_transform() * _caret_bounds.second);
			com_ptr<ID2D1SolidColorBrush> b;
			_d2dDeviceContext->CreateSolidColorBrush(_caret_color, &b);
			_d2dDeviceContext->FillRectangle (&_caret_bounds.first, b);
		}

		hr = _d2dDeviceContext->EndDraw(); assert(SUCCEEDED(hr));

		DXGI_PRESENT_PARAMETERS pp = {};
		hr = _swapChain->Present1(0, 0, &pp); assert(SUCCEEDED(hr));

		::EndPaint(hwnd(), &ps); // this will show the caret in case BeginPaint above hid it.
	
		#pragma region Calculate performance data.
		LARGE_INTEGER timeNow;
		bRes = QueryPerformanceCounter(&timeNow);
		assert(bRes);

		RenderPerfInfo perfInfo;
		perfInfo.startTime = startTime;
		perfInfo.durationMilliseconds = (float)(timeNow.QuadPart - startTime.QuadPart) / (float)_performanceCounterFrequency.QuadPart * 1000.0f;

		perfInfoQueue.push_back(perfInfo);
		if (perfInfoQueue.size() > 16)
			perfInfoQueue.pop_front();
		#pragma endregion
	
		this->release_render_resources (_d2dDeviceContext);

		assert(_painting);
		_painting = false;
	}


	float d2d_window::GetFPS ()
	{
		if (perfInfoQueue.empty())
			return 0;

		LARGE_INTEGER startTime = perfInfoQueue.cbegin()->startTime;

		LARGE_INTEGER timeNow;
		QueryPerformanceCounter (&timeNow);

		float seconds = (float) (timeNow.QuadPart - startTime.QuadPart) / (float) _performanceCounterFrequency.QuadPart;

		float fps = (float) perfInfoQueue.size() / seconds;

		return fps;
	}

	float d2d_window::GetAverageRenderDuration()
	{
		if (perfInfoQueue.empty())
			return 0;

		float sum = 0;
		for (const auto& entry : perfInfoQueue)
		{
			sum += entry.durationMilliseconds;
		}

		float avg = sum / perfInfoQueue.size();

		return avg;
	}

	D2D1_POINT_2F d2d_window::pointp_to_pointd (POINT p) const
	{
		return { p.x * 96.0f / _dpi, p.y * 96.0f / _dpi };
	}

	D2D1_POINT_2F d2d_window::pointp_to_pointd (long xPixels, long yPixels) const
	{
		return { xPixels * 96.0f / _dpi, yPixels * 96.0f / _dpi };
	}

	POINT d2d_window::pointd_to_pointp (float xDips, float yDips, int round_style) const
	{
		if (round_style < 0)
			return { (int)std::floor(xDips / 96.0f * _dpi), (int)std::floor(yDips / 96.0f * _dpi) };

		if (round_style > 0)
			return { (int)std::ceil(xDips / 96.0f * _dpi), (int)std::ceil(yDips / 96.0f * _dpi) };

		return { (int)std::round(xDips / 96.0f * _dpi), (int)std::round(yDips / 96.0f * _dpi) };
	}

	POINT d2d_window::pointd_to_pointp (D2D1_POINT_2F locationDips, int round_style) const
	{
		return pointd_to_pointp(locationDips.x, locationDips.y, round_style);
	}

	D2D1_SIZE_F d2d_window::GetDipSizeFromPixelSize(SIZE sz) const
	{
		return D2D1_SIZE_F{ sz.cx * 96.0f / _dpi, sz.cy * 96.0f / _dpi };
	}

	SIZE d2d_window::GetPixelSizeFromDipSize(D2D1_SIZE_F sizeDips) const
	{
		return SIZE{ (int)(sizeDips.width / 96.0f * _dpi), (int)(sizeDips.height / 96.0f * _dpi) };
	}

	D2D1_MATRIX_3X2_F d2d_window::dpi_transform() const
	{
		return { (float)_dpi / 96, 0, 0, (float)_dpi / 96, 0, 0 };
	}

	void d2d_window::invalidate (const D2D1_RECT_F& rect)
	{
		auto tl = this->pointd_to_pointp (rect.left, rect.top, -1);
		auto br = this->pointd_to_pointp (rect.right, rect.bottom, 1);
		RECT rc = { tl.x, tl.y, br.x, br.y };
		::InvalidateRect (hwnd(), &rc, FALSE);
	}

	#pragma region Caret methods
	void d2d_window::process_wm_set_focus()
	{
		if ((_caret_blink_timer != nullptr) && _caret_blink_on)
			invalidate_caret();
	}

	void d2d_window::process_wm_kill_focus()
	{
		if ((_caret_blink_timer != nullptr) && _caret_blink_on)
			invalidate_caret();
	}

	void d2d_window::invalidate_caret()
	{
		auto points = corners(_caret_bounds.first);
		auto matrix = D2D1::Matrix3x2F::ReinterpretBaseType(&_caret_bounds.second);
		for (auto& point : points)
			point = matrix->TransformPoint(point);
		auto bounds = polygon_bounds(points);
		invalidate(bounds);
	}

	void d2d_window::show_caret (const D2D1_RECT_F& bounds, D2D1_COLOR_F color, const D2D1_MATRIX_3X2_F* transform)
	{
		assert (!_painting);

		auto new_bounds = std::make_pair (bounds, (transform != nullptr) ? *transform : IdentityMatrix());

		if ((_caret_bounds != new_bounds) || (_caret_color != color))
		{
			if (::GetFocus() == hwnd())
				invalidate_caret();

			_caret_bounds = new_bounds;
			_caret_color = color;

			if (::GetFocus() == hwnd())
				invalidate_caret();
		}

		static const auto blink_callback = [](void* lpParameter, BOOLEAN)
		{
			// We're on a worker thread. Let's post this message and continue processing on the GUI thread.
			auto d2dw = static_cast<d2d_window*>(lpParameter);
			::PostMessage(d2dw->hwnd(), WM_BLINK, 0, 0);
		};
		_caret_blink_timer = create_timer_queue_timer (blink_callback, this, GetCaretBlinkTime(), GetCaretBlinkTime());
		_caret_blink_on = true;
	}

	void d2d_window::hide_caret()
	{
		assert (!_painting); // "This function may not be called during paiting.

		assert (_caret_blink_timer != nullptr); // ShowCaret() was not called.

		_caret_blink_timer = nullptr;

		if ((::GetFocus() == hwnd()) && _caret_blink_on)
			invalidate_caret();
	}

	void d2d_window::process_wm_blink()
	{
		if (_caret_blink_timer != nullptr)
		{
			_caret_blink_on = !_caret_blink_on;
			invalidate_caret();
		}
	}

	#pragma endregion

	text_layout text_layout::create (IDWriteFactory* dWriteFactory, IDWriteTextFormat* format, std::string_view str, float maxWidth)
	{
		assert (maxWidth >= 0);

		HRESULT hr;

		if (maxWidth == 0)
			maxWidth = 100'000;

		com_ptr<IDWriteTextLayout> tl;
		if (str.empty())
		{
			hr = dWriteFactory->CreateTextLayout (L"", 0, format, maxWidth, 100'000, &tl);
			assert(SUCCEEDED(hr));
		}
		else
		{
			int utf16_char_count = MultiByteToWideChar (CP_UTF8, 0, str.data(), (int)str.length(), nullptr, 0);
			assert(utf16_char_count > 0);

			auto buffer = std::make_unique<wchar_t[]>(utf16_char_count);
			MultiByteToWideChar (CP_UTF8, 0, str.data(), (int)str.length(), buffer.get(), utf16_char_count);

			hr = dWriteFactory->CreateTextLayout (buffer.get(), (UINT32) utf16_char_count, format, (maxWidth != 0) ? maxWidth : 10000, 10000, &tl);
			assert(SUCCEEDED(hr));
		}

		DWRITE_TEXT_METRICS metrics;
		hr = tl->GetMetrics(&metrics); assert(SUCCEEDED(hr));

		return text_layout { std::move(tl), metrics };
	}
}
