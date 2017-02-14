#pragma once
#include "ComPtr.h"
#include "EventManager.h"
#include "Win32Defs.h"

struct IProject;
struct IProjectWindow;

// ============================================================================

struct IEditArea abstract : public IWin32Window
{
	virtual ~IEditArea() { }
};

using EditAreaFactory = std::unique_ptr<IEditArea>(IProject* project, IProjectWindow* pw, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
extern EditAreaFactory* const editAreaFactory;

// ============================================================================

struct ProjectWindowClosingEvent : public Event<ProjectWindowClosingEvent, void(IProjectWindow* pw, bool* cancelClose)> { };

struct IProjectWindow : public IUnknown, public IWin32Window
{
	//virtual IProject* GetProject() const = 0;
	virtual ProjectWindowClosingEvent::Subscriber GetProjectWindowClosingEvent() = 0;
	virtual void ShowAtSavedWindowLocation(const wchar_t* regKeyPath) = 0;
	virtual void SaveWindowLocation(const wchar_t* regKeyPath) const = 0;
};

using ProjectWindowFactory = ComPtr<IProjectWindow>(IProject* project, HINSTANCE rfResourceHInstance, const wchar_t* rfResourceName,
	EditAreaFactory* const editAreaFactory, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
extern ProjectWindowFactory* const projectWindowFactory;

// ============================================================================

struct IProject abstract
{
	virtual ~IProject() { }
};

using ProjectFactory = std::unique_ptr<IProject>();
extern ProjectFactory* const projectFactory;

// ============================================================================
