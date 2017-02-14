#pragma once
#include "ComPtr.h"
#include "EventManager.h"
#include "Win32Defs.h"

struct IProject;
struct IProjectWindow;

class Object
{
protected:
	virtual ~Object() { }
};

class Bridge : public Object
{
};

class Port : public Object
{
};

struct ISelection abstract : public IUnknown
{
	virtual ~ISelection() { }
	virtual const std::vector<Object*>& GetObjects() const = 0;
};

using SelectionFactory = ComPtr<ISelection>(*const)();
extern const SelectionFactory selectionFactory;

// ============================================================================

struct IEditArea abstract : public IWin32Window
{
	virtual ~IEditArea() { }
};

using EditAreaFactory = std::unique_ptr<IEditArea>(*const)(IProject* project, IProjectWindow* pw, ISelection* selection, IUIFramework* rf, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

struct ProjectWindowClosingEvent : public Event<ProjectWindowClosingEvent, void(IProjectWindow* pw, bool* cancelClose)> { };

struct IProjectWindow : public IUnknown, public IWin32Window
{
	//virtual IProject* GetProject() const = 0;
	virtual ProjectWindowClosingEvent::Subscriber GetProjectWindowClosingEvent() = 0;
	virtual void ShowAtSavedWindowLocation(const wchar_t* regKeyPath) = 0;
	virtual void SaveWindowLocation(const wchar_t* regKeyPath) const = 0;
};

using ProjectWindowFactory = ComPtr<IProjectWindow>(*const)(IProject* project, HINSTANCE rfResourceHInstance, const wchar_t* rfResourceName,
	ISelection* selection, EditAreaFactory editAreaFactory, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
extern const ProjectWindowFactory projectWindowFactory;

// ============================================================================

struct IProject abstract
{
	virtual ~IProject() { }
};

using ProjectFactory = std::unique_ptr<IProject>(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================
