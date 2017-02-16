
#include "pch.h"
#include "SimulatorDefs.h"
#include "ZoomableWindow.h"
#include "Ribbon/RibbonIds.h"
#include "EditStates/EditState.h"

using namespace std;
using namespace D2D1;

class EditArea : public ZoomableWindow, public IEditArea
{
	typedef ZoomableWindow base;

	ULONG _refCount = 1;
	IProjectWindow* const _pw;
	IUIFramework* const _rf;
	ComPtr<ISelection> const _selection;
	ComPtr<IProject> const _project;
	ComPtr<IDWriteFactory> const _dWriteFactory;
	ComPtr<ID2D1SolidColorBrush> _poweredBrush;
	ComPtr<ID2D1SolidColorBrush> _unpoweredBrush;
	ComPtr<ID2D1SolidColorBrush> _brushWindowText;
	ComPtr<ID2D1SolidColorBrush> _brushWindow;
	ComPtr<ID2D1SolidColorBrush> _brushHighlight;
	ComPtr<ID2D1SolidColorBrush> _brushDiscardingPort;
	ComPtr<ID2D1SolidColorBrush> _brushLearningPort;
	ComPtr<ID2D1SolidColorBrush> _brushForwardingPort;
	ComPtr<ID2D1SolidColorBrush> _brushForwardingWire;
	ComPtr<ID2D1SolidColorBrush> _brushNoForwardingWire;
	ComPtr<ID2D1SolidColorBrush> _brushTempWire;
	ComPtr<ID2D1StrokeStyle> _strokeStyleNoForwardingWire;
	ComPtr<IDWriteTextFormat> _regularTextFormat;
	unsigned int _selectedVlanNumber = 1;
	unique_ptr<EditState> _state;

	struct BeginningDrag
	{
		POINT pt;
		D2D1_POINT_2F dLocation;
		D2D1_POINT_2F wLocation;
		MouseButton button;
		Object* clickedObj;
	};

	optional<BeginningDrag> _beginningDrag;

public:
	EditArea(IProject* project, IProjectWindow* pw, ISelection* selection, IUIFramework* rf, const RECT& rect, ID3D11DeviceContext1* deviceContext, IDWriteFactory* dWriteFactory, IWICImagingFactory2* wicFactory)
		: base(0, WS_CHILD | WS_VISIBLE, rect, pw->GetHWnd(), 55, deviceContext, dWriteFactory, wicFactory)
		, _project(project), _pw(pw), _rf(rf), _selection(selection), _dWriteFactory(dWriteFactory)
	{
		_selection->GetSelectionChangedEvent().AddHandler (&OnSelectionChanged, this);
		_project->GetProjectInvalidateEvent().AddHandler (&OnProjectInvalidate, this);
		auto dc = base::GetDeviceContext();
		auto hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green), &_poweredBrush); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_unpoweredBrush); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Red), &_brushDiscardingPort); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Yellow), &_brushLearningPort); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green), &_brushForwardingPort); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Green), &_brushForwardingWire); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Gray), &_brushNoForwardingWire); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (ColorF (ColorF::Blue), &_brushTempWire); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOWTEXT), &_brushWindowText); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_WINDOW), &_brushWindow); ThrowIfFailed(hr);
		hr = dc->CreateSolidColorBrush (GetD2DSystemColor (COLOR_HIGHLIGHT), &_brushHighlight); ThrowIfFailed(hr);
		hr = _dWriteFactory->CreateTextFormat (L"Tahoma", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14, L"en-US", &_regularTextFormat); ThrowIfFailed(hr);
	}

	virtual ~EditArea()
	{
		assert (_refCount == 0);
		_project->GetProjectInvalidateEvent().RemoveHandler(&OnProjectInvalidate, this);
		_selection->GetSelectionChangedEvent().RemoveHandler (&OnSelectionChanged, this);
	}

	static void OnProjectInvalidate (void* callbackArg, IProject*)
	{
		auto area = static_cast<EditArea*>(callbackArg);
		::InvalidateRect (area->GetHWnd(), nullptr, FALSE);
	}

	static ColorF GetD2DSystemColor (int sysColorIndex)
	{
		DWORD brg = GetSysColor (sysColorIndex);
		DWORD rgb = ((brg & 0xff0000) >> 16) | (brg & 0xff00) | ((brg & 0xff) << 16);
		return ColorF (rgb);
	}

	static void OnSelectionChanged (void* callbackArg, ISelection* selection)
	{
		auto editArea = static_cast<EditArea*>(callbackArg);
		::InvalidateRect (editArea->GetHWnd(), nullptr, FALSE);
	}

	void RenderExteriorNonStpPort (ID2D1DeviceContext* dc, bool macOperational) const
	{
		auto brush = macOperational ? _brushForwardingPort : _brushDiscardingPort;
		dc->DrawLine (Point2F (0, 0), Point2F (0, PortExteriorHeight), brush);
	}

	void RenderExteriorStpPort (ID2D1DeviceContext* dc, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge) const
	{
		static constexpr float circleDiameter = min (PortExteriorHeight / 2, PortExteriorWidth);

		static constexpr float edw = PortExteriorWidth;
		static constexpr float edh = PortExteriorHeight;

		static constexpr float discardingFirstHorizontalLineY = circleDiameter + (edh - circleDiameter) / 3;
		static constexpr float discardingSecondHorizontalLineY = circleDiameter + (edh - circleDiameter) * 2 / 3;
		static constexpr float learningHorizontalLineY = circleDiameter + (edh - circleDiameter) / 2;

		static constexpr float dfhly = discardingFirstHorizontalLineY;
		static constexpr float dshly = discardingSecondHorizontalLineY;

		static const D2D1_ELLIPSE ellipse = { Point2F (0, circleDiameter / 2), circleDiameter / 2, circleDiameter / 2};

		if (role == STP_PORT_ROLE_DISABLED)
		{
			// disabled
			dc->DrawLine (Point2F (0, 0), Point2F (0, edh), _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, edh / 3), Point2F (edw / 2, edh * 2 / 3), _brushDiscardingPort);
		}
		else if ((role == STP_PORT_ROLE_DESIGNATED) && !learning && !forwarding)
		{
			// designated discarding
			dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), _brushDiscardingPort);
			dc->FillEllipse (&ellipse, _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), _brushDiscardingPort);
		}
		else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && !forwarding)
		{
			// designated learning
			dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), _brushLearningPort);
			dc->FillEllipse (&ellipse, _brushLearningPort);
			dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), _brushLearningPort);
		}
		else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && !operEdge)
		{
			// designated forwarding
			dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), _brushForwardingPort);
			dc->FillEllipse (&ellipse, _brushForwardingPort);
		}
		else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && operEdge)
		{
			// designated forwarding operEdge
			dc->FillEllipse (&ellipse, _brushForwardingPort);
			static constexpr D2D1_POINT_2F points[] = 
			{
				{ 0, circleDiameter },
				{ -edw / 2 + 1, circleDiameter + (edh - circleDiameter) / 2 },
				{ 0, edh },
				{ edw / 2 - 1, circleDiameter + (edh - circleDiameter) / 2 },
			};

			dc->DrawLine (points[0], points[1], _brushForwardingPort);
			dc->DrawLine (points[1], points[2], _brushForwardingPort);
			dc->DrawLine (points[2], points[3], _brushForwardingPort);
			dc->DrawLine (points[3], points[0], _brushForwardingPort);
		}
		else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && !learning && !forwarding)
		{
			// root or master discarding
			dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), _brushDiscardingPort);
			dc->DrawEllipse (&ellipse, _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), _brushDiscardingPort);
		}
		else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && !forwarding)
		{
			// root or master learning
			dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), _brushLearningPort);
			dc->DrawEllipse (&ellipse, _brushLearningPort);
			dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), _brushLearningPort);
		}
		else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && forwarding)
		{
			// root or master forwarding
			dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), _brushForwardingPort);
			dc->DrawEllipse (&ellipse, _brushForwardingPort);
		}
		else if ((role == STP_PORT_ROLE_ALTERNATE) && !learning && !forwarding)
		{
			// Alternate discarding
			dc->DrawLine (Point2F (0, 0), Point2F (0, edh), _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), _brushDiscardingPort);
		}
		else if ((role == STP_PORT_ROLE_ALTERNATE) && learning && !forwarding)
		{
			// Alternate learning
			dc->DrawLine (Point2F (0, 0), Point2F (0, edh), _brushLearningPort);
			dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), _brushLearningPort);
		}
		else if ((role == STP_PORT_ROLE_BACKUP) && !learning && !forwarding)
		{
			// Backup discarding
			dc->DrawLine (Point2F (0, 0), Point2F (0, edh), _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dfhly / 2), Point2F (edw / 2, dfhly / 2), _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), _brushDiscardingPort);
			dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), _brushDiscardingPort);
		}
		else if (role == STP_PORT_ROLE_UNKNOWN)
		{
			// Undefined
			dc->DrawLine (Point2F (0, 0), Point2F (0, edh), _brushDiscardingPort);

			D2D1_RECT_F rect = { 2, 0, 20, 20 };
			dc->DrawText (L"?", 1, _regularTextFormat, &rect, _brushDiscardingPort, D2D1_DRAW_TEXT_OPTIONS_NO_SNAP);
		}
		else
			throw exception("Not implemented.");
	}

	struct LegendInfoEntry
	{
		const wchar_t* text;
		STP_PORT_ROLE role;
		bool learning;
		bool forwarding;
		bool operEdge;
	};

	static constexpr LegendInfoEntry LegendInfo[] =
	{
		{ L"Disabled",							STP_PORT_ROLE_DISABLED,   false, false, false },
		
		{ L"Designated discarding",				STP_PORT_ROLE_DESIGNATED, false, false, false },
		{ L"Designated learning",				STP_PORT_ROLE_DESIGNATED, true,  false, false },
		{ L"Designated forwarding",				STP_PORT_ROLE_DESIGNATED, true,  true,  false },
		{ L"Designated forwarding operEdge",	STP_PORT_ROLE_DESIGNATED, true,  true,  true  },
		
		{ L"Root/Master discarding",			STP_PORT_ROLE_ROOT,       false, false, false },
		{ L"Root/Master learning",				STP_PORT_ROLE_ROOT,       true,  false, false },
		{ L"Root/Master forwarding",			STP_PORT_ROLE_ROOT,       true,  true,  false },
		
		{ L"Alternate discarding",				STP_PORT_ROLE_ALTERNATE,  false, false, false },
		{ L"Alternate learning",				STP_PORT_ROLE_ALTERNATE,  true,  false, false },
		
		{ L"Backup discarding",					STP_PORT_ROLE_BACKUP,     false, false, false },
		{ L"Undefined",							STP_PORT_ROLE_UNKNOWN,    false, false, false },
	};

	void RenderLegend (ID2D1DeviceContext* dc) const
	{
		auto clientRect = GetClientRectDips();

		float maxLineWidth = 0;
		float maxLineHeight = 0;
		vector<ComPtr<IDWriteTextLayout>> layouts;
		for (auto& info : LegendInfo)
		{
			ComPtr<IDWriteTextLayout> tl;
			auto hr = _dWriteFactory->CreateTextLayout (info.text, wcslen(info.text), _regularTextFormat, 1000, 1000, &tl); ThrowIfFailed(hr);

			DWRITE_TEXT_METRICS metrics;
			tl->GetMetrics (&metrics);

			if (metrics.width > maxLineWidth)
				maxLineWidth = metrics.width;

			if (metrics.height > maxLineHeight)
				maxLineHeight = metrics.height;

			layouts.push_back(move(tl));
		}

		float textX = clientRect.right - (5 + maxLineWidth + 5 + PortExteriorHeight + 5);
		float bitmapX = clientRect.right - (5 + PortExteriorHeight + 5);
		float rowHeight = 2 + max (maxLineHeight, PortExteriorWidth);
		float y = clientRect.bottom - _countof(LegendInfo) * rowHeight;

		Matrix3x2F oldTransform;
		dc->GetTransform (&oldTransform);

		for (size_t i = 0; i < _countof(LegendInfo); i++)
		{
			auto& info = LegendInfo[i];

			auto oldaa = dc->GetAntialiasMode();
			dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
			dc->DrawLine (Point2F (textX, y), Point2F (clientRect.right, y), _brushWindowText);
			dc->SetAntialiasMode(oldaa);

			dc->DrawTextLayout (Point2F (textX, y + 1), layouts[i], _brushWindowText);

			// Rotate 270 degrees and then translate.
			Matrix3x2F trans (0.0f, -1.0f, 1.0f, 0.0f, bitmapX, y + rowHeight / 2);
			trans.SetProduct (oldTransform, trans);
			dc->SetTransform (&trans);

			RenderExteriorStpPort (dc, info.role, info.learning, info.forwarding, info.operEdge);

			dc->SetTransform (&oldTransform);

			y += rowHeight;
		}
	}

	void RenderSelectionRectangles (ID2D1DeviceContext* dc) const
	{
		auto oldaa = dc->GetAntialiasMode();
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		for (auto o : _selection->GetObjects())
		{
			if (auto b = dynamic_cast<PhysicalBridge*>(o))
			{
				auto tl = GetDLocationFromWLocation ({ b->GetLeft() - BridgeOutlineWidth / 2, b->GetTop() - BridgeOutlineWidth / 2 });
				auto br = GetDLocationFromWLocation ({ b->GetRight() + BridgeOutlineWidth / 2, b->GetBottom() + BridgeOutlineWidth / 2 });

				ComPtr<ID2D1Factory> factory;
				dc->GetFactory(&factory);
				D2D1_STROKE_STYLE_PROPERTIES ssprops = {};
				ssprops.dashStyle = D2D1_DASH_STYLE_DASH;
				ComPtr<ID2D1StrokeStyle> ss;
				auto hr = factory->CreateStrokeStyle (&ssprops, nullptr, 0, &ss); ThrowIfFailed(hr);
				ComPtr<ID2D1SolidColorBrush> brush;
				hr = dc->CreateSolidColorBrush (ColorF(ColorF::Blue), &brush);
				dc->DrawRectangle ({ tl.x - 10, tl.y - 10, br.x + 10, br.y + 10 }, brush, 1, ss);
			}
		}

		dc->SetAntialiasMode(oldaa);
	}

	void RenderBridge (ID2D1DeviceContext* dc, PhysicalBridge* b) const
	{
		optional<unsigned int> treeIndex;
		if (b->IsStpEnabled())
			treeIndex = b->GetStpTreeIndexFromVlanNumber(_selectedVlanNumber);

		// Draw bridge outline.
		D2D1_RECT_F bridgeRect = b->GetBounds();
		D2D1_ROUNDED_RECT rr = RoundedRect (bridgeRect, BridgeRoundRadius, BridgeRoundRadius);
		auto outlineBrush = b->IsPowered() ? _poweredBrush.Get() : _unpoweredBrush.Get();
		outlineBrush->SetOpacity(0.25f);
		dc->FillRoundedRectangle (&rr, outlineBrush);
		outlineBrush->SetOpacity(1);
		dc->DrawRoundedRectangle (&rr, outlineBrush, 2.0f);

		// Draw bridge name.
		auto address = b->GetMacAddress();
		wchar_t str[128];
		int strlen;
		if (b->IsStpEnabled())
		{
			unsigned short prio = b->GetStpBridgePriority(treeIndex.value());
			strlen = swprintf_s (str, L"%04x.%02x%02x%02x%02x%02x%02x\r\nSTP enabled", prio,
				address[0], address[1], address[2], address[3], address[4], address[5]);
		}
		else
		{
			strlen = swprintf_s (str, L"%02x%02x%02x%02x%02x%02x\r\nSTP disabled (right-click to enable)",
				address[0], address[1], address[2], address[3], address[4], address[5]);
		}
		ComPtr<IDWriteTextLayout> tl;
		HRESULT hr = _dWriteFactory->CreateTextLayout (str, strlen, _regularTextFormat, 10000, 10000, &tl); ThrowIfFailed(hr);
		dc->DrawTextLayout ({ b->GetLeft() + BridgeOutlineWidth / 2 + 3, b->GetTop() + BridgeOutlineWidth / 2 + 3}, tl, _brushWindowText);

		Matrix3x2F oldTransform;
		dc->GetTransform (&oldTransform);

		for (unsigned int portIndex = 0; portIndex < b->GetPortCount(); portIndex++)
		{
			PhysicalPort* port = b->GetPort(portIndex);

			Matrix3x2F portTransform;
			if (port->GetSide() == Side::Left)
			{
				//portTransform = Matrix3x2F::Rotation (90, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left, bridgeRect.top + port->GetOffset ());
				// The above calculation is correct but slow. Let's assign the matrix members directly.
				portTransform._11 = 0;
				portTransform._12 = 1;
				portTransform._21 = -1;
				portTransform._22 = 0;
				portTransform._31 = bridgeRect.left;
				portTransform._32 = bridgeRect.top + port->GetOffset();
			}
			else if (port->GetSide() == Side::Right)
			{
				//portTransform = Matrix3x2F::Rotation (270, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.right, bridgeRect.top + port->GetOffset ());
				portTransform._11 = 0;
				portTransform._12 = -1;
				portTransform._21 = 1;
				portTransform._22 = 0;
				portTransform._31 = bridgeRect.right;
				portTransform._32 = bridgeRect.top + port->GetOffset();
			}
			else if (port->GetSide() == Side::Top)
			{
				//portTransform = Matrix3x2F::Rotation (180, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.top);
				portTransform._11 = -1;
				portTransform._12 = 0;
				portTransform._21 = 0;
				portTransform._22 = -1;
				portTransform._31 = bridgeRect.left + port->GetOffset();
				portTransform._32 = bridgeRect.top;
			}
			else if (port->GetSide() == Side::Bottom)
			{
				//portTransform = Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.bottom);
				portTransform._11 = portTransform._22 = 1;
				portTransform._12 = portTransform._21 = 0;
				portTransform._31 = bridgeRect.left + port->GetOffset();
				portTransform._32 = bridgeRect.bottom;
			}
			else
				throw exception("Not implemented.");

			portTransform.SetProduct (portTransform, oldTransform);
			dc->SetTransform (&portTransform);
			
			// Draw the interior of the port.
			D2D1_RECT_F portRect = RectF (
				-PortInteriorLongSize / 2,
				-PortInteriorShortSize,
				-PortInteriorLongSize / 2 + PortInteriorLongSize,
				-PortInteriorShortSize + PortInteriorShortSize);
			outlineBrush = port->GetMacOperational() ? _poweredBrush : _unpoweredBrush;
			dc->FillRectangle (&portRect, _brushWindow);
			dc->DrawRectangle (&portRect, outlineBrush);

			// Draw the exterior of the port.
			if (b->IsStpEnabled())
			{
				STP_PORT_ROLE role = b->GetStpPortRole (portIndex, treeIndex.value());
				bool learning      = b->GetStpPortLearning (portIndex, treeIndex.value());
				bool forwarding    = b->GetStpPortForwarding (portIndex, treeIndex.value());
				bool operEdge      = b->GetStpPortOperEdge (portIndex);
				RenderExteriorStpPort (dc, role, learning, forwarding, operEdge);
			}
			else
				RenderExteriorNonStpPort(dc, port->GetMacOperational());

			// fill the gray/green circle representing the operational state of the port.
			auto fillBrush = port->GetMacOperational() ? _poweredBrush : _unpoweredBrush;
			float radius = 4;
			D2D1_POINT_2F circleCenter = Point2F (-PortInteriorLongSize / 2 + 2 + radius, -PortInteriorShortSize + 2 + radius);
			D2D1_ELLIPSE circle = Ellipse (circleCenter, radius, radius);
			dc->FillEllipse (&circle, fillBrush);

			dc->SetTransform (&oldTransform);
		}
	}
	/*
	void RenderWire (Wire* wire, unsigned short selectedVlanNumber)
	{
		ID2D1SolidColorBrush* brush = _brushNoForwardingWire;
		ID2D1StrokeStyle* strokeStyle = _strokeStyleNoForwardingWire;

		// If both ends are forwarding, make the wire thick.
		ConnectedWireEnd* firstConnectedEnd  = dynamic_cast<ConnectedWireEnd*> (wire->GetFirstEnd  ());
		ConnectedWireEnd* secondConnectedEnd = dynamic_cast<ConnectedWireEnd*> (wire->GetSecondEnd ());
		if ((firstConnectedEnd != NULL) && (secondConnectedEnd != NULL)
			&& firstConnectedEnd->Port->GetMacOperational () && secondConnectedEnd->Port->GetMacOperational ())
		{
			STP_BRIDGE* stpBridge = firstConnectedEnd->Port->Bridge->LockStpBridge ();
			byte oneEndTreeIndex = STP_GetTreeIndexFromVlanNumber (stpBridge, selectedVlanNumber);
			bool oneForwarding = STP_GetPortForwarding (stpBridge, firstConnectedEnd->Port->PortIndex, oneEndTreeIndex);
			firstConnectedEnd->Port->Bridge->UnlockStpBridge ();

			if (oneForwarding)
			{
				stpBridge = secondConnectedEnd->Port->Bridge->LockStpBridge ();
				byte otherEndTreeIndex = STP_GetTreeIndexFromVlanNumber (stpBridge, selectedVlanNumber);
				bool otherForwarding = STP_GetPortForwarding (stpBridge, secondConnectedEnd->Port->PortIndex, otherEndTreeIndex);
				secondConnectedEnd->Port->Bridge->UnlockStpBridge ();

				if (otherForwarding)
				{
					brush = _brushForwardingWire;
					strokeStyle = NULL;
				}
			}
		}

		_renderTarget->DrawLine (wire->GetFirstEnd ()->GetLocation (), wire->GetSecondEnd ()->GetLocation (), brush, 2.0f, strokeStyle);
	}
	*/
	void RenderBridges (ID2D1DeviceContext* dc) const
	{
		D2D1_MATRIX_3X2_F oldtr;
		dc->GetTransform(&oldtr);
		dc->SetTransform(GetZoomTransform());
		for (auto& b : _project->GetBridges())
			RenderBridge (dc, b);
		dc->SetTransform(oldtr);
	}

	virtual void Render(ID2D1DeviceContext* dc) const override final
	{
		auto backGdiColor = GetSysColor(COLOR_WINDOW);
		auto backColor = D2D1_COLOR_F{ (backGdiColor & 0xff) / 255.0f, ((backGdiColor >> 8) & 0xff) / 255.0f, ((backGdiColor >> 16) & 0xff) / 255.0f, 1.0f };

		auto textGdiColor = GetSysColor(COLOR_WINDOWTEXT);
		auto textColor = D2D1_COLOR_F{ (textGdiColor & 0xff) / 255.0f, ((textGdiColor >> 8) & 0xff) / 255.0f, ((textGdiColor >> 16) & 0xff) / 255.0f, 1.0f };

		dc->Clear(backColor);

		auto clientRectDips = GetClientRectDips();

		HRESULT hr;

		if (_project->GetBridges().empty())
		{
			auto text = L"No bridges created. Right-click to create some.";
			ComPtr<IDWriteTextFormat> tf;
			hr = _dWriteFactory->CreateTextFormat (L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 16, L"en-US", &tf); ThrowIfFailed(hr);
			ComPtr<IDWriteTextLayout> tl;
			hr = _dWriteFactory->CreateTextLayout (text, wcslen(text), tf, 10000, 10000, &tl); ThrowIfFailed(hr);
			DWRITE_TEXT_METRICS metrics;
			hr = tl->GetMetrics(&metrics); ThrowIfFailed(hr);
			D2D1_POINT_2F origin = { clientRectDips.right / 2 - metrics.width / 2, clientRectDips.bottom / 2 - metrics.height / 2 };
			ComPtr<ID2D1SolidColorBrush> brush;
			dc->CreateSolidColorBrush (textColor, &brush);
			dc->DrawTextLayout (origin, tl, brush);
		}
		else
		{
			RenderBridges(dc);
			RenderLegend(dc);
			RenderSelectionRectangles(dc);
		}
	}

	virtual HWND GetHWnd() const override final { return base::GetHWnd(); }

	virtual std::optional<LRESULT> WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) override
	{
		if ((uMsg == WM_LBUTTONDOWN) || (uMsg == WM_RBUTTONDOWN))
		{
			auto button = (uMsg == WM_LBUTTONDOWN) ? MouseButton::Left : MouseButton::Right;
			auto result = ProcessMouseButtonDown (button, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return result ? result.value() : base::WindowProc (hwnd, uMsg, wParam, lParam);
		}
		else if (uMsg == WM_MOUSEMOVE)
		{
			ProcessWmMouseMove (POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			base::WindowProc (hwnd, uMsg, wParam, lParam);
			return 0;
		}
		else if ((uMsg == WM_LBUTTONUP) || (uMsg == WM_RBUTTONUP))
		{
			auto button = (uMsg == WM_LBUTTONUP) ? MouseButton::Left : MouseButton::Right;
			auto result = ProcessMouseButtonUp (button, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });
			return result ? result.value() : base::WindowProc (hwnd, uMsg, wParam, lParam);
		}

		if (uMsg == WM_CONTEXTMENU)
			return ProcessWmContextMenu (hwnd, POINT{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) });

		return base::WindowProc (hwnd, uMsg, wParam, lParam);
	}

	Object* GetObjectAt (float x, float y) const
	{
		for (auto& b : _project->GetBridges())
		{
			if ((x >= b->GetLeft()) && (x < b->GetRight()) && (y >= b->GetTop()) && (y < b->GetBottom()))
			{
				return b;
			}
		}

		return nullptr;
	};

	std::optional<LRESULT> ProcessMouseButtonDown (MouseButton button, POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		auto clickedObject = GetObjectAt(wLocation.x, wLocation.y);

		if (_state != nullptr)
		{
			_state->OnMouseDown (dLocation, wLocation, button, clickedObject);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};
			
			return 0;
		}

		if (clickedObject == nullptr)
			_selection->Clear();
		else
			_selection->Select(clickedObject);

		if (!_beginningDrag)
		{
			_beginningDrag = BeginningDrag { pt, dLocation, wLocation, button, clickedObject };
			return 0;
		}

		return nullopt;
	}

	std::optional<LRESULT> ProcessMouseButtonUp (MouseButton button, POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_state != nullptr)
		{
			_state->OnMouseUp (dLocation, wLocation, button);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			};

			return 0;
		}

		if (_beginningDrag && (button == _beginningDrag->button))
			_beginningDrag = nullopt;

		if (button == MouseButton::Right)
			return nullopt; // return "not handled", to cause our called to pass the message to DefWindowProc, which will generate WM_CONTEXTMENU

		return 0;
	}

	void ProcessWmMouseMove (POINT pt)
	{
		auto dLocation = GetDipLocationFromPixelLocation(pt);
		auto wLocation = GetWLocationFromDLocation(dLocation);

		if (_beginningDrag)
		{
			RECT rc = { _beginningDrag->pt.x, _beginningDrag->pt.y, _beginningDrag->pt.x, _beginningDrag->pt.y };
			InflateRect (&rc, GetSystemMetrics(SM_CXDRAG), GetSystemMetrics(SM_CYDRAG));
			if (!PtInRect (&rc, pt))
			{
				if (_beginningDrag->clickedObj == nullptr)
				{
					// TODO: area selection
				}
				else if (auto b = dynamic_cast<PhysicalBridge*>(_beginningDrag->clickedObj))
				{
					if (_beginningDrag->button == MouseButton::Left)
						_state = CreateStateMoveBridges (this, _selection);
				}
				else if (auto b = dynamic_cast<PhysicalPort*>(_beginningDrag->clickedObj))
				{
					// TODO: move port
					//if (_beginningDrag->button == MouseButton::Left)
					//	_state = CreateStateMovePorts (this, _selection);
				}
				else
					throw exception("Not implemented.");

				if (_state != nullptr)
				{
					_state->OnMouseDown (_beginningDrag->dLocation, _beginningDrag->wLocation, _beginningDrag->button, _beginningDrag->clickedObj);
					assert (!_state->Completed());
					_state->OnMouseMove (dLocation, wLocation);
				}

				_beginningDrag = nullopt;
			}
		}
		else if (_state != nullptr)
		{
			_state->OnMouseMove (dLocation, wLocation);
			if (_state->Completed())
			{
				_state = nullptr;
				::SetCursor (LoadCursor (nullptr, IDC_ARROW));
			}
		}
	}

	std::optional<LRESULT> ProcessWmContextMenu (HWND hwnd, POINT pt)
	{
		//D2D1_POINT_2F dipLocation = GetDipLocationFromPixelLocation(pt);
		//_elementsAtContextMenuLocation.clear();
		//GetElementsAt(_project->GetInnerRootElement(), { dipLocation.x, dipLocation.y }, _elementsAtContextMenuLocation);

		UINT32 viewId;
		if (_selection->GetObjects().empty())
			viewId = cmdContextMenuBlankArea;
		else if (dynamic_cast<PhysicalBridge*>(_selection->GetObjects()[0]) != nullptr)
			viewId = cmdContextMenuBridge;
		else if (dynamic_cast<PhysicalPort*>(_selection->GetObjects()[0]) != nullptr)
			viewId = cmdContextMenuPort;
		else
			throw exception("Not implemented.");

		ComPtr<IUIContextualUI> ui;
		auto hr = _rf->GetView(viewId, IID_PPV_ARGS(&ui)); ThrowIfFailed(hr);
		hr = ui->ShowAtLocation(pt.x, pt.y); ThrowIfFailed(hr);
		return 0;
	}

	virtual void SelectVlan (unsigned int vlanNumber) override final
	{
		if (_selectedVlanNumber != vlanNumber)
		{
			_selectedVlanNumber = vlanNumber;
			::InvalidateRect (GetHWnd(), nullptr, FALSE);
		}
	};

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override final { throw exception("Not implemented."); }

	virtual ULONG STDMETHODCALLTYPE AddRef() override final
	{
		return InterlockedIncrement(&_refCount);
	}

	virtual ULONG STDMETHODCALLTYPE Release() override final
	{
		auto newRefCount = InterlockedDecrement(&_refCount);
		if (newRefCount == 0)
			delete this;
		return newRefCount;
	}
	#pragma endregion
};

extern const EditAreaFactory editAreaFactory = [](auto... params) { return ComPtr<IEditArea>(new EditArea(params...), false); };
