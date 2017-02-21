#pragma once
#include "EventManager.h"
#include "Win32Defs.h"

struct IProject;
struct IProjectWindow;
struct ISelection;
struct ILogArea;
struct DrawingObjects;
class Object;
class Bridge;
class Port;
class Wire;

struct HTResult
{
	Object* object;
	int code;
};

class Object : public IUnknown
{
	ULONG _refCount = 1;
protected:
	EventManager _em;
	virtual ~Object() { assert(_refCount == 0); }

public:
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override { return E_NOTIMPL; }
	virtual ULONG STDMETHODCALLTYPE AddRef() override final;
	virtual ULONG STDMETHODCALLTYPE Release() override final;

	struct InvalidateEvent : public Event<InvalidateEvent, void(Object*)> { };
	InvalidateEvent::Subscriber GetInvalidateEvent() { return InvalidateEvent::Subscriber(_em); }

	virtual void Render (ID2D1RenderTarget* rt, const DrawingObjects& dos, IDWriteFactory* dWriteFactory, uint16_t vlanNumber) const = 0;
	virtual void RenderSelection (const IZoomable* zoomable, ID2D1RenderTarget* rt, const DrawingObjects& dos) const = 0;
	virtual HTResult HitTest (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance) = 0;
};

enum class Side { Left, Top, Right, Bottom };

struct DrawingObjects
{
	ComPtr<ID2D1SolidColorBrush> _poweredOutlineBrush;
	ComPtr<ID2D1SolidColorBrush> _poweredFillBrush;
	ComPtr<ID2D1SolidColorBrush> _unpoweredBrush;
	ComPtr<ID2D1SolidColorBrush> _brushWindowText;
	ComPtr<ID2D1SolidColorBrush> _brushWindow;
	ComPtr<ID2D1SolidColorBrush> _brushHighlight;
	ComPtr<ID2D1SolidColorBrush> _brushDiscardingPort;
	ComPtr<ID2D1SolidColorBrush> _brushLearningPort;
	ComPtr<ID2D1SolidColorBrush> _brushForwarding;
	ComPtr<ID2D1SolidColorBrush> _brushNoForwardingWire;
	ComPtr<ID2D1SolidColorBrush> _brushTempWire;
	ComPtr<ID2D1StrokeStyle> _strokeStyleNoForwardingWire;
	ComPtr<IDWriteTextFormat> _regularTextFormat;
	ComPtr<ID2D1StrokeStyle> _strokeStyleSelectionRect;
};

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

struct LogAreaCloseButtonClicked : public Event<LogAreaCloseButtonClicked, void(ILogArea* logArea)> {};
struct LogAreaResizingEvent : public Event<LogAreaResizingEvent, void(ILogArea* logArea, Side side, LONG offset)> {};

struct ILogArea abstract : public IUnknown
{
	virtual HWND GetHWnd() const = 0;
	virtual LogAreaCloseButtonClicked::Subscriber GetLogAreaCloseButtonClicked() = 0;
	virtual LogAreaResizingEvent::Subscriber GetLogAreaResizingEvent() = 0;
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
	virtual Port* GetReceivingPort (Port* txPort) const = 0;
};

using ProjectFactory = ComPtr<IProject>(*const)();
extern const ProjectFactory projectFactory;

// ============================================================================

unsigned int GetTimestampMilliseconds();
D2D1::ColorF GetD2DSystemColor (int sysColorIndex);
bool HitTestLine (const IZoomable* zoomable, D2D1_POINT_2F dLocation, float tolerance, D2D1_POINT_2F p0w, D2D1_POINT_2F p1w, float lineWidth);
bool PointInPolygon (const D2D1_POINT_2F* vertices, size_t vertexCount, D2D1_POINT_2F point);
