
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "edge/Z80Xml.h"

static constexpr float thickness = 2;

struct WireImpl : IWire, IWireProperties, IConnectionPointContainer
{
	ULONG _refCount = 0;
	IStpProject* _parent = nullptr;
	std::array<wire_end, 2> _points;
	com_ptr<ConnectionPointImpl<IInvalidateSink>> _invalidateCP;
	com_ptr<ConnectionPointImpl<IPropertyChangeSink>> _propChangeCP;

public:
	HRESULT InitInstance()
	{
		auto hr = MakeConnectionPoint(this, &_invalidateCP); RETURN_IF_FAILED(hr);
		hr = MakeConnectionPoint(this, &_propChangeCP); RETURN_IF_FAILED(hr);
		return S_OK;
	}

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IWire>(this, riid, ppvObject)
			|| TryQI<IWireProperties>(this, riid, ppvObject)
			|| TryQI<IDispatch>(static_cast<IWireProperties*>(this), riid, ppvObject)
			|| TryQI<IUnknown>(static_cast<IWireProperties*>(this), riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
			|| TryQI<ISelectableObject>(this, riid, ppvObject)
		)
			return S_OK;

		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	IMPLEMENT_IDISPATCH(IWireProperties);

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

	static HRESULT SerializeWireEnd (const wire_end& from, BSTR* pbstrEnd)
	{
		HRESULT hr;

		if (std::holds_alternative<connected_wire_end>(from))
		{
			IPort* p = std::get<connected_wire_end>(from);
			for (ULONG bi = 0; bi < p->bridge()->parent()->BridgeCount(); bi++)
			{
				auto b = p->bridge()->parent()->BridgeAt(bi);
				for (ULONG pi = 0; pi < b->PortCount(); pi++)
				{
					if (b->PortAt(pi) == p)
					{				
						wil::unique_process_heap_string ss;
						hr = wil::str_printf_nothrow (ss, L"Connected;%u;%u", bi, pi); RETURN_IF_FAILED(hr);
						*pbstrEnd = SysAllocString(ss.get()); RETURN_IF_NULL_ALLOC(*pbstrEnd);
						return S_OK;
					}
				}
			}

			RETURN_HR(E_UNEXPECTED);
		}
		else
		{
			auto location = std::get<loose_wire_end>(from);
			wil::unique_process_heap_string ss;
			hr = wil::str_printf_nothrow (ss, L"Loose;%ld;%ld", (LONG)location.x, (LONG)location.y);
			*pbstrEnd = SysAllocString(ss.get()); RETURN_IF_NULL_ALLOC(*pbstrEnd);
			return S_OK;
		}
	}

	static HRESULT DeserializeWireEnd (IStpProject* project, PCWSTR pszEnd, wire_end* endOut)
	{
		RETURN_HR_IF(E_POINTER, !project);
		RETURN_HR_IF(E_POINTER, !pszEnd);
		RETURN_HR_IF(E_POINTER, !endOut);

		const wchar_t* s1 = wcschr(pszEnd, L';');
		RETURN_HR_IF(E_INVALIDARG, !s1);
		const wchar_t* s2 = wcsrchr(pszEnd, L';');
		RETURN_HR_IF(E_INVALIDARG, !s2 || (s1 == s2));

		if (!wcsncmp(pszEnd, L"Connected", s1 - pszEnd))
		{
			wchar_t* end = nullptr;
			ULONG bridgeIndex = wcstoul(s1 + 1, &end, 10);
			RETURN_HR_IF(E_INVALIDARG, end != s2);
			ULONG portIndex = wcstoul(s2 + 1, &end, 10);
			RETURN_HR_IF(E_INVALIDARG, *end != 0);

			RETURN_HR_IF(E_INVALIDARG, bridgeIndex >= project->BridgeCount());
			auto bridge = project->BridgeAt(bridgeIndex);
			RETURN_HR_IF(E_INVALIDARG, portIndex >= bridge->PortCount());
			*endOut = bridge->PortAt(portIndex);
			return S_OK;
		}

		if (!wcsncmp(pszEnd, L"Loose", s1 - pszEnd))
		{
			wchar_t* end = nullptr;
			LONG x = wcstol(s1 + 1, &end, 10);
			RETURN_HR_IF(E_INVALIDARG, end != s2);
			LONG y = wcstol(s2 + 1, &end, 10);
			RETURN_HR_IF(E_INVALIDARG, *end != 0);
			*endOut = POINT{ x, y };
			return S_OK;
		}

		RETURN_HR(E_INVALIDARG);
	}

	#pragma region IWireProperties
	virtual HRESULT STDMETHODCALLTYPE get_P0 (BSTR *pbstrEnd) override
	{
		return SerializeWireEnd (_points[0], pbstrEnd);
	}

	virtual HRESULT STDMETHODCALLTYPE put_P0 (BSTR bstrEnd) override
	{
		wire_end end;
		auto hr = DeserializeWireEnd(_parent, bstrEnd, &end); RETURN_IF_FAILED(hr);
		set_point(0, end);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE get_P1 (BSTR *pbstrEnd) override
	{
		return SerializeWireEnd (_points[1], pbstrEnd);
	}

	virtual HRESULT STDMETHODCALLTYPE put_P1 (BSTR bstrEnd) override
	{
		wire_end end;
		auto hr = DeserializeWireEnd(_parent, bstrEnd, &end); RETURN_IF_FAILED(hr);
		set_point(1, end);
		return S_OK;
	}
	#pragma endregion

	virtual IStpProject* parent() const override
	{
		return _parent;
	}

	virtual void set_parent (IStpProject* parent) override
	{
		_parent = parent;
	}

	virtual std::array<wire_end, 2>& points() override { return _points; }
	const wire_end& point (size_t i) const { return _points[i]; }
	wire_end& point (size_t i) { return _points[i]; }

	void set_point (size_t pointIndex, wire_end point)
	{
		if (_points[pointIndex] != point)
		{
			_points[pointIndex] = point;
			_invalidateCP->Notify([this,e=extent()] (IInvalidateSink* sink) { return sink->OnInvalidate(e); });
		}
	}

	virtual POINT point_coords (size_t pointIndex) const noexcept override
	{
		if (std::holds_alternative<loose_wire_end>(_points[pointIndex]))
			return std::get<loose_wire_end>(_points[pointIndex]);
		else
			return std::get<connected_wire_end>(_points[pointIndex])->GetCPLocation();
	}

	virtual void render (ID2D1RenderTarget* rt, const drawing_resources& dos, bool forwarding, bool isPartOfLoop) const override
	{
		float width = thickness;
		ID2D1Brush* brush;

		if (!forwarding)
			brush = dos._brushNoForwardingWire;
		else if (!isPartOfLoop)
			brush = dos._brushForwarding;
		else
		{
			brush = dos._brushLoop;
			width *= 2;
		}

		auto& ss = forwarding ? dos._strokeStyleForwardingWire : dos._strokeStyleNoForwardingWire;
		rt->DrawLine (point_to_pointf(point_coords(0)), point_to_pointf(point_coords(1)), brush, width, ss);
	}

	#pragma region ISelectableObject
	virtual void render_selection (ID2D1DeviceContext* dc, const edge::IZoomer* zoomer, const drawing_resources& dos) const override
	{
		auto fd = zoomer->pointw_to_pointd(point_to_pointf(point_coords(0)));
		auto td = zoomer->pointw_to_pointd(point_to_pointf(point_coords(1)));

		float halfw = 10;
		float angle = atan2(td.y - fd.y, td.x - fd.x);
		float s = sin(angle);
		float c = cos(angle);

		D2D1_POINT_2F vertices[4] =
		{
			D2D1_POINT_2F { fd.x + s * halfw, fd.y - c * halfw },
			D2D1_POINT_2F { fd.x - s * halfw, fd.y + c * halfw },
			D2D1_POINT_2F { td.x - s * halfw, td.y + c * halfw },
			D2D1_POINT_2F { td.x + s * halfw, td.y - c * halfw }
		};

		dc->DrawLine (vertices[0], vertices[1], dos._brushHighlight, 2, dos._strokeStyleSelectionRect);
		dc->DrawLine (vertices[1], vertices[2], dos._brushHighlight, 2, dos._strokeStyleSelectionRect);
		dc->DrawLine (vertices[2], vertices[3], dos._brushHighlight, 2, dos._strokeStyleSelectionRect);
		dc->DrawLine (vertices[3], vertices[0], dos._brushHighlight, 2, dos._strokeStyleSelectionRect);
	}

	virtual int32_t hit_test (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance) override
	{
		auto p0d = wtr.TransformPoint(point_to_pointf(point_coords(0)));
		auto p1d = wtr.TransformPoint(point_to_pointf(point_coords(1)));

		if (   (dLocation.x >= p0d.x - tolerance) && (dLocation.x < p0d.x + tolerance)
			&& (dLocation.y >= p0d.y - tolerance) && (dLocation.y < p0d.y + tolerance))
			return 0;

		if (   (dLocation.x >= p1d.x - tolerance) && (dLocation.x < p1d.x + tolerance)
			&& (dLocation.y >= p1d.y - tolerance) && (dLocation.y < p1d.y + tolerance))
			return 1;

		auto lw = thickness * wtr._11; // assume no rotation, no skew
		if (edge::hit_test_line(dLocation, tolerance, p0d, p1d, lw))
			return -1;

		return 0;
	}

	virtual RECT extent() const noexcept override
	{
		auto tl = point_coords(0);
		auto br = point_coords(0);
		for (size_t i = 1; i < _points.size(); i++)
		{
			auto c = point_coords(i);
			tl.x = std::min (tl.x, c.x);
			tl.y = std::min (tl.y, c.y);
			br.x = std::max (br.x, c.x);
			br.y = std::max (br.y, c.y);
		}

		return { tl.x, tl.y, br.x, br.y };
	}
	#pragma endregion
};

HRESULT MakeWire (IWire** ppWire)
{
	auto p = com_ptr(new (std::nothrow) WireImpl()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(); RETURN_IF_FAILED(hr);
	*ppWire = p.detach();
	return S_OK;
}
