
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "simulator.h"
#include "dispids.h"

using namespace edge;

class selection : public ISelection, IPropertyChangeSink, IConnectionPointContainer
{
	ULONG _refCount = 0;
	ULONG _sig = 0xAA550001;
	IStpProject* _project;
	std::vector<com_ptr<IDispatch>> _objects;
	AdviseSinkToken _token;
	WeakRefToThis _weakRefToThis;
	com_ptr<ConnectionPointImpl<IObjectCollectionChangeEvents>> _occCP;

public:
	HRESULT InitInstance (IStpProject* project)
	{
		_project = project;

		auto hr = _weakRefToThis.InitInstance(static_cast<ISelection*>(this)); RETURN_IF_FAILED(hr);

		hr = MakeConnectionPoint(this, &_occCP); RETURN_IF_FAILED(hr);

		hr = AdviseSink<IPropertyChangeSink>(project, _weakRefToThis, &_token); LOG_IF_FAILED(hr);
		return S_OK;
	}

	~selection()
	{
		// TODO: remove this
		clear();
	}

	IUnknown* AsUnknown() { return static_cast<ISelection*>(this); }
	#pragma region IUnknown
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override
	{
		RETURN_HR_IF(E_POINTER, !ppvObject);
		*ppvObject = nullptr;

		if (   TryQI<IUnknown>(AsUnknown(), riid, ppvObject)
			|| TryQI<ISelection>(this, riid, ppvObject)
			|| TryQI<IPropertyChangeSink>(this, riid, ppvObject)
			|| TryQI<IConnectionPointContainer>(this, riid, ppvObject)
		)
			return S_OK;

		if (riid == __uuidof(IWeakRef))
			return _weakRefToThis.QueryIWeakRef(ppvObject);

		RETURN_HR(E_NOINTERFACE);
	}

	virtual ULONG STDMETHODCALLTYPE AddRef() override { return ++_refCount; }

	virtual ULONG STDMETHODCALLTYPE Release() override { return ReleaseST(this, _refCount); }
	#pragma endregion

	#pragma region IConnectionPointContainer
	virtual HRESULT STDMETHODCALLTYPE EnumConnectionPoints (IEnumConnectionPoints **ppEnum) override
	{
		RETURN_HR(E_NOTIMPL);
	}

	virtual HRESULT STDMETHODCALLTYPE FindConnectionPoint (REFIID riid, IConnectionPoint **ppCP) override
	{
		if (riid == __uuidof(IObjectCollectionChangeEvents))
			return wil::com_query_to_nothrow(_occCP, ppCP);
		RETURN_HR(E_NOTIMPL);
	}
	#pragma endregion

	#pragma region IPropertyChangeSink
	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging (IUnknown* obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		auto project = wil::try_com_query_nothrow<IStpProject>(obj);
		if (dispID == dispidBridges)
		{
			WI_ASSERT(args->propertyType == PropertyType::Collection);
			if (args->collectionChangeArgs.changeType == CollectionChangeType::Remove)
			{
				// Removing bridge.
				RETURN_HR_IF(E_NOTIMPL, args->collectionChangeArgs.setInsertRemoveArgs.count != 1);
				ULONG index = args->collectionChangeArgs.setInsertRemoveArgs.index;
				auto disp = wil::try_com_query_nothrow<IDispatch>(project->BridgeAt(index));
				auto it = std::find(_objects.begin(), _objects.end(), disp.get());
				if (it != _objects.end())
				{
					size_t i = it - _objects.begin();
					remove_internal(i, 1);
				}
			}
		}
		else if (dispID == dispidWires)
		{
			WI_ASSERT(args->propertyType == PropertyType::Collection);
			if (args->collectionChangeArgs.changeType == CollectionChangeType::Remove)
			{
				// Removing wire.
				RETURN_HR_IF(E_NOTIMPL, args->collectionChangeArgs.setInsertRemoveArgs.count != 1);
				ULONG index = args->collectionChangeArgs.setInsertRemoveArgs.index;
				auto disp = wil::try_com_query_nothrow<IDispatch>(project->WireAt(index));
				auto it = std::find(_objects.begin(), _objects.end(), disp.get());
				if (it != _objects.end())
				{
					size_t i = it - _objects.begin();
					remove_internal(i, 1);
				}
			}
		}

		return S_OK;
	}

	virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged (IUnknown* obj, DISPID dispID, const PropertyChangeArgs* args) override
	{
		return S_OK;
	}
	#pragma endregion

	virtual uint32_t size() const override { return (uint32_t)_objects.size(); }
	
	virtual IDispatch* operator[](uint32_t index) const override { return _objects[index]; }

	void add_internal (IDispatch* o)
	{
		ObjectCollectionChangeArgs occ = { .changeType = Insert, .setInsertRemoveArgs = { .index = (ULONG)_objects.size(), .count = 1, .childObjs = &o } };
		_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) { return e->OnCollectionChanging(AsUnknown(), &occ); });
		_objects.push_back(o);
		occ.setInsertRemoveArgs.childObjs = nullptr;
		_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) { return e->OnCollectionChanged(AsUnknown(), &occ); });
	}

	void remove_internal (size_t index, size_t size)
	{
		if (size)
		{
			ObjectCollectionChangeArgs occ = { .changeType = Remove, .setInsertRemoveArgs = { .index = (ULONG)index, .count = (ULONG)size } };
			_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) { return e->OnCollectionChanging(AsUnknown(), &occ); });
			std::vector<com_ptr<IDispatch>> removed;
			std::copy(_objects.begin() + index, _objects.begin() + index + size, std::back_inserter(removed));
			_objects.erase(_objects.begin() + index, _objects.begin() + index + size);
			vector_nothrow<IDispatch*> punks;
			for (auto d : removed)
				punks.try_push_back(d.try_query<IDispatch>().get());
			occ.setInsertRemoveArgs.childObjs = punks.data();
			_occCP->Notify([this,&occ](IObjectCollectionChangeEvents* e) { return e->OnCollectionChanged(AsUnknown(), &occ); });
		}
	}

	virtual void clear() override final
	{
		remove_internal(0, _objects.size());
	}

	virtual void select (IDispatch* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		if ((_objects.size() != 1) || (_objects[0] != o))
		{
			remove_internal (0, _objects.size());
			add_internal(o);
		}
	}

	virtual void add (IDispatch* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		if (std::find (_objects.begin(), _objects.end(), o) != _objects.end())
			throw std::invalid_argument("Object already in selection.");

		add_internal(o);
	}

	virtual void remove (IDispatch* o) override final
	{
		if (o == nullptr)
			throw std::invalid_argument("Parameter may not be nullptr.");

		auto it = std::find (_objects.begin(), _objects.end(), o);
		if (it == _objects.end())
			throw std::invalid_argument("Object not in selection.");
		size_t index = it - _objects.begin();

		remove_internal(index, 1);
	}
};

extern HRESULT selection_factory(IStpProject* project, ISelection** ppSelection)
{
	auto p = com_ptr(new (std::nothrow) selection()); RETURN_IF_NULL_ALLOC(p);
	auto hr = p->InitInstance(project); RETURN_IF_FAILED(hr);
	*ppSelection = p.detach();
	return S_OK;
};
