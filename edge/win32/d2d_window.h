
#pragma once
#include "window.h"
#include "com_ptr.h"

namespace edge
{
	class d2d_window abstract : public window
	{
		using base = window;
		D2D1_SIZE_F _clientSizeDips;
		bool _painting = false;
		bool _forceFullPresentation;
		com_ptr<IDWriteFactory> const _dwrite_factory;
		com_ptr<ID3D11Device1> _d3dDevice;
		com_ptr<ID3D11DeviceContext1> _d3dDeviceContext;
		com_ptr<IDXGIDevice2> _dxgiDevice;
		com_ptr<IDXGIAdapter> _dxgiAdapter;
		com_ptr<IDXGIFactory2> _dxgiFactory;
		com_ptr<IDXGISwapChain1> _swapChain;
		com_ptr<ID2D1DeviceContext> _d2dDeviceContext;
		com_ptr<ID2D1Factory1> _d2dFactory;
		bool _caret_shown = false;
		RECT _caret_bounds;
		uint32_t _dpi;

		struct RenderPerfInfo
		{
			LARGE_INTEGER startTime;
			float durationMilliseconds;
		};

		LARGE_INTEGER _performanceCounterFrequency;
		std::deque<RenderPerfInfo> perfInfoQueue;

		enum class DebugFlag
		{
			RenderFrameDurationsAndFPS = 1,
			RenderUpdateRects = 2,
			FullClear = 4,
		};
		//virtual bool GetDebugFlag (DebugFlag flag) const = 0;
		//virtual void SetDebugFlag (DebugFlag flag, bool value) = 0;

		DebugFlag _debugFlags = (DebugFlag)0; //DebugFlag::RenderFrameDurationsAndFPS;
		com_ptr<IDWriteTextFormat> _debugTextFormat;

	public:
		d2d_window (HINSTANCE hInstance, DWORD exStyle, DWORD style,
				   const RECT& rect, HWND hWndParent, int child_control_id,
				   ID3D11DeviceContext1* deviceContext, IDWriteFactory* dwrite_factory);

		D2D1_SIZE_F client_size() const { return _clientSizeDips; }
		float client_width() const { return _clientSizeDips.width; }
		float client_height() const { return _clientSizeDips.height; }
		D2D1_POINT_2F pointp_to_pointd (POINT locationPixels) const;
		D2D1_POINT_2F pointp_to_pointd (long xPixels, long yPixels) const;
		POINT pointd_to_pointp (float xDips, float yDips, int round_style) const;
		POINT pointd_to_pointp (D2D1_POINT_2F locationDips, int round_style) const;
		D2D1_SIZE_F GetDipSizeFromPixelSize(SIZE sizePixels) const;
		SIZE GetPixelSizeFromDipSize(D2D1_SIZE_F sizeDips) const;
		D2D1_MATRIX_3X2_F dpi_transform() const;

		ID2D1DeviceContext* d2d_dc() const { return _d2dDeviceContext; }
		ID2D1Factory1* d2d_factory() const { return _d2dFactory; }
		IDWriteFactory* dwrite_factory() const { return _dwrite_factory; }

		void SetCaretBounds (const D2D1_RECT_F& bounds);
		void ShowCaret();
		void HideCaret();

		float GetFPS();
		float GetAverageRenderDuration();

		void invalidate (const D2D1_RECT_F& rect);
		void invalidate();

		uint32_t dpi() const { return _dpi; }

	protected:
		virtual std::optional<LRESULT> window_proc (HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
		virtual void create_render_resources (ID2D1DeviceContext* dc) { }
		virtual void render (ID2D1DeviceContext* dc) const = 0;
		virtual void release_render_resources (ID2D1DeviceContext* dc) { }

	private:
		void create_and_show_win32_caret();
		void CreateD2DDeviceContext();
		void ReleaseD2DDeviceContext();
		void ProcessWmPaint();
		void process_wm_set_focus();
		void process_wm_kill_focus();
	};

	struct text_layout
	{
		com_ptr<IDWriteTextLayout> layout;
		DWRITE_TEXT_METRICS metrics;

		static text_layout create (IDWriteFactory* dWriteFactory, IDWriteTextFormat* format, std::string_view str, float maxWidth = 0);
	};
}
