
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#pragma once
#include "vector_nothrow.h"

template<typename T>
using com_ptr = wil::com_ptr_nothrow<T>;

template<typename ITo>
static bool TryQI (ITo* from, REFIID riid, void** ppvObject)
{
	if (__uuidof(from) == riid)
	{
		*ppvObject = from;
		static_cast<IUnknown*>(*ppvObject)->AddRef();
		return true;
	}
	return false;
}

// Helper function meant to be called from the IUnknown::Release() of STA objects.
// When the last reference is released, calls the destructor while the object still has a reference count of 1.
// Meant only for objects allocated with the regular operator new, or new (std::nothrow).
template<typename T>
ULONG ReleaseST (T* _this, ULONG& refCount)
{
	WI_ASSERT(refCount);
	if (refCount > 1)
		return --refCount;

	// We want to set the refCount to 0 after the destructor runs and before the memory is freed.
	// This helps catch accesses to the object after its refCount went to 0 (in the WI_ASSERT
	// at the top of this function).
	_this->~T();
	refCount = 0;
	operator delete(_this);

	return 0;
}

#pragma region IWeakRef
// This interface is meant to be implemented by COM classes (in QueryInterface), but not by C++ classes.
// Deriving a C++ class from this interface in addition to other interfaces is most likely an error.
// Only a concrete C++ class that implements IWeakRef, and only IWeakRef, should derive from it;
// an example of this is WeakRefToThis::WeakRefImpl.
struct DECLSPEC_NOVTABLE DECLSPEC_UUID("{D02BB0BB-54C9-4B5D-81BA-80CF223D2F55}") IWeakRef : IUnknown
{
};

// This class contains an implementation of IWeakRef. It must be used on a single thread.
// COM classes that implement IWeakRef can have a member variable of type WeakRefToThis
// and hand out weak references to themselves using QueryIWeakRef() and operator IWeakRef*().
// Note that the function that overloads that operator does not call AddRef(); it's the
// responsibility of the caller to do AddRef().
class WeakRefToThis
{
	struct WeakRefImpl : IWeakRef
	{
		ULONG _weakRefCount = 0;
		ULONG _sig = 0xAA550010;
		IUnknown* _ptr;

		WeakRefImpl (IUnknown* ptr) : _ptr(ptr) { }

		virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
		{
			if (riid == __uuidof(IWeakRef))
			{
				*ppvObject = this;
				AddRef();
				return S_OK;
			}

			if (!_ptr)
			{
				// All normal references have been released and the object is now dead.
				*ppvObject = nullptr;
				return S_OK;
			}

			return _ptr->QueryInterface(riid, ppvObject);
		}

		virtual ULONG STDMETHODCALLTYPE AddRef() override
		{
			return ++_weakRefCount;
		}

		virtual ULONG STDMETHODCALLTYPE Release() override
		{
			return ReleaseST(this, _weakRefCount);
		}
	};

	WeakRefImpl* wr = nullptr; // A NULL "wr" means no weak references have been created yet.

public:
	// This function allocates a weak reference to the objects holding us;
	// this is best called from the InitInstance of that object.
	// Once this allocation succeeds, generating weak references to that object is an operation
	// that never fails (it merely does an AddRef() on the already allocated weak reference).
	HRESULT InitInstance (IUnknown* holdingObject)
	{
		wr = new (std::nothrow) WeakRefImpl(holdingObject); RETURN_IF_NULL_ALLOC(wr);

		// One AddRef from the WeakRefToThis object, which will get released
		// when the object is deleted and our destructor is called.
		wr->AddRef();

		return S_OK;
	}

	~WeakRefToThis()
	{
		// Last normal reference to the object holding us has been released.
		// The object holding us is now being deleted.
		WI_ASSERT(wr);
		WI_ASSERT(wr->_ptr);
		wr->_ptr = nullptr;
		wr->Release();
		wr = nullptr;
	}

	HRESULT QueryIWeakRef (void** ppvObject)
	{
		WI_ASSERT(wr);
		*ppvObject = wr;
		wr->AddRef();
		return S_OK;
	}

	operator IWeakRef*()
	{
		return wr;
	}
};
#pragma endregion

#pragma region Connection Points
HRESULT MakeConnectionPointEnumerator (const CONNECTDATA* data, uint32_t size, IEnumConnections** ppEnum);

template<typename ISink> requires wistd::is_base_of_v<IUnknown, ISink>
class ConnectionPointImpl : public IConnectionPoint
{
	ULONG _refCount = 0;
	IConnectionPointContainer* _cont = nullptr;
	// We store pointers to IUnknown, rather than pointers to the sink interface type, in order to
	// allow the users of our class to pass us weak references to sinks (pointers to IWeakRef).
	// The sources don't know if the sinks are strong or weak references. So at the point of notifying the sinks;
	// a source does a QueryInterface to the sink interface type. If the result is S_OK and the obtained pointer
	// is non-NULL, it has now a strong reference and notifies it; if the result is S_OK and the obtained pointer
	// is NULL, that was a weak reference to an object that's now dead, so it ignores it.
	vector_nothrow<CONNECTDATA> _cps;
	DWORD _nextCookie = 1;

	template<typename ISink> requires wistd::is_base_of_v<IUnknown, ISink>
	friend HRESULT MakeConnectionPoint (IConnectionPointContainer* cont, ConnectionPointImpl<ISink>** cp);

	template<typename T>
	friend ULONG ReleaseST (T* _this, ULONG& refCount);

	HRESULT InitInstance (IConnectionPointContainer* cont)
	{
		_cont = cont;
		return S_OK;
	}

	~ConnectionPointImpl()
	{
		WI_ASSERT(_cps.empty());
		for (uint32_t i = 0; i < _cps.size(); i++)
			_cps[i].pUnk->Release();
	}

public:
	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);

		if (riid == IID_IUnknown)
		{
			*ppvObject = static_cast<IConnectionPoint*>(this);
			AddRef();
			return S_OK;
		}

		if (riid == IID_IConnectionPoint)
		{
			*ppvObject = static_cast<IConnectionPoint*>(this);
			AddRef();
			return S_OK;
		}

		*ppvObject = nullptr;
		return E_NOINTERFACE;
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IConnectionPoint
	virtual HRESULT STDMETHODCALLTYPE GetConnectionInterface (IID *pIID) override
	{
		*pIID = __uuidof(ISink);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE GetConnectionPointContainer (IConnectionPointContainer** ppCPC) override
	{
		*ppCPC = _cont;
		(*ppCPC)->AddRef();
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Advise (IUnknown* pUnkSink, DWORD* pdwCookie) override
	{
		// Early test that the sink actually implements ISink.
		com_ptr<ISink> unused;
		auto hr = pUnkSink->QueryInterface(IID_PPV_ARGS(&unused)); RETURN_IF_FAILED(hr);

		bool pushed = _cps.try_push_back({ pUnkSink, _nextCookie }); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
		pUnkSink->AddRef();
		*pdwCookie = _nextCookie++;
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Unadvise (DWORD dwCookie) override
	{
		auto it = _cps.find_if([dwCookie](const CONNECTDATA& cd) { return cd.dwCookie == dwCookie; });
		RETURN_HR_IF(E_POINTER, it == _cps.end());
		it->pUnk->Release();
		_cps.erase(it);
		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE EnumConnections (IEnumConnections **ppEnum) override
	{
		return MakeConnectionPointEnumerator(_cps.data(), _cps.size(), ppEnum);
	}
	#pragma endregion

	bool empty() const { return _cps.empty(); }

	template<typename predicate_t> requires wistd::is_invocable_r_v<HRESULT, predicate_t, ISink*>
	HRESULT Notify (const predicate_t& pred)
	{
		vector_nothrow<com_ptr<IUnknown>> copy;
		bool reserved = copy.try_reserve(_cps.size()); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);
		for (auto& c : _cps)
			copy.try_push_back(c.pUnk);

		for (auto& c : copy)
		{
			com_ptr<ISink> sink;
			auto hr = c->QueryInterface(IID_PPV_ARGS(&sink)); RETURN_IF_FAILED(hr);
			// If QueryInterface succeeded but the result is NULL, we're likely dealing with one of our
			// weak references, and the referenced object is now dead. We allow this and we ignore the sink.
			if (!sink)
			{
				#ifdef _DEBUG
				com_ptr<IWeakRef> wr;
				_ASSERT(SUCCEEDED(c->QueryInterface(&wr)) && wr);
				#endif
			}
			else
			{
				hr = pred(sink.get()); RETURN_IF_FAILED(hr);
			}
		}
		return S_OK;
	}
};

template<typename ISink> requires wistd::is_base_of_v<IUnknown, ISink>
HRESULT MakeConnectionPoint (IConnectionPointContainer* cont, ConnectionPointImpl<ISink>** cp)
{
	auto p = com_ptr (new (std::nothrow) ConnectionPointImpl<ISink>()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(cont); RETURN_IF_FAILED(hr);
	*cp = p.detach();
	return S_OK;
}

class EnumConnectionsImpl : public IEnumConnections
{
	ULONG _refCount = 0;
	ULONG _next = 0;
	vector_nothrow<CONNECTDATA> _cps;

public:
	HRESULT InitInstace (const CONNECTDATA* data, uint32_t size)
	{
		bool reserved = _cps.try_reserve(size); RETURN_HR_IF(E_OUTOFMEMORY, !reserved);
		for (uint32_t i = 0; i < size; i++)
		{
			_cps.try_push_back(data[i]);
			data[i].pUnk->AddRef();
		}

		return S_OK;
	}

	~EnumConnectionsImpl()
	{
		for (uint32_t i = 0; i < _cps.size(); i++)
			_cps[i].pUnk->Release();
	}

	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);

		if (riid == IID_IEnumConnections)
		{
			*ppvObject = static_cast<IEnumConnections*>(this);
			AddRef();
			return S_OK;
		}

		*ppvObject = nullptr;
		RETURN_HR(E_NOINTERFACE);
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IEnumConnections
	virtual HRESULT STDMETHODCALLTYPE Next (ULONG cConnections, LPCONNECTDATA rgcd, ULONG* pcFetched) override
	{
		if (_next == _cps.size())
		{
			if (pcFetched)
				*pcFetched = 0;
			return S_FALSE;
		}

		if (cConnections != 1)
			RETURN_HR(E_NOTIMPL);

		*rgcd = _cps[_next];
		rgcd->pUnk->AddRef();
		_next++;

		if (pcFetched)
			*pcFetched = 1;

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE Skip (ULONG cConnections) override { RETURN_HR(E_NOTIMPL); }

	virtual HRESULT STDMETHODCALLTYPE Reset() override { RETURN_HR(E_NOTIMPL); }

	virtual HRESULT STDMETHODCALLTYPE Clone (IEnumConnections **ppEnum) override { RETURN_HR(E_NOTIMPL); }
	#pragma endregion
};

inline HRESULT MakeConnectionPointEnumerator (const CONNECTDATA* data, uint32_t size, IEnumConnections** ppEnum)
{
	com_ptr<EnumConnectionsImpl> p = new (std::nothrow) EnumConnectionsImpl(); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstace(data, size); RETURN_IF_FAILED(hr);
	*ppEnum = p.detach();
	return S_OK;
}

class AdviseSinkToken
{
	com_ptr<IConnectionPoint> _cp;
	DWORD _dwCookie = 0;

	template<typename ISink>
	friend HRESULT AdviseSink (IUnknown* source, IWeakRef* sink, AdviseSinkToken* pToken);

public:
	AdviseSinkToken() noexcept = default;

	AdviseSinkToken(const AdviseSinkToken&) = delete;
	AdviseSinkToken& operator=(const AdviseSinkToken&) = delete;

	AdviseSinkToken (AdviseSinkToken&& from) noexcept
		: _cp(std::move(from._cp)), _dwCookie(from._dwCookie)
	{
		from._dwCookie = 0;
	}

	AdviseSinkToken& operator=(AdviseSinkToken&& from) noexcept
	{
		reset();
		std::swap(_cp, from._cp);
		std::swap(_dwCookie, from._dwCookie);
		return *this;
	}

	~AdviseSinkToken() noexcept
	{
		reset();
	}

	void reset() noexcept
	{
		if (_dwCookie)
		{
			auto hr = _cp->Unadvise(_dwCookie); LOG_IF_FAILED(hr);
			_dwCookie = 0;
			_cp = nullptr;
		}
		else
			WI_ASSERT(!_cp);
	}

	AdviseSinkToken* operator&() noexcept
	{
		reset();
		return this;
	}

	operator bool() const
	{
		return _dwCookie != 0;
	}
};

template<typename ISink>
inline HRESULT AdviseSink (IUnknown* source, IWeakRef* sink, AdviseSinkToken* pToken)
{
	HRESULT hr;
	com_ptr<IConnectionPointContainer> cpc;
	hr = source->QueryInterface(&cpc); RETURN_IF_FAILED_EXPECTED(hr);
	com_ptr<IConnectionPoint> cp;
	hr = cpc->FindConnectionPoint(__uuidof(ISink), &cp); RETURN_IF_FAILED_EXPECTED(hr);
	DWORD dwCookie;
	hr = cp->Advise(sink, &dwCookie); RETURN_IF_FAILED(hr);
	pToken->reset();
	pToken->_cp = std::move(cp);
	pToken->_dwCookie = dwCookie;
	return S_OK;
}
#pragma endregion

#define IMPLEMENT_IDISPATCH_(IID, filename) \
	static inline com_ptr<ITypeInfo> _typeInfo; \
	static inline com_ptr<ITypeLib> _typeLib; \
	static ITypeInfo* InitTypeInfo() \
	{ \
		if (!_typeInfo) { \
			HRESULT hr; \
			if(!filename) { \
				wil::unique_process_heap_string fn; \
				hr = wil::GetModuleFileNameW((HMODULE)&__ImageBase, fn); FAIL_FAST_IF_FAILED(hr); \
				hr = LoadTypeLibEx (fn.get(), REGKIND_NONE, &_typeLib); FAIL_FAST_IF_FAILED(hr); \
			} else { \
				hr = LoadTypeLibEx (filename, REGKIND_NONE, &_typeLib); FAIL_FAST_IF_FAILED(hr); \
			} \
			hr = _typeLib->GetTypeInfoOfGuid(__uuidof(IID), &_typeInfo); FAIL_FAST_IF_FAILED(hr); \
		} \
		return _typeInfo.get(); \
	} \
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT* pctinfo) override final { *pctinfo = 1; return S_OK; } \
	virtual HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo** ppTInfo) override final { \
		*ppTInfo = InitTypeInfo(); \
		(*ppTInfo)->AddRef(); \
		return S_OK; \
	} \
	virtual HRESULT STDMETHODCALLTYPE GetIDsOfNames(REFIID riid, LPOLESTR* rgszNames, UINT cNames, LCID lcid, DISPID* rgDispId) override final { \
		return DispGetIDsOfNames (InitTypeInfo(), rgszNames, cNames, rgDispId); \
	} \
	virtual HRESULT STDMETHODCALLTYPE Invoke(DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS* pDispParams, VARIANT* pVarResult, EXCEPINFO* pExcepInfo, UINT* puArgErr) override final { \
		return DispInvoke (static_cast<IID*>(this), InitTypeInfo(), dispIdMember, wFlags, pDispParams, pVarResult, pExcepInfo, puArgErr); \
	}

#define IMPLEMENT_IDISPATCH(IID) IMPLEMENT_IDISPATCH_(IID,NULL)

using unique_safearray = wil::unique_any<SAFEARRAY*, decltype(SafeArrayDestroy), &SafeArrayDestroy>;
