
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "Simulator.h"
#include "edge/Z80Xml.h"

using namespace D2D1;
using namespace edge;

class PortImpl : public IPort, IPortProperties, IConnectionPointContainer, IXmlParent, IStpPropertyChangeSink
{
	ULONG _refCount = 0;
	WeakRefToThis _weakRefToThis;
	IBridge* _parent;
	uint32_t _port_index;
	PortSide _side = PortSide::Bottom;
	LONG _offset;
	uint32_t _supported_speed = 100;
	uint32_t _actualSpeed = 0;
	vector_nothrow<com_ptr<IPortTree>> _trees;
	com_ptr<ConnectionPointImpl<IPropertyChangeSink>> _propChangeCP;
	com_ptr<ConnectionPointImpl<IInvalidateSink>> _invalidateCP;
	AdviseSinkToken _stpPropertyChangedToken;

public:
	HRESULT InitInstance (IBridge* parent, uint32_t port_index, PortSide side, LONG offset)
	{
		auto hr = _weakRefToThis.InitInstance(AsUnknown()); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint<IPropertyChangeSink>(this, &_propChangeCP); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint<IInvalidateSink>(this, &_invalidateCP); RETURN_IF_FAILED(hr);

		_parent = parent;
		_port_index = port_index;
		_side = side;
		_offset = offset;
		uint32_t tree_count = (uint32_t)parent->TreeCount();
		bool reserved = _trees.try_reserve(tree_count); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);
		for (uint32_t treeIndex = 0; treeIndex < tree_count; treeIndex++)
		{
			com_ptr<IPortTree> tree;
			auto hr = MakePortTree(this, treeIndex, &tree); RETURN_IF_FAILED(hr);
			_trees.try_push_back(std::move(tree));
		}

		hr = AdviseSink<IStpPropertyChangeSink>(parent, _weakRefToThis, &_stpPropertyChangedToken); RETURN_IF_FAILED(hr);

		return S_OK;
	}

	IUnknown* AsUnknown() { return static_cast<IPort*>(this); }

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IPort>(this, riid, ppvObject)
			|| TryQI<IPortProperties>(this, riid, ppvObject)
			|| TryQI<IDispatch>(static_cast<IPortProperties*>(this), riid, ppvObject)
			|| TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<ISelectableObject>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<IXmlParent>(this, riid, ppvObject)
			|| TryQI<IStpPropertyChangeSink>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH(IPortProperties);

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(IPropertyChangeSink))
			return wil::com_query_to_nothrow(_propChangeCP, ppCP);
		if (riid == __uuidof(IInvalidateSink))
			return wil::com_query_to_nothrow(_invalidateCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	#pragma region IPortProperties
	virtual HRESULT STDMETHODCALLTYPE get_Name (BSTR *pName) override
	{
		*pName = SysAllocString(L"Port"); RETURN_IF_NULL_ALLOC(*pName);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_ClassName (BSTR *pClassName) override
	{
		*pClassName = SysAllocString(L"Port"); RETURN_IF_NULL_ALLOC(*pClassName);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_Side (enum PortSide *pSide) override
	{
		*pSide = _side;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_Side (enum PortSide side) override
	{
		if (_side != side)
		{
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidPortSide);
			NotifyInvalidate(_invalidateCP, nullptr);
			_side = side;
			NotifyInvalidate(_invalidateCP, nullptr);
			NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidPortSide);
		}
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_Offset (LONG* pOffset) override
	{
		*pOffset = _offset;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_Offset (LONG offset) override
	{
		if (_offset != offset)
		{
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidPortOffset);
			NotifyInvalidate(_invalidateCP, nullptr);
			_offset = offset;
			NotifyInvalidate(_invalidateCP, nullptr);
			NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidPortOffset);
		}
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_SupportedSpeed (enum PortSpeed* pSpeed) override
	{
		*pSpeed = (enum PortSpeed)_supported_speed;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_SupportedSpeed (enum PortSpeed speed) override
	{
		if (_supported_speed != (uint32_t)speed)
		{
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidSupportedSpeed);
			_supported_speed = (uint32_t)speed;
			NotifyPropertyChanged  (_propChangeCP, AsUnknown(), dispidSupportedSpeed);
		};

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_ActualSpeed (DWORD *pSpeed) override
	{
		*pSpeed = _actualSpeed;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_AutoEdge (VARIANT_BOOL *pbAutoEdge) override
	{
		bool b = STP_GetPortAutoEdge (bridge()->stp_bridge(), _port_index);
		*pbAutoEdge = b ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_AutoEdge (VARIANT_BOOL bAutoEdge) override
	{
		//	STP_SetPortAutoEdge (bridge()->stp_bridge(), _port_index, autoEdge, (unsigned int) GetMessageTime());
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE get_AdminEdge (VARIANT_BOOL *pbAdminEdge) override
	{
		bool b = STP_GetPortAdminEdge (bridge()->stp_bridge(), _port_index);
		*pbAdminEdge = b ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_AdminEdge (VARIANT_BOOL bAdminEdge) override
	{
		STP_BRIDGE* b = bridge()->stp_bridge();
		STP_SetPortAdminEdge (b, _port_index, bAdminEdge, (unsigned int) GetMessageTime());
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_operEdge (VARIANT_BOOL* pboperEdge) override
	{
		if (!STP_IsBridgeStarted(_parent->stp_bridge()))
			return SetErrorInfoStpDisabled();
		bool b = STP_GetPortOperEdge (_parent->stp_bridge(), _port_index);
		*pboperEdge = b ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_MAC_Operational (VARIANT_BOOL *pbMAC_Operational) override
	{
		auto enabled = STP_GetPortEnabled(bridge()->stp_bridge(), _port_index);
		*pbMAC_Operational = enabled ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_DetectedPortPathCost (DWORD* pDetectedPortPathCost) override
	{
		*pDetectedPortPathCost = STP_GetDetectedPortPathCost(bridge()->stp_bridge(), _port_index);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_AdminExternalPortPathCost (DWORD* pdwAdminExternalPortPathCost) override
	{
		*pdwAdminExternalPortPathCost = STP_GetAdminExternalPortPathCost (bridge()->stp_bridge(), _port_index);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_AdminExternalPortPathCost (DWORD dwAdminExternalPortPathCost) override
	{
		RETURN_HR(E_NOTIMPL);
	//	//edge::property_changing_e::invoker(_em).invoke(this, value_property_change_args(admin_external_port_path_cost_property));
	//	//edge::property_changing_e::invoker(_em).invoke(this, value_property_change_args(external_port_path_cost_property));
	//	STP_SetAdminExternalPortPathCost (bridge()->stp_bridge(), (unsigned int)_port_index, adminExternalPortPathCost, GetMessageTime());
	//	//edge::property_changed_e::invoker(_em).invoke(this, value_property_change_args(external_port_path_cost_property));
	//	//edge::property_changed_e::invoker(_em).invoke(this, value_property_change_args(admin_external_port_path_cost_property));
	}

	virtual HRESULT STDMETHODCALLTYPE get_ExternalPortPathCost (DWORD* pExternalPortPathCost) override
	{
		*pExternalPortPathCost = STP_GetExternalPortPathCost (bridge()->stp_bridge(), _port_index);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_detectedPointToPointMAC (VARIANT_BOOL* pbDetectedPointToPointMAC) override
	{
		bool detected = STP_GetDetectedPointToPointMAC(bridge()->stp_bridge(), _port_index);
		*pbDetectedPointToPointMAC = detected ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_adminPointToPointMAC (enum AdminPointToPoint* pAdminP2P) override
	{
		*pAdminP2P = (AdminPointToPoint)STP_GetAdminPointToPointMAC(bridge()->stp_bridge(), (unsigned int)_port_index);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE put_adminPointToPointMAC (enum AdminPointToPoint adminP2P) override
	{
		STP_SetAdminPointToPointMAC (bridge()->stp_bridge(), _port_index, (STP_ADMIN_P2P)adminP2P, ::GetMessageTime());
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_operPointToPointMAC (VARIANT_BOOL* pbOperPointToPointMAC) override
	{
		bool b = STP_GetOperPointToPointMAC(bridge()->stp_bridge(), _port_index);
		*pbOperPointToPointMAC = b ? VARIANT_TRUE : VARIANT_FALSE;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_Trees (SAFEARRAY * *ppsaItems) override
	{
		return GetItems (_trees.size(), [this](ULONG i, IDispatch** ppDisp) -> HRESULT {
			return _trees[i]->QueryInterface(ppDisp);
		}, ppsaItems);
	}
	#pragma endregion

	#pragma region IXmlParent
	virtual HRESULT STDMETHODCALLTYPE GetChildSerializeInfo (
		_In_ DISPID dispidProperty,
		_In_ IDispatch* pChild,
		_Outptr_ BSTR* pbstrXmlElementName,
		_Outptr_opt_result_buffer_(*pcFactoryDispids) DISPID** ppFactoryDispids,
		_Out_ ULONG* pcFactoryDispids) override
	{
		if (dispidProperty == dispidPortTrees)
		{
			*pbstrXmlElementName = SysAllocString(L"Tree"); RETURN_IF_NULL_ALLOC(*pbstrXmlElementName);
			return S_OK;
		}
		else
			RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE GetChildDeserializeInfo (
		_In_ DISPID dispidProperty,
		_In_ PCWSTR xmlElementName,
		_Outptr_ ITypeInfo** ppTypeInfo,
		_Outptr_result_buffer_(*pcFactoryDispids) DISPID** ppFactoryDispids,
		_Out_ ULONG* pcFactoryDispids) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE CreateChild (
		_In_ DISPID dispidProperty,
		_In_ PCWSTR xmlElementName,
		_In_reads_(factoryPropCount) const VARIANT* factoryPropValues,
		_In_ ULONG factoryPropCount,
		_Outptr_ IDispatch** childOut) override
	{
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	virtual IBridge* bridge() const override
	{
		return _parent;
	}

	virtual uint32_t port_index() const override { return _port_index; }
	
	virtual POINT GetCPLocation() const override
	{
		if (_side == PortSide::Left)
			return { bridge()->left() - PortExteriorHeight, bridge()->top() + _offset };

		if (_side == PortSide::Right)
			return { bridge()->right() + PortExteriorHeight, bridge()->top() + _offset };

		if (_side == PortSide::Top)
			return { bridge()->left() + _offset, bridge()->top() - PortExteriorHeight };

		// _side == PortSide::Bottom
		return { bridge()->left() + _offset, bridge()->bottom() + PortExteriorHeight };
	}

	Matrix3x2F GetPortTransform() const
	{
		if (_side == PortSide::Left)
		{
			//portTransform = Matrix3x2F::Rotation (90, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left, bridgeRect.top + port->GetOffset ());
			// The above calculation is correct but slow. Let's assign the matrix members directly.
			return { 0, 1, -1, 0, (float)bridge()->left(), (float)(bridge()->top() + _offset) };
		}
		else if (_side == PortSide::Right)
		{
			//portTransform = Matrix3x2F::Rotation (270, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.right, bridgeRect.top + port->GetOffset ());
			return { 0, -1, 1, 0, (float)bridge()->right(), (float)(bridge()->top() + _offset) };
		}
		else if (_side == PortSide::Top)
		{
			//portTransform = Matrix3x2F::Rotation (180, Point2F (0, 0)) * Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.top);
			return { -1, 0, 0, -1, (float)(bridge()->left() + _offset), (float)bridge()->top() };
		}
		else //if (_side == PortSide::bottom)
		{
			//portTransform = Matrix3x2F::Translation (bridgeRect.left + port->GetOffset (), bridgeRect.bottom);
			return { 1, 0, 0, 1, (float)(bridge()->left() + _offset), (float)bridge()->bottom() };
		}
	}

	virtual HRESULT STDMETHODCALLTYPE Render (ID2D1RenderTarget* rt, const drawing_resources& dos, unsigned int vlanNumber) const noexcept override
	{
		HRESULT hr;

		D2D1_MATRIX_3X2_F oldtr;
		rt->GetTransform(&oldtr);
		rt->SetTransform (GetPortTransform() * oldtr);

		// Draw the exterior of the port.
		float interiorPortOutlineWidth = PortOutlineWidth;
		auto b = bridge()->stp_bridge();
		auto treeIndex = STP_GetTreeIndexFromVlanNumber(b, vlanNumber);
		if (STP_IsBridgeStarted (b))
		{
			auto role       = STP_GetPortRole (b, (unsigned int)_port_index, treeIndex);
			auto learning   = STP_GetPortLearning (b, (unsigned int)_port_index, treeIndex);
			auto forwarding = STP_GetPortForwarding (b, (unsigned int)_port_index, treeIndex);
			auto operEdge   = STP_GetPortOperEdge (b, (unsigned int)_port_index);
			RenderExteriorStpPort (rt, dos, role, learning, forwarding, operEdge);

			if (STP_GetTxCount(b, (unsigned int)_port_index) == STP_GetTxHoldCount(b))
			{
				TextLayoutWithMetrics layout;
				hr = CreateTextLayoutWithMetrics(dos._dWriteFactory, dos._smallTextFormat, L"txCount=TxHoldCount", -1, 0, layout); RETURN_IF_FAILED(hr);
				D2D1_MATRIX_3X2_F old;
				rt->GetTransform(&old);
				rt->SetTransform (Matrix3x2F::Rotation(-90) * old);
				rt->DrawTextLayout ({ -layout.metrics.width - 3, 2 }, layout, dos._brushWindowText);
				rt->SetTransform(&old);
			}

			if (role == STP_PORT_ROLE_ROOT)
				interiorPortOutlineWidth *= 2;
		}
		else
			RenderExteriorNonStpPort(rt, dos, mac_operational());

		// Draw the interior of the port.
		auto portRect = D2D1_RECT_F { -PortInteriorWidth / 2, -PortInteriorDepth, PortInteriorWidth / 2, 0 };
		edge::inflate (&portRect, -interiorPortOutlineWidth / 2);
		rt->FillRectangle (&portRect, mac_operational() ? dos._poweredFillBrush : dos._unpoweredBrush);
		rt->DrawRectangle (&portRect, dos._brushWindowText, interiorPortOutlineWidth);

		std::wstringstream ss;
		ss << std::setfill(L'0') << std::setw(4) << std::hex << STP_GetPortIdentifier(b, (unsigned int)_port_index, treeIndex);
		TextLayoutWithMetrics layout;
		hr = CreateTextLayoutWithMetrics(dos._dWriteFactory, dos._smallTextFormat, ss.str().c_str(), -1, 0, layout); RETURN_IF_FAILED(hr);
		DWRITE_LINE_METRICS lineMetrics;
		UINT32 actualLineCount;
		hr = layout->GetLineMetrics(&lineMetrics, 1, &actualLineCount); RETURN_IF_FAILED(hr);
		rt->DrawTextLayout ({ -layout.metrics.width / 2, -lineMetrics.baseline - PortOutlineWidth * 2 - 1}, layout, dos._brushWindowText);

		if (_trees[treeIndex]->fdb_flush_text_visible())
		{
			hr = CreateTextLayoutWithMetrics(dos._dWriteFactory, dos._smallBoldTextFormat, L"Flush", -1, 0, layout); RETURN_IF_FAILED(hr);
			hr = layout->GetLineMetrics(&lineMetrics, 1, &actualLineCount); RETURN_IF_FAILED(hr);
			D2D1_POINT_2F l = { -layout.metrics.width / 2, -PortInteriorDepth - lineMetrics.baseline - lineMetrics.height * 0.2f };
			rt->DrawTextLayout (l, layout, dos._brushWindowText);
		}

		rt->SetTransform (&oldtr);

		return S_OK;
	}

	D2D1_RECT_F GetInnerOuterRect() const
	{
		auto tl = D2D1_POINT_2F { -PortInteriorWidth / 2, -PortInteriorDepth };
		auto br = D2D1_POINT_2F { PortInteriorWidth / 2, PortExteriorHeight };
		auto tr = GetPortTransform();
		tl = tr.TransformPoint(tl);
		br = tr.TransformPoint(br);
		return { std::min(tl.x, br.x), std::min (tl.y, br.y), std::max(tl.x, br.x), std::max(tl.y, br.y) };
	}

	virtual void render_selection (ID2D1DeviceContext* rt, const IZoomer* zoomer, const drawing_resources& dos) const override
	{
		auto ir = GetInnerOuterRect();

		auto oldaa = rt->GetAntialiasMode();
		rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

		auto lt = zoomer->pointw_to_pointd ({ ir.left, ir.top });
		auto rb = zoomer->pointw_to_pointd ({ ir.right, ir.bottom });
		rt->DrawRectangle ({ lt.x - 10, lt.y - 10, rb.x + 10, rb.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

		rt->SetAntialiasMode(oldaa);
	}

	bool HitTestCP (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance) const
	{
		auto cpWLocation = GetCPLocation();
		auto cpDLocation = wtr.TransformPoint({ (float)cpWLocation.x, (float)cpWLocation.y });

		return (abs (cpDLocation.x - dLocation.x) <= tolerance)
			&& (abs (cpDLocation.y - dLocation.y) <= tolerance);
	}

	bool HitTestInnerOuter (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance) const
	{
		auto ir = GetInnerOuterRect();
		auto lt = wtr.TransformPoint ({ ir.left, ir.top });
		auto rb = wtr.TransformPoint ({ ir.right, ir.bottom });
		return (dLocation.x >= lt.x) && (dLocation.y >= lt.y) && (dLocation.x < rb.x) && (dLocation.y < rb.y);
	}

	virtual std::optional<int> hit_test (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance) override
	{
		if (HitTestCP (wtr, dLocation, tolerance))
			return HTCodeCP;

		if (HitTestInnerOuter (wtr, dLocation, tolerance))
			return HTCodeInnerOuter;

		return { };
	}

	virtual RECT extent() const noexcept override { _ASSERT(false); return { }; }

	virtual bool IsForwarding (unsigned int vlanNumber) const override
	{
		auto stpb = bridge()->stp_bridge();
		if (!STP_IsBridgeStarted(stpb))
			return true;

		auto treeIndex = STP_GetTreeIndexFromVlanNumber(stpb, vlanNumber);
		return STP_GetPortForwarding (stpb, (unsigned int)_port_index, treeIndex);
	}

	virtual void Move (POINT proposedLocation) override
	{
		LONG mouseX = (proposedLocation.x - _parent->x());
		LONG mouseY = (proposedLocation.y - _parent->y());
		LONG _width = _parent->width();
		LONG _height = _parent->height();
		float wh = (float)_parent->width() / _parent->height();

		PortSide side;
		LONG offset;

		// top side
		if ((mouseX > mouseY * wh) && (_width - mouseX) > mouseY * wh)
		{
			side = PortSide::Top;

			if (mouseX < PortInteriorWidth / 2)
				offset = PortInteriorWidth / 2;
			else if (mouseX > _width - PortInteriorWidth / 2)
				offset = _width - PortInteriorWidth / 2;
			else
				offset = mouseX;
		}

		// bottom side
		else if ((mouseX <= mouseY * wh) && (_width - mouseX) <= mouseY * wh)
		{
			side = PortSide::Bottom;

			if (mouseX < PortInteriorWidth / 2)
				offset = PortInteriorWidth / 2;
			else if (mouseX > _width - PortInteriorWidth / 2)
				offset = _width - PortInteriorWidth / 2;
			else
				offset = mouseX;
		}
		
		// left side
		if ((mouseX <= mouseY * wh) && (_width - mouseX) > mouseY * wh)
		{
			side = PortSide::Left;

			if (mouseY < PortInteriorWidth / 2)
				offset = PortInteriorWidth / 2;
			else if (mouseY > _height - PortInteriorWidth / 2)
				offset = _height - PortInteriorWidth / 2;
			else
				offset = mouseY;
		}

		// right side
		if ((mouseX > mouseY * wh) && (_width - mouseX) <= mouseY * wh)
		{
			side = PortSide::Right;

			if (mouseY < PortInteriorWidth / 2)
				offset = PortInteriorWidth / 2;
			else if (mouseY > _height - PortInteriorWidth / 2)
				offset = _height - PortInteriorWidth / 2;
			else
				offset = mouseY;
		}

		if (_side != side || _offset != offset)
		{
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidPortSide);
			NotifyPropertyChanging (_propChangeCP, AsUnknown(), dispidPortOffset);
			NotifyInvalidate(_invalidateCP, nullptr);
			_side = side;
			_offset = offset;
			NotifyInvalidate(_invalidateCP, nullptr);
			NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidPortOffset);
			NotifyPropertyChanged (_propChangeCP, AsUnknown(), dispidPortSide);
		}
	}

	virtual bool mac_operational() const override { return _actualSpeed > 0; }

	virtual uint32_t SupportedSpeed() const override { return _supported_speed; }

	virtual void SetActualSpeed (uint32_t value) override
	{
		if (_actualSpeed != value)
		{
			// The actual speed should either be transitioning from zero to non-zero (MAC_Operational becoming True),
			// or it should be transitioning from non-zero to zero (MAC_Operational becoming False).
			WI_ASSERT (_actualSpeed ^ value);

			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidPortActualSpeed);
			NotifyPropertyChanging(_propChangeCP, AsUnknown(), dispidPortMacOperational);
			_actualSpeed = value;
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidPortMacOperational);
			NotifyPropertyChanged(_propChangeCP, AsUnknown(), dispidPortActualSpeed);
		}
	}

	virtual uint32_t treeCount() const override { return _trees.size(); }
	
	virtual IPortTree* treeAt(uint32_t i) override { return _trees[i]; }

	#pragma region IStpPropertyChangeSink
	static inline const std::pair<STP_PROPERTY, DISPID> stpPropertyToDispidMap[] = {
		{ STP_PROPERTY_ADMIN_EDGE,   dispidAdminEdge },
		{ STP_PROPERTY_OPER_EDGE,    dispidOperEdge },
		{ STP_PROPERTY_DETECTED_P2P, dispidPortDetectedP2P },
		{ STP_PROPERTY_OPER_P2P,     dispidPortOperP2P },
		{ STP_PROPERTY_ADMIN_P2P,    dispidPortAdminP2P },
		{ STP_PROPERTY_PORT_ENABLED, dispidPortMacOperational },
	};

	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanging (IBridge*, unsigned int portIndex, unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept override
	{
		// STP properties don't change that often, so it's acceptable for a UI port (PortImpl) to receive
		// notifications for all STP properties and filter out the ones that are not relevant to it.
		if (portIndex != _port_index || treeIndex != -1)
			return S_OK;

		auto it = std::find_if(std::begin(stpPropertyToDispidMap), std::end(stpPropertyToDispidMap),
			[prop](const auto& pair) { return pair.first == prop; });
		if (it != std::end(stpPropertyToDispidMap))
			return NotifyPropertyChanging(_propChangeCP, AsUnknown(), it->second);

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnStpPropertyChanged(IBridge*, unsigned int portIndex, unsigned int treeIndex, STP_PROPERTY prop, unsigned int timestamp) noexcept override
	{
		if (portIndex != _port_index || treeIndex != -1)
			return S_OK;

		auto it = std::find_if(std::begin(stpPropertyToDispidMap), std::end(stpPropertyToDispidMap),
			[prop](const auto& pair) { return pair.first == prop; });
		if (it != std::end(stpPropertyToDispidMap))
			return NotifyPropertyChanged(_propChangeCP, AsUnknown(), it->second);

		return S_OK;
	}
	#pragma endregion
};

HRESULT MakePort (IBridge* parent, uint32_t portIndex, PortSide side, LONG offset, IPort** ppPort)
{
	auto p = com_ptr (new (std::nothrow) PortImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(parent, portIndex, side, offset); RETURN_IF_FAILED(hr);
	*ppPort = p.detach();
	return S_OK;
}

void RenderExteriorNonStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, bool macOperational)
{
	auto& brush = macOperational ? dos._brushForwarding : dos._brushDiscardingPort;
	dc->DrawLine (Point2F (0, 0), Point2F (0, PortExteriorHeight), brush, 2);
}

void RenderExteriorStpPort (ID2D1RenderTarget* dc, const drawing_resources& dos, STP_PORT_ROLE role, bool learning, bool forwarding, bool operEdge)
{
	static constexpr LONG circleDiameter = std::min (PortExteriorHeight / 2, PortExteriorWidth);

	static constexpr float edw = PortExteriorWidth;
	static constexpr float edh = PortExteriorHeight;

	static constexpr float discardingFirstHorizontalLineY = circleDiameter + (edh - circleDiameter) / 3;
	static constexpr float discardingSecondHorizontalLineY = circleDiameter + (edh - circleDiameter) * 2 / 3;
	static constexpr float learningHorizontalLineY = circleDiameter + (edh - circleDiameter) / 2;

	static constexpr float dfhly = discardingFirstHorizontalLineY;
	static constexpr float dshly = discardingSecondHorizontalLineY;

	static const D2D1_ELLIPSE ellipseFill = { Point2F (0, circleDiameter / 2), circleDiameter / 2 + 0.5f, circleDiameter / 2 + 0.5f };
	static const D2D1_ELLIPSE ellipseDraw = { Point2F (0, circleDiameter / 2), circleDiameter / 2 - 0.5f, circleDiameter / 2 - 0.5f};

	auto oldaa = dc->GetAntialiasMode();

	if (role == STP_PORT_ROLE_DISABLED)
	{
		// disabled
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawLine (Point2F (-edw / 2, edh / 3), Point2F (edw / 2, edh * 2 / 3), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && !learning && !forwarding)
	{
		// designated discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushDiscardingPort);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && !forwarding)
	{
		// designated learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushLearningPort);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && !operEdge)
	{
		// designated forwarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushForwarding);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushForwarding, 2);
	}
	else if ((role == STP_PORT_ROLE_DESIGNATED) && learning && forwarding && operEdge)
	{
		// designated forwarding operEdge
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->FillEllipse (&ellipseFill, dos._brushForwarding);
		static constexpr D2D1_POINT_2F points[] =
		{
			{ 0, circleDiameter },
			{ -edw / 2 + 1, circleDiameter + (edh - circleDiameter) / 2 },
			{ 0, edh },
			{ edw / 2 - 1, circleDiameter + (edh - circleDiameter) / 2 },
		};

		dc->DrawLine (points[0], points[1], dos._brushForwarding, 2);
		dc->DrawLine (points[1], points[2], dos._brushForwarding, 2);
		dc->DrawLine (points[2], points[3], dos._brushForwarding, 2);
		dc->DrawLine (points[3], points[0], dos._brushForwarding, 2);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && !learning && !forwarding)
	{
		// root or master discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushDiscardingPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && !forwarding)
	{
		// root or master learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushLearningPort, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if (((role == STP_PORT_ROLE_ROOT) || (role == STP_PORT_ROLE_MASTER)) && learning && forwarding)
	{
		// root or master forwarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
		dc->DrawEllipse (&ellipseDraw, dos._brushForwarding, 2);
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, circleDiameter), Point2F (0, edh), dos._brushForwarding, 2);
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) && !learning && !forwarding)
	{
		// Alternate discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else if ((role == STP_PORT_ROLE_ALTERNATE) && learning && !forwarding)
	{
		// Alternate learning
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushLearningPort, 2);
		dc->DrawLine (Point2F (-edw / 2, learningHorizontalLineY), Point2F (edw / 2, learningHorizontalLineY), dos._brushLearningPort);
	}
	else if ((role == STP_PORT_ROLE_BACKUP) && !learning && !forwarding)
	{
		// Backup discarding
		dc->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
		dc->DrawLine (Point2F (0, 0), Point2F (0, edh), dos._brushDiscardingPort, 2);
		dc->DrawLine (Point2F (-edw / 2, dfhly / 2), Point2F (edw / 2, dfhly / 2), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dfhly), Point2F (edw / 2, dfhly), dos._brushDiscardingPort);
		dc->DrawLine (Point2F (-edw / 2, dshly), Point2F (edw / 2, dshly), dos._brushDiscardingPort);
	}
	else
		WI_ASSERT(false); // not implemented

	dc->SetAntialiasMode(oldaa);
}
