
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

RCHInfo::RCHInfo (RCHCommandsAndProperties&& cps, RCHFactory factory)
	: _cps(move(cps)), _factory(factory)
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

HRESULT RCHBase::QueryInterface(REFIID riid, void **ppvObject)
{
	throw NotImplementedException();
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
