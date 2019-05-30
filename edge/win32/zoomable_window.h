
#pragma once
#include "d2d_window.h"

namespace edge { class zoomable_window; }

class edge::zoomable_window abstract : public d2d_window, public zoomable_i
{
	using base = d2d_window;

	static constexpr float ZoomFactor = 1.5f;
	D2D1_POINT_2F _workspaceOrigin = { 0, 0 }; // Location in client area of the point (0;0) of the workspace
	float _zoom = 1.0f;
	float _minDistanceBetweenGridPoints = 15;
	float _minDistanceBetweenGridLines = 40;
	bool _enableUserZoomingAndPanning = true;
	bool _panning = false;
	D2D1_POINT_2F _panningLastMouseLocation;

	struct SmoothZoomInfo
	{
		float _zoomStart;
		float _woXStart;
		float _woYStart;
		float _zoomEnd;
		float _woXEnd;
		float _woYEnd;
		LARGE_INTEGER _zoomStartTime;
	};
	std::unique_ptr<SmoothZoomInfo> _smoothZoomInfo;

	struct ZoomedToRect
	{
		D2D1_RECT_F  _rect;
		float _minMarginDips;
		float _maxZoomOrZero;
	};
	std::unique_ptr<ZoomedToRect> _zoomedToRect;

public:
	using base::base;

	D2D1::Matrix3x2F GetZoomTransform() const;
	float GetZoom() const { return _zoom; }
	D2D1_POINT_2F GetWorkspaceOrigin() const { return _workspaceOrigin; }
	float GetWorkspaceOriginX() const { return _workspaceOrigin.x; }
	float GetWorkspaceOriginY() const { return _workspaceOrigin.y; }
	void ZoomToRectangle(const D2D1_RECT_F& rect, float minMarginDips, float maxZoomOrZero, bool smooth);
	void SetZoomAndOrigin(float zoom, float originX, float originY, bool smooth);

	// zoomable_i
	D2D1_POINT_2F pointd_to_pointw (D2D1_POINT_2F dlocation) const final;
	D2D1_POINT_2F pointw_to_pointd (D2D1_POINT_2F wlocation) const final;
	float lengthw_to_lengthd (float wLength) const final { return wLength * _zoom; }
	//virtual zoom_transform_changed_event::subscriber zoom_transform_changed() override final { return zoom_transform_changed_event::subscriber(this); }

protected:
	virtual std::optional<LRESULT> window_proc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	virtual void create_render_resources (ID2D1DeviceContext* dc) override;
	virtual void release_render_resources (ID2D1DeviceContext* dc) override;
	virtual void OnZoomTransformChanged();

private:
	void SetZoomAndOriginInternal (float zoom, float originX, float originY, bool smooth);
	void ProcessWmSize (WPARAM wparam, LPARAM lparam);
	bool ProcessWmMButtonDown(WPARAM wparam, LPARAM lparam);
	bool ProcessWmMButtonUp(WPARAM wparam, LPARAM lparam);
	bool ProcessWmMouseWheel(WPARAM wparam, LPARAM lparam);
	void ProcessWmMouseMove(WPARAM wparam, LPARAM lparam);
};

