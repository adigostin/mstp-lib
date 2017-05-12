#pragma once
#include "..\Simulator.h"

class EditState abstract
{
protected:
	IProjectWindow* const _pw;

public:
	EditState (IProjectWindow* pw)
		: _pw(pw)
	{ }

	virtual ~EditState() { }
	virtual void OnMouseDown (const MouseLocation& location, MouseButton button) { }
	virtual void OnMouseMove (const MouseLocation& location) { }
	virtual void OnMouseUp   (const MouseLocation& location, MouseButton button) { }
	virtual std::optional<LRESULT> OnKeyDown (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual std::optional<LRESULT> OnKeyUp   (UINT virtualKey, UINT modifierKeys) { return std::nullopt; }
	virtual void Render (ID2D1RenderTarget* dc) { }
	virtual bool Completed() const = 0;
	virtual HCURSOR GetCursor() const { return LoadCursor(nullptr, IDC_ARROW); }
};

std::unique_ptr<EditState> CreateStateMoveBridges (IProjectWindow* pw);
std::unique_ptr<EditState> CreateStateMovePort (IProjectWindow* pw);
std::unique_ptr<EditState> CreateStateCreateBridge (IProjectWindow* pw);
std::unique_ptr<EditState> CreateStateCreateWire (IProjectWindow* pw, Port* fromPort);
std::unique_ptr<EditState> CreateStateMoveWirePoint (IProjectWindow* pw, Wire* wire, size_t pointIndex);
