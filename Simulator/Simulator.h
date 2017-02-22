#pragma once
#include "EventManager.h"
#include "Win32Defs.h"
#include "UtilityFunctions.h"

struct IProject;
struct IProjectWindow;
struct ISelection;
struct ILogArea;
struct ISidePanel;
struct DrawingObjects;
class Object;
class Bridge;
class Port;
class Wire;

enum class MouseButton
{
	None = 0,
	Left = 1,
	Right = 2,
	Middle = 4,
};

struct AddedToSelectionEvent : public Event<AddedToSelectionEvent, void(ISelection*, Object*)> { };
struct RemovingFromSelectionEvent : public Event<RemovingFromSelectionEvent, void(ISelection*, Object*)> { };
struct SelectionChangedEvent : public Event<SelectionChangedEvent, void(ISelection*)> { };

struct ISelection abstract : public IUnknown
{
	virtual const std::vector<ComPtr<Object>>& GetObjects() const = 0;
	virtual void Select (Object* o) = 0;
	virtual void Clear() = 0;
	virtual AddedToSelectionEvent::Subscriber GetAddedToSelectionEvent() = 0;
	virtual RemovingFromSelectionEvent::Subscriber GetRemovingFromSelectionEvent() = 0;
	virtual SelectionChangedEvent::Subscriber GetSelectionChangedEvent() = 0;
};

using SelectionFactory = ComPtr<ISelection>(*const)();
extern const SelectionFactory selectionFactory;

// ============================================================================

struct SidePanelCloseButtonClicked : public Event<SidePanelCloseButtonClicked, void(ISidePanel* panel)> {};
struct SidePanelResizingEvent : public Event<SidePanelResizingEvent, void(ISidePanel* panel, Side side, LONG offset)> {};

struct ISidePanel abstract : public IUnknown
{
	virtual HWND GetHWnd() const = 0;
	virtual SidePanelCloseButtonClicked::Subscriber GetSidePanelCloseButtonClicked() = 0;
	virtual SidePanelResizingEvent::Subscriber GetSidePanelResizingEvent() = 0;
};

using SidePanelFactory = ComPtr<ISidePanel>(*const)(HWND hWndParent, DWORD controlId, const RECT& rect);
extern const SidePanelFactory sidePanelFactory;

// ============================================================================

struct ILogArea abstract : public IUnknown
{
	virtual void SelectBridge (Bridge* b) = 0;
};

using LogAreaFactory = ComPtr<ILogArea>(*const)(HWND hWndParent, DWORD controlId, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
extern const LogAreaFactory logAreaFactory;

// ============================================================================

class EditState;
struct EditStateDeps;

static constexpr float SnapDistance = 6;

struct MouseLocation
{
	POINT pt;
	D2D1_POINT_2F d;
	D2D1_POINT_2F w;
};

struct IEditArea abstract : public IUnknown
{
	virtual HWND GetHWnd() const = 0;
	virtual void SelectVlan (uint16_t vlanNumber) = 0;
	virtual uint16_t GetSelectedVlanNumber() const = 0;
	virtual const DrawingObjects& GetDrawingObjects() const = 0;
	virtual IDWriteFactory* GetDWriteFactory() const = 0;
	virtual void EnterState (std::unique_ptr<EditState>&& state) = 0;
	virtual EditStateDeps MakeEditStateDeps() = 0;
	virtual Port* GetCPAt (D2D1_POINT_2F dLocation, float tolerance) const = 0;
	virtual void RenderHoverCP (ID2D1RenderTarget* rt, Port* port) const = 0;
	virtual D2D1::Matrix3x2F GetZoomTransform() const = 0;
};

using EditAreaFactory = ComPtr<IEditArea>(*const)(IProject* project, IProjectWindow* pw, DWORD controlId, ISelection* selection, IUIFramework* rf, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
extern const EditAreaFactory editAreaFactory;

// ============================================================================

struct ProjectWindowClosingEvent : public Event<ProjectWindowClosingEvent, void(IProjectWindow* pw, bool* cancelClose)> { };
struct SelectedTreeIndexChangedEvent : public Event<SelectedTreeIndexChangedEvent, void(IProjectWindow*, unsigned int)> { };

struct IProjectWindow : public IUnknown
{
	virtual HWND GetHWnd() const = 0;
	virtual ProjectWindowClosingEvent::Subscriber GetProjectWindowClosingEvent() = 0;
	virtual void ShowAtSavedWindowLocation(const wchar_t* regKeyPath) = 0;
	virtual void SaveWindowLocation(const wchar_t* regKeyPath) const = 0;
	virtual unsigned int GetSelectedTreeIndex() const = 0;
	virtual SelectedTreeIndexChangedEvent::Subscriber GetSelectedTreeIndexChangedEvent() = 0;
};

using ProjectWindowFactory = ComPtr<IProjectWindow>(*const)(IProject* project, HINSTANCE rfResourceHInstance, const wchar_t* rfResourceName,
	ISelection* selection, EditAreaFactory editAreaFactory, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory);
extern const ProjectWindowFactory projectWindowFactory;

// ============================================================================

struct ObjectInsertedEvent : public Event<ObjectInsertedEvent, void(IProject*, size_t index, Object*)> { };
struct ObjectRemovingEvent : public Event<ObjectRemovingEvent, void(IProject*, size_t index, Object*)> { };
struct ProjectInvalidateEvent : public Event<ProjectInvalidateEvent, void(IProject*)> { };

struct IProject abstract : public IUnknown
{
	virtual const std::vector<ComPtr<Object>>& GetObjects() const = 0;
	virtual void Insert (size_t index, Object* bridge) = 0;
	virtual void Remove (size_t index) = 0;
	virtual ObjectInsertedEvent::Subscriber GetObjectInsertedEvent() = 0;
	virtual ObjectRemovingEvent::Subscriber GetObjectRemovingEvent() = 0;
	virtual ProjectInvalidateEvent::Subscriber GetProjectInvalidateEvent() = 0;
	void Add (Object* object) { Insert (GetObjects().size(), object); }

	virtual std::array<uint8_t, 6> AllocMacAddressRange (size_t count) = 0;
	virtual std::pair<Wire*, size_t> GetWireConnectedToPort (const Port* port) const = 0;
	virtual Port* FindReceivingPort (Port* txPort) const = 0;
};

using ProjectFactory = ComPtr<IProject>(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================

class IBridgePropsArea abstract : public IUnknown
{
	virtual HWND GetHWnd() const = 0;
};

using BridgePropsAreaFactory = ComPtr<IBridgePropsArea>(*const)(HWND hWndParent, DWORD controlId, const RECT& rect);
extern const BridgePropsAreaFactory bridgePropsAreaFactory;
