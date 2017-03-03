
#include "pch.h"
#include "RCHBase.h"
#include "../Win32Defs.h"

using namespace std;

// C++03 standard 3.6.2 assures us that the runtime initializes these POD types
// to zero before it constructs any static instance of TypeInfo.
static unordered_set<const RCHInfo*>* chInfos = nullptr;

const std::unordered_set<const RCHInfo*>& GetRCHInfos()
{
	assert (chInfos != nullptr);
	return *chInfos;
}

RCHInfo::RCHInfo (std::unordered_map<UINT32, Callbacks>&& commands, RCHFactory factory)
	: _commands(move(commands)), _factory(factory)
{
	if (chInfos == nullptr)
		chInfos = new unordered_set<const RCHInfo*>();
	chInfos->insert(this);
}

RCHInfo::~RCHInfo()
{
	chInfos->erase(this);
	if (chInfos->empty())
	{
		delete chInfos;
		chInfos = nullptr;
	}
}

// ============================================================================

void RCHBase::InjectDependencies (const RCHDeps& deps)
{
	_pw = deps.pw;
	_rf = deps.rf;
	_project = deps.project;
	_area = deps.area;
	_selection = deps.selection;

	_selection->GetSelectionChangedEvent().AddHandler(&OnSelectionChangedStatic, this);
	_selection->GetAddedToSelectionEvent().AddHandler(OnAddedToSelection, this);
	_selection->GetRemovingFromSelectionEvent().AddHandler(OnRemovingFromSelection, this);
}

RCHBase::~RCHBase()
{
	_selection->GetRemovingFromSelectionEvent().RemoveHandler(OnRemovingFromSelection, this);
	_selection->GetAddedToSelectionEvent().RemoveHandler(OnAddedToSelection, this);
	_selection->GetSelectionChangedEvent().RemoveHandler(&OnSelectionChangedStatic, this);
}

HRESULT STDMETHODCALLTYPE RCHBase::Execute (UINT32 commandId, UI_EXECUTIONVERB verb, const PROPERTYKEY *key, const PROPVARIANT *currentValue, IUISimplePropertySet *commandExecutionProperties)
{
	const auto& commands = this->GetInfo()._commands;
	auto it = commands.find(commandId);
	if ((it == commands.end()) || (it->second.execute == nullptr))
		return E_NOTIMPL;

	try
	{
		return (this->*(it->second.execute)) (commandId, verb, key, currentValue, commandExecutionProperties);
	}
	catch (const exception& ex)
	{
		MessageBoxA (_pw->GetHWnd(), ex.what(), nullptr, 0);
		return S_FALSE;
	}
}

HRESULT STDMETHODCALLTYPE RCHBase::UpdateProperty (UINT32 commandId, REFPROPERTYKEY key, const PROPVARIANT *currentValue, PROPVARIANT *newValue)
{
	const auto& commands = this->GetInfo()._commands;
	auto it = commands.find(commandId);
	if ((it == commands.end()) || (it->second.update == nullptr))
		return E_NOTIMPL;

	return (this->*(it->second.update)) (commandId, key, currentValue, newValue);
}

HRESULT RCHBase::QueryInterface(REFIID riid, void **ppvObject)
{
	if (!ppvObject)
		return E_INVALIDARG;

	*ppvObject = NULL;
	if (riid == __uuidof(IUnknown))
	{
		*ppvObject = static_cast<IUnknown*>((IProjectWindow*) this);
		AddRef();
		return S_OK;
	}
	else if (riid == __uuidof(IUICommandHandler))
	{
		*ppvObject = static_cast<IUICommandHandler*>(this);
		AddRef();
		return S_OK;
	}

	return E_NOINTERFACE;
}

ULONG RCHBase::AddRef()
{
	return InterlockedIncrement (&_refCount);
}

ULONG RCHBase::Release()
{
	ULONG newRefCount = InterlockedDecrement(&_refCount);
	if (newRefCount == 0)
		delete this;
	return newRefCount;
}

// ============================================================================

ItemPropertySet::ItemPropertySet (wstring&& text)
	: _text(move(text))
{ }

ULONG STDMETHODCALLTYPE ItemPropertySet::AddRef()
{
	return InterlockedIncrement (&_refCount);
}

ULONG STDMETHODCALLTYPE ItemPropertySet::Release()
{
	auto newRefCount = InterlockedDecrement(&_refCount);
	if (newRefCount == 0)
		delete this;
	return newRefCount;
}

HRESULT STDMETHODCALLTYPE ItemPropertySet::QueryInterface(REFIID iid, void** ppv)
{
	if (!ppv)
		return E_POINTER;

	if (iid == __uuidof(IUnknown))
	{
		*ppv = static_cast<IUnknown*>(this);
		AddRef();
		return S_OK;
	}

	if (iid == __uuidof(IUISimplePropertySet))
	{
		*ppv = static_cast<IUISimplePropertySet*>(this);
		AddRef();
		return S_OK;
	}

	*ppv = NULL;
	return E_NOINTERFACE;
}

// IUISimplePropertySet
HRESULT STDMETHODCALLTYPE ItemPropertySet::GetValue (REFPROPERTYKEY key, PROPVARIANT *ppropvar)
{
	/*
	if (key == UI_PKEY_Enabled)
	return UIInitPropertyFromBoolean (UI_PKEY_Enabled, _enabled, ppropvar);

	if (key == UI_PKEY_ItemImage)
	{
	if (_image)
	return UIInitPropertyFromImage (UI_PKEY_ItemImage, _image, ppropvar);

	return S_FALSE;
	}
	*/
	if (key == UI_PKEY_Label)
		return UIInitPropertyFromString (UI_PKEY_Label, _text.c_str(), ppropvar);

	//if (key == UI_PKEY_CategoryId)
	//	return UIInitPropertyFromUInt32 (UI_PKEY_CategoryId, _categoryId, ppropvar);

	return E_NOTIMPL;
}