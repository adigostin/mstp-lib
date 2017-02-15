#pragma once
#include "EventManager.h"
#include "Win32Defs.h"
#include "PhysicalBridge.h"

struct IProject;
struct IProjectWindow;

struct ISelection abstract : public IUnknown
{
	virtual ~ISelection() { }
	virtual const std::vector<Object*>& GetObjects() const = 0;
};

using SelectionFactory = ComPtr<ISelection>(*const)();
extern const SelectionFactory selectionFactory;

// ============================================================================

struct IEditArea abstract : public IWin32Window, public IUnknown
{
};

using EditAreaFactory = ComPtr<IEditArea>(*const)(IProject* project, IProjectWindow* pw, ISelection* selection, IUIFramework* rf, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
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

struct BridgeInsertedEvent : public Event<BridgeInsertedEvent, void(IProject*, size_t index, PhysicalBridge*)> { };
struct BridgeRemovingEvent : public Event<BridgeRemovingEvent, void(IProject*, size_t index, PhysicalBridge*)> { };

struct IProject abstract : public IUnknown
{
	virtual const std::vector<std::unique_ptr<PhysicalBridge>>& GetBridges() const = 0;
	virtual void InsertBridge (size_t index, std::unique_ptr<PhysicalBridge>&& bridge) = 0;
	virtual void RemoveBridge (size_t index) = 0;
	virtual BridgeInsertedEvent::Subscriber GetBridgeInsertedEvent() = 0;
	virtual BridgeRemovingEvent::Subscriber GetBridgeRemovingEvent() = 0;

	void AddBridge (std::unique_ptr<PhysicalBridge>&& bridge) { InsertBridge (GetBridges().size(), move(bridge)); }
};

using ProjectFactory = ComPtr<IProject>(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================
