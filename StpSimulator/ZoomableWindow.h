
#pragma once
#include "D2DWindow.h"
#include "EventManager.h"

class ZoomableWindow;
struct ZoomTransformChangedEvent : public Event<ZoomTransformChangedEvent, void(ZoomableWindow*)> { };

class ZoomableWindow abstract : public D2DWindow
{
	typedef D2DWindow base;

	static constexpr float ZoomFactor = 1.5f;
	D2D1_POINT_2F _workspaceOrigin = { 0, 0 }; // Location in client area of the point (0;0) of the workspace
	float _zoom = 1.0f;
	float _minDistanceBetweenGridPoints = 15;
	float _minDistanceBetweenGridLines = 40;
	bool _enableUserZoomingAndPanning = true;
	std::optional<D2D1_POINT_2F> _panningLastMouseLocation;

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
	std::optional<SmoothZoomInfo> _smoothZoomInfo;

	struct ZoomedToRect
	{
		D2D1_RECT_F  _rect;
		float _minMarginDips;
		float _maxZoomOrZero;
	};
	std::optional<ZoomedToRect> _zoomedToRect;

	EventManager _em;

public:
	using base::base;

	ZoomTransformChangedEvent::Subscriber GetZoomTransformChangedEvent() { return ZoomTransformChangedEvent::Subscriber(_em); }
	D2D1_MATRIX_3X2_F GetZoomTransform() const;
	float GetZoom() const { return _zoom; }
	D2D1_POINT_2F GetWorkspaceOrigin() const { return _workspaceOrigin; }
	float GetWorkspaceOriginX() const { return _workspaceOrigin.x; }
	float GetWorkspaceOriginY() const { return _workspaceOrigin.y; }
	void ZoomToRectangle(const D2D1_RECT_F& rect, float minMarginDips, float maxZoomOrZero, bool smooth);
	void SetZoomAndOrigin(float zoom, float originX, float originY, bool smooth);

protected:
	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override;
	virtual void OnBeforeRender() override;
	virtual void OnAfterRender() override;
	void OnZoomTransformChanged();

private:
	void SetZoomAndOriginInternal (float zoom, float originX, float originY, bool smooth);
	void ProcessWmSize (WPARAM wparam, LPARAM lparam);
	std::optional<LRESULT> ProcessWmMButtonDown(WPARAM wparam, LPARAM lparam);
	std::optional<LRESULT> ProcessWmMButtonUp(WPARAM wparam, LPARAM lparam);
	std::optional<LRESULT> ProcessWmMouseWheel(WPARAM wparam, LPARAM lparam);
	void ProcessWmMouseMove(WPARAM wparam, LPARAM lparam);
};

