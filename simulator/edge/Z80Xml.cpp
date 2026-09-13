
#include "pch.h"
#include "com.h"
#include "Z80Xml.h"
#include "edge.h"
#include "unordered_map_nothrow.h"

#pragma comment (lib, "xmllite.lib")

using namespace edge;

using unique_safearray = wil::unique_any<SAFEARRAY*, decltype(SafeArrayDestroy), &SafeArrayDestroy>;

using FactoryPropsPtr = wil::unique_any<DISPID*, decltype(&::CoTaskMemFree), ::CoTaskMemFree>;

static HRESULT GetNameFromEnumValue (ITypeInfo* ti, const TYPEATTR* typeAttr, LONG value, BSTR* nameOut)
{
	RETURN_HR_IF(E_INVALIDARG, typeAttr->typekind != TKIND_ENUM);

	for (WORD i = 0; i < typeAttr->cVars; i++)
	{
		VARDESC* varDesc;
		auto hr = ti->GetVarDesc(i, &varDesc); RETURN_IF_FAILED(hr);
		auto releaseVarDesc = wil::scope_exit([ti, varDesc] { ti->ReleaseVarDesc(varDesc); });
		RETURN_HR_IF(E_INVALIDARG, varDesc->lpvarValue->vt != VT_I4);
		if (varDesc->lpvarValue->lVal == value)
		{
			UINT cNames;
			hr = hr = ti->GetNames(varDesc->memid, nameOut, 1, &cNames); RETURN_IF_FAILED(hr);
			RETURN_HR_IF(E_FAIL, cNames != 1);
			return S_OK;
		}
	}

	RETURN_HR(DISP_E_UNKNOWNNAME);
}

static HRESULT GetEnumValueFromName (ITypeInfo* ti, const TYPEATTR* typeAttr, LPCWSTR name, LONG* valueOut)
{
	RETURN_HR_IF(E_INVALIDARG, typeAttr->typekind != TKIND_ENUM);

	for (WORD i = 0; i < typeAttr->cVars; i++)
	{
		VARDESC* varDesc;
		auto hr = ti->GetVarDesc(i, &varDesc); RETURN_IF_FAILED(hr);
		auto releaseVarDesc = wil::scope_exit([ti, varDesc] { ti->ReleaseVarDesc(varDesc); });
		RETURN_HR_IF(E_INVALIDARG, varDesc->lpvarValue->vt != VT_I4);
		wil::unique_bstr n;
		UINT cNames;
		hr = ti->GetNames(varDesc->memid, &n, 1, &cNames); RETURN_IF_FAILED(hr);
		if (!wcscmp(n.get(), name))
		{
			*valueOut = varDesc->lpvarValue->lVal;
			return S_OK;
		}
	}

	RETURN_HR(DISP_E_UNKNOWNNAME);
}

template<typename ValueType>
struct Property
{
	DISPID dispid;
	ValueType value;
};

using ValueProperty = Property<wil::unique_bstr>;
using ObjectProperty = Property<com_ptr<IDispatch>>;
using ObjectCollectionProperty = Property<vector_nothrow<com_ptr<IDispatch>>>;

// VS runs out of memory if we templatize SaveToXmlInternal and have it call itself. So let's reinvent the wheel...
struct EnsureElementCreated
{
	bool insideElement = false;
	const wchar_t* const elementName;
	IXmlWriter* writer;
	EnsureElementCreated* outer;

	EnsureElementCreated (const wchar_t* elementName, IXmlWriter* writer, EnsureElementCreated* outer)
		: elementName(elementName), writer(writer), outer(outer)
	{ }

	HRESULT CreateStartElement()
	{
		if (!insideElement)
		{
			if (outer)
			{
				auto hr = outer->CreateStartElement(); RETURN_IF_FAILED(hr);
			}

			auto hr = writer->WriteStartElement(NULL, elementName, NULL); RETURN_IF_FAILED(hr);
			insideElement = true;
		}

		return S_OK;
	};

	HRESULT CreateEndElement()
	{
		if (insideElement)
		{
			auto hr = writer->WriteEndElement(); RETURN_IF_FAILED(hr);
			insideElement = false;
		}

		return S_OK;
	}

	~EnsureElementCreated()
	{
		WI_ASSERT(!insideElement);
	}
};

struct PropValues
{
	vector_nothrow<ValueProperty> attributes;
	vector_nothrow<ObjectProperty> childObjects;
	vector_nothrow<ObjectCollectionProperty> childCollections;
};

struct FunctionIndices
{
	short getIndex;
	short putIndex;
};

using FunctionIndexMap = unordered_map_nothrow<MEMBERID, FunctionIndices>;

static HRESULT BuildFunctionIndexMap (ITypeInfo* typeInfo, FunctionIndexMap& functionIndices)
{
	HRESULT hr;

	TYPEATTR* typeAttr;
	hr = typeInfo->GetTypeAttr(&typeAttr); RETURN_IF_FAILED(hr);
	auto releaseTypeAttr = wil::scope_exit([&typeInfo, typeAttr] { typeInfo->ReleaseTypeAttr(typeAttr); });

	for (WORD i = 0; i < typeAttr->cFuncs; i++)
	{
		FUNCDESC* fd;
		hr = typeInfo->GetFuncDesc(i, &fd); RETURN_IF_FAILED(hr);
		auto releaseFuncDesc = wil::scope_exit([typeInfo, fd] { typeInfo->ReleaseFuncDesc(fd); });

		if (fd->invkind == INVOKE_PROPERTYGET || fd->invkind == INVOKE_PROPERTYPUT)
		{
			auto it = functionIndices.find(fd->memid);
			if (it == functionIndices.end())
			{
				bool inserted = functionIndices.try_insert({ fd->memid, { -1, -1 } }); RETURN_HR_IF(E_OUTOFMEMORY, !inserted);
				it = functionIndices.find(fd->memid);
			}
			
			if (fd->invkind == INVOKE_PROPERTYGET)
				it->second.getIndex = i;
			else
				it->second.putIndex = i;
		}
	}

	return S_OK;
}

static HRESULT ReadCollectionProperty (IDispatch* obj, MEMBERID memid, SAFEARRAY** ppsaChildren, ULONG* count)
{
	// An object that contains a property of type SAFEARRAY must implement IXmlParent.
	RETURN_HR_IF(E_UNEXPECTED, !wil::try_com_query_nothrow<IXmlParent>(obj));

	DISPPARAMS params = { };
	wil::unique_variant result;
	EXCEPINFO exception;
	UINT uArgErr;
	auto hr = obj->Invoke(memid, IID_NULL, InvariantLCID, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);

	unique_safearray sa (result.release().parray);
	VARTYPE vt;
	hr = SafeArrayGetVartype(sa.get(), &vt); RETURN_IF_FAILED(hr);
	RETURN_HR_IF(E_NOTIMPL, vt != VT_DISPATCH);
	UINT dim = SafeArrayGetDim(sa.get());
	RETURN_HR_IF(E_NOTIMPL, dim != 1);
	LONG lbound;
	hr = SafeArrayGetLBound(sa.get(), 1, &lbound); RETURN_IF_FAILED(hr);
	RETURN_HR_IF(E_NOTIMPL, lbound != 0);
	LONG ubound;
	hr = SafeArrayGetUBound(sa.get(), 1, &ubound); RETURN_IF_FAILED(hr);

	*ppsaChildren = sa.release();
	*count = (ULONG)ubound + 1;
	return S_OK;
}

static HRESULT ReadProperty (IDispatch* obj, ITypeInfo* typeInfo, MEMBERID memid, FunctionIndices indices, bool forceSerializeDefaults, bool isFactoryProp, PropValues& pv)
{
	HRESULT hr;

	FUNCDESC* fd;
	hr = typeInfo->GetFuncDesc(indices.getIndex, &fd); RETURN_IF_FAILED(hr);
	auto releaseFuncDesc = wil::scope_exit([&typeInfo,fd] { typeInfo->ReleaseFuncDesc(fd); });

	DISPPARAMS params = { };
	wil::unique_variant result;
	EXCEPINFO exception;
	UINT uArgErr;

	switch (fd->elemdescFunc.tdesc.vt)
	{
		case VT_UI1:
		case VT_UI2:
		case VT_UI4:
		case VT_I1:
		case VT_I2:
		case VT_I4:
			if (isFactoryProp
				|| (indices.putIndex != -1 && (forceSerializeDefaults || PropertyHasDefaultValue(obj, fd->memid, indices.getIndex) != S_OK)))
			{
				hr = typeInfo->Invoke(obj, fd->memid, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);
				hr = VariantChangeTypeEx(&result, &result, InvariantLCID, 0, VT_BSTR); RETURN_IF_FAILED(hr);
				auto value = wil::make_bstr_nothrow(V_BSTR(&result)); RETURN_IF_NULL_ALLOC(value);
				bool pushed = pv.attributes.try_push_back(ValueProperty{ fd->memid, std::move(value) }); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
			}
			break;

		case VT_BOOL:
			if (isFactoryProp
				|| (indices.putIndex != -1 && (forceSerializeDefaults || PropertyHasDefaultValue(obj, fd->memid, indices.getIndex) != S_OK)))
			{
				hr = typeInfo->Invoke(obj, fd->memid, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);
				bool val = (V_BOOL(&result) == VARIANT_TRUE);
				auto value = wil::make_bstr_nothrow(val ? L"True" : L"False"); RETURN_IF_NULL_ALLOC(value);
				bool pushed = pv.attributes.try_push_back(ValueProperty{ fd->memid, std::move(value) }); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
			}
			break;

		case VT_BSTR:
			if (isFactoryProp
				|| (indices.putIndex != -1 && (forceSerializeDefaults || PropertyHasDefaultValue(obj, fd->memid, indices.getIndex) != S_OK)))
			{
				hr = typeInfo->Invoke(obj, fd->memid, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);
				wil::unique_bstr value;
				if (result.bstrVal && result.bstrVal[0])
				{
					value = wil::make_bstr_nothrow(V_BSTR(&result)); RETURN_IF_NULL_ALLOC(value);
				}
				else
				{
					value = wil::make_bstr_nothrow(L""); RETURN_IF_NULL_ALLOC(value);
				}
				bool pushed = pv.attributes.try_push_back(ValueProperty{ fd->memid, std::move(value) }); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
			}
			break;

		case VT_USERDEFINED:
		{
			wil::com_ptr_nothrow<ITypeInfo> refTypeInfo;
			hr = typeInfo->GetRefTypeInfo(fd->elemdescFunc.tdesc.hreftype, &refTypeInfo); RETURN_IF_FAILED(hr);
			TYPEATTR* refTypeAttr;
			hr = refTypeInfo->GetTypeAttr(&refTypeAttr); RETURN_IF_FAILED(hr);
			auto releaseRefTypeAttr = wil::scope_exit([ti = refTypeInfo.get(), refTypeAttr] { ti->ReleaseTypeAttr(refTypeAttr); });
			if (refTypeAttr->typekind == TKIND_ENUM)
			{
				if (isFactoryProp
					|| (indices.putIndex != -1 && (forceSerializeDefaults || PropertyHasDefaultValue(obj, fd->memid, indices.getIndex) != S_OK)))
				{
					hr = typeInfo->Invoke(obj, fd->memid, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);
					wil::unique_bstr value;
					hr = GetNameFromEnumValue(refTypeInfo.get(), refTypeAttr, V_I4(&result), &value); RETURN_IF_FAILED(hr);
					bool pushed = pv.attributes.try_push_back(ValueProperty{ fd->memid, std::move(value) }); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
				}
			}
			else
				RETURN_HR(E_NOTIMPL);
		}
		break;

		case VT_PTR:
			if (isFactoryProp || forceSerializeDefaults || PropertyHasDefaultValue(obj, fd->memid, indices.getIndex) != S_OK)
			{
				// child object
				hr = typeInfo->Invoke(obj, fd->memid, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);
				wil::com_ptr_nothrow<ITypeInfo> refTypeInfo;
				hr = typeInfo->GetRefTypeInfo(fd->elemdescFunc.tdesc.lptdesc->hreftype, &refTypeInfo); RETURN_IF_FAILED(hr);
				TYPEATTR* refTypeAttr;
				hr = refTypeInfo->GetTypeAttr(&refTypeAttr); RETURN_IF_FAILED(hr);
				auto releaseRefTypeAttr = wil::scope_exit([ti = refTypeInfo.get(), refTypeAttr] { ti->ReleaseTypeAttr(refTypeAttr); });
				RETURN_HR_IF(E_FAIL, refTypeAttr->typekind != TKIND_DISPATCH);
				bool pushed = pv.childObjects.try_push_back(ObjectProperty{ fd->memid, V_DISPATCH(&result) }); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
			}
			break;

		case VT_SAFEARRAY:
			if (isFactoryProp || forceSerializeDefaults || PropertyHasDefaultValue(obj, fd->memid, indices.getIndex) != S_OK)
			{
				unique_safearray children;
				ULONG count;
				hr = ReadCollectionProperty (obj, fd->memid, &children, &count); RETURN_IF_FAILED(hr);
				bool pushed = pv.childCollections.try_push_back(ObjectCollectionProperty{ fd->memid, { } }); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
				for (LONG i = 0; i < (LONG)count; i++)
				{
					com_ptr<IDispatch> obj;
					hr = SafeArrayGetElement (children.get(), &i, &obj); RETURN_IF_FAILED(hr);
					pushed = pv.childCollections.back().value.try_push_back(std::move(obj)); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
				}
			}
			break;

		default:
			RETURN_HR(E_NOTIMPL);
	}

	return S_OK;
}

static HRESULT SaveToXmlInternal (IDispatch* obj, PCWSTR elementName, DWORD flags, IXmlWriter* writer,
								  EnsureElementCreated* ensureOuterElementCreated,
								  const DISPID* factoryProps = nullptr, ULONG factoryPropCount = 0);

static HRESULT WriteAttributes (std::span<ValueProperty const> attributes, ITypeInfo* typeInfo,
								IXmlWriter* writer, EnsureElementCreated& ensureElementCreated)
{
	if (attributes.size())
	{
		auto hr = ensureElementCreated.CreateStartElement(); RETURN_IF_FAILED(hr);

		for (auto& attr : attributes)
		{
			wil::unique_bstr name;
			UINT cNames;
			hr = typeInfo->GetNames(attr.dispid, &name, 1, &cNames); RETURN_IF_FAILED(hr);
			hr = writer->WriteAttributeString(NULL, name.get(), NULL, attr.value.get()); RETURN_IF_FAILED(hr);
		}
	}

	return S_OK;
};

static HRESULT WriteChildObjects (std::span<ObjectProperty const> childObjects, IDispatch* obj, ITypeInfo* typeInfo,
								  DWORD flags, IXmlWriter* writer, EnsureElementCreated& ensureElementCreated)
{
	if (childObjects.empty())
		return S_OK;

	com_ptr<IXmlParent> objAsParent;
	obj->QueryInterface(&objAsParent); // no error checking, an object with child objects can choose to implement this or not

	for (auto& child : childObjects)
	{
		wil::unique_bstr name;
		UINT cNames;
		auto hr = typeInfo->GetNames(child.dispid, &name, 1, &cNames); RETURN_IF_FAILED(hr);

		if (objAsParent)
		{
			wil::unique_bstr childXmlElementName;
			FactoryPropsPtr factoryProps;
			ULONG factoryPropCount = 0;
			hr = objAsParent->GetChildSerializeInfo (child.dispid, child.value.get(), &childXmlElementName, factoryProps.addressof(), &factoryPropCount);
			RETURN_HR_IF(hr, FAILED(hr) && hr != E_NOTIMPL);
			if (hr == E_NOTIMPL || childXmlElementName)
			{
				EnsureElementCreated ensureChildElemCreated (name.get(), writer, &ensureElementCreated);

				LPCWSTR elemName = childXmlElementName ? childXmlElementName.get() : name.get();
				hr = SaveToXmlInternal (child.value.get(), elemName, flags, writer,
										&ensureChildElemCreated, factoryProps.get(), factoryPropCount); RETURN_IF_FAILED(hr);

				hr = ensureChildElemCreated.CreateEndElement(); RETURN_IF_FAILED(hr);
			}
			else
			{
				hr = SaveToXmlInternal (child.value.get(), name.get(), flags, writer, &ensureElementCreated); RETURN_IF_FAILED(hr);
			}
		}
		else
		{
			hr = SaveToXmlInternal (child.value.get(), name.get(), flags, writer, &ensureElementCreated); RETURN_IF_FAILED(hr);
		}
	}

	return S_OK;
}

static HRESULT WriteChildCollections (std::span<ObjectCollectionProperty const> childCollections, IDispatch* obj, ITypeInfo* typeInfo,
									  DWORD flags, IXmlWriter* writer, EnsureElementCreated& ensureElementCreated)
{
	if (childCollections.empty())
		return S_OK;

	com_ptr<IXmlParent> objAsParent;
	auto hr = obj->QueryInterface(&objAsParent); RETURN_IF_FAILED(hr);

	for (auto& coll : childCollections)
	{
		wil::unique_bstr name;
		UINT cNames;
		hr = typeInfo->GetNames(coll.dispid, &name, 1, &cNames); RETURN_IF_FAILED(hr);

		EnsureElementCreated ensureCollectionElementCreated (name.get(), writer, &ensureElementCreated);

		for (auto& child : coll.value)
		{
			wil::unique_bstr xmlElementName;
			FactoryPropsPtr factoryProps;
			ULONG factoryPropCount = 0;
			hr = objAsParent->GetChildSerializeInfo (coll.dispid, child.get(), &xmlElementName, factoryProps.addressof(), &factoryPropCount); RETURN_IF_FAILED(hr);
			hr = SaveToXmlInternal (child.get(), xmlElementName.get(), flags, writer, &ensureCollectionElementCreated,
									factoryProps.get(), factoryPropCount); RETURN_IF_FAILED(hr);
		}

		hr = ensureCollectionElementCreated.CreateEndElement(); RETURN_IF_FAILED(hr);
	}

	return S_OK;
}

static HRESULT SaveToXmlInternal (IDispatch* obj, PCWSTR elementName, DWORD flags, IXmlWriter* writer,
								  EnsureElementCreated* ensureOuterElementCreated,
								  const DISPID* factoryProps, ULONG factoryPropCount)
{
	HRESULT hr;

	EnsureElementCreated ensureElementCreated (elementName, writer, ensureOuterElementCreated);
	
	// If we are the outmost XML element, we must create it even if there's nothing to save to it.
	// That's because XmlLite doesn't seem to support writing an empty document (WriteEndDocument() will fail).
	if (!ensureOuterElementCreated)
	{
		hr = ensureElementCreated.CreateStartElement(); RETURN_IF_FAILED(hr);
	}

	wil::com_ptr_nothrow<IXmlParent> objAsXmlParent;
	hr = obj->QueryInterface(&objAsXmlParent); RETURN_HR_IF(hr, FAILED(hr) && (hr != E_NOINTERFACE));

	com_ptr<ITypeInfo> typeInfo;
	hr = obj->GetTypeInfo(0, InvariantLCID, &typeInfo); RETURN_IF_FAILED(hr);
	
	FunctionIndexMap functionIndices;
	hr = BuildFunctionIndexMap(typeInfo.get(), functionIndices); RETURN_IF_FAILED(hr);

	bool forceSerializeDefaults = flags & SAVE_XML_FORCE_SERIALIZE_DEFAULTS;

	PropValues pv;

	// Read factory props.
	for (ULONG i = 0; i < factoryPropCount; i++)
	{
		auto it = functionIndices.find(factoryProps[i]);
		RETURN_HR_IF(E_UNEXPECTED, it == functionIndices.end());
		RETURN_HR_IF(E_UNEXPECTED, it->second.getIndex < 0);
		hr = ReadProperty (obj, typeInfo, factoryProps[i], it->second, forceSerializeDefaults, true, pv); RETURN_IF_FAILED(hr);
		functionIndices.erase(it);
	}

	// Serialize factory props.
	RETURN_HR_IF(E_UNEXPECTED, !pv.childObjects.empty()); // we don't support this as factory props
	RETURN_HR_IF(E_UNEXPECTED, !pv.childCollections.empty()); // we don't support this as factory props
	hr = WriteAttributes(pv.attributes, typeInfo, writer, ensureElementCreated); RETURN_IF_FAILED(hr);
	pv.attributes.clear();

	for (auto& [memid, indices] : functionIndices)
	{
		RETURN_HR_IF(E_UNEXPECTED, indices.getIndex < 0);
		hr = ReadProperty (obj, typeInfo, memid, indices, forceSerializeDefaults, false, pv); RETURN_IF_FAILED(hr);
	}

	hr = WriteAttributes(pv.attributes, typeInfo, writer, ensureElementCreated); RETURN_IF_FAILED(hr);
	hr = WriteChildObjects(pv.childObjects, obj, typeInfo, flags, writer, ensureElementCreated); RETURN_IF_FAILED(hr);
	hr = WriteChildCollections(pv.childCollections, obj, typeInfo, flags, writer, ensureElementCreated); RETURN_IF_FAILED(hr);

	hr = ensureElementCreated.CreateEndElement(); RETURN_IF_FAILED(hr);

	return S_OK;
}

HRESULT SaveToXml (IDispatch* obj, PCWSTR elementName, DWORD flags, IStream* to, UINT nEncodingCodePage)
{
	wil::com_ptr_nothrow<IXmlWriter> writer;
	auto hr = CreateXmlWriter(IID_PPV_ARGS(&writer), nullptr); RETURN_IF_FAILED(hr);
	if (nEncodingCodePage != CP_UTF8)
	{
		com_ptr<IXmlWriterOutput> output;
		hr = CreateXmlWriterOutputWithEncodingCodePage (to, nullptr, nEncodingCodePage, &output); RETURN_IF_FAILED(hr);
		hr = writer->SetOutput(output.get()); RETURN_IF_FAILED(hr);
	}
	else
	{
		hr = writer->SetOutput(to); RETURN_IF_FAILED(hr);
	}
	hr = writer->SetProperty(XmlWriterProperty_Indent, TRUE);
	hr = writer->WriteStartDocument(XmlStandalone_Omit); RETURN_IF_FAILED(hr);
	hr = SaveToXmlInternal (obj, elementName, flags, writer.get(), nullptr, nullptr, 0); RETURN_IF_FAILED(hr);
	hr = writer->WriteEndDocument(); RETURN_IF_FAILED(hr);

	return S_OK;
}

// ============================================================================

static HRESULT LoadFromXmlInternal (IDispatch* parent, MEMBERID memid, IXmlReader* reader, PCWSTR elementName, IDispatch* pObjIn, IDispatch** ppObjOut);

static HRESULT LoadReadOnlyCollection (IXmlReader* reader, IDispatch* obj, MEMBERID memid)
{
	HRESULT hr;

	unique_safearray children;
	ULONG childCount;
	hr = ReadCollectionProperty (obj, memid, &children, &childCount); RETURN_IF_FAILED(hr);

	com_ptr<IXmlParent> objAsParent;
	hr = obj->QueryInterface(&objAsParent); RETURN_IF_FAILED(hr);

	ULONG index = 0;
	while(true)
	{
		XmlNodeType nodeType;
		hr = reader->Read(&nodeType); RETURN_IF_FAILED(hr);
		if (nodeType == XmlNodeType_Whitespace || nodeType == XmlNodeType_Comment)
		{
		}
		else if (nodeType == XmlNodeType_Element)
		{
			LPCWSTR entryName;
			hr = reader->GetLocalName(&entryName, nullptr); RETURN_IF_FAILED(hr);
			com_ptr<IDispatch> child;
			LONG i = (LONG)index;
			hr = SafeArrayGetElement(children.get(), &i, &child); RETURN_IF_FAILED(hr);

			wil::unique_bstr expectedElementName;
			hr = objAsParent->GetChildSerializeInfo(memid, child, &expectedElementName, nullptr, nullptr); RETURN_IF_FAILED(hr);
			RETURN_HR_IF(E_UNEXPECTED, wcscmp(entryName, expectedElementName.get()));

			hr = LoadFromXmlInternal (obj, memid, reader, entryName, child, nullptr); RETURN_IF_FAILED(hr);
			index++;
		}
		else if (nodeType == XmlNodeType_EndElement)
		{
			return S_OK;
		}
		else
			RETURN_HR(E_NOTIMPL);
	}
}

static HRESULT LoadCollection (IXmlReader* reader, IDispatch* obj, MEMBERID memid, SAFEARRAY** to)
{
	HRESULT hr;

	*to = nullptr;

	vector_nothrow<com_ptr<IDispatch>> children;

	ULONG index = 0;
	while(true)
	{
		XmlNodeType nodeType;
		hr = reader->Read(&nodeType); RETURN_IF_FAILED(hr);
		if (nodeType == XmlNodeType_Whitespace || nodeType == XmlNodeType_Comment)
		{
		}
		else if (nodeType == XmlNodeType_Element)
		{
			LPCWSTR entryName;
			hr = reader->GetLocalName(&entryName, nullptr); RETURN_IF_FAILED(hr);
			com_ptr<IDispatch> child;
			hr = LoadFromXmlInternal (obj, memid, reader, entryName, nullptr, &child); RETURN_IF_FAILED(hr);
			bool pushed = children.try_push_back (std::move(child)); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
			index++;
		}
		else if (nodeType == XmlNodeType_EndElement)
		{
			LPCWSTR endElementName;
			hr = reader->GetLocalName(&endElementName, nullptr); RETURN_IF_FAILED(hr);
			SAFEARRAYBOUND sabound = { .cElements = (ULONG)children.size(), .lLbound = 0 };
			SAFEARRAY* sa = SafeArrayCreate (VT_DISPATCH, 1, &sabound); RETURN_IF_NULL_ALLOC(sa);
			auto freeSafeArray = wil::scope_exit([&sa] { SafeArrayDestroy(sa); sa = nullptr; });
			for (LONG i = 0; i < (LONG)children.size(); i++)
			{
				hr = SafeArrayPutElement (sa, &i, children[i].get()); RETURN_IF_FAILED(hr);
			}
			freeSafeArray.release();
			*to = sa;
			return S_OK;
		}
		else
			RETURN_HR(E_NOTIMPL);
	}
}

static HRESULT ReadAttributeToVariant (IXmlReader* reader, ITypeInfo* typeInfo, FunctionIndexMap& functionIndices, MEMBERID& memid, wil::unique_variant& valueVariant)
{
	HRESULT hr;

	LPCWSTR attrName, attrValue;
	UINT attrNameLen, attrValueLen;
	hr = reader->GetLocalName(&attrName, &attrNameLen); RETURN_IF_FAILED(hr);
	hr = reader->GetValue(&attrValue, &attrValueLen); RETURN_IF_FAILED(hr);

	// We intentionally read the descriptor of the "get" function, not that of the "put" function.
	// That's because we might be reading the XML attribute for a factory property, which might have just the "get" function.
	
	hr = typeInfo->GetIDsOfNames(&const_cast<LPOLESTR&>(attrName), 1, &memid); RETURN_IF_FAILED(hr);
	auto indices = functionIndices.find(memid);
	RETURN_HR_IF(DISP_E_MEMBERNOTFOUND, indices == functionIndices.end());
	RETURN_HR_IF(DISP_E_MEMBERNOTFOUND, indices->second.getIndex == -1);
	FUNCDESC* getfd;
	hr = typeInfo->GetFuncDesc(indices->second.getIndex, &getfd); RETURN_IF_FAILED(hr);
	auto releaseFD = wil::scope_exit([typeInfo,getfd] { typeInfo->ReleaseFuncDesc(getfd); });

	VARTYPE vt = getfd->elemdescFunc.tdesc.vt;
	switch (vt)
	{
		case VT_UI1:
		case VT_UI2:
		case VT_UI4:
		case VT_I1:
		case VT_I2:
		case VT_I4:
		{
			hr = InitVariantFromString(attrValue, &valueVariant); RETURN_IF_FAILED(hr);
			hr = VariantChangeTypeEx (&valueVariant, &valueVariant, InvariantLCID, 0, vt); RETURN_IF_FAILED(hr);
			break;
		}

		case VT_BSTR:
			hr = InitVariantFromString(attrValue, &valueVariant); RETURN_IF_FAILED(hr);
			break;

		case VT_BOOL:
			hr = InitVariantFromBoolean(!wcscmp(attrValue, L"True"), &valueVariant); RETURN_IF_FAILED(hr);
			break;

		case VT_USERDEFINED:
		{
			com_ptr<ITypeInfo> refTypeInfo;
			hr = typeInfo->GetRefTypeInfo (getfd->elemdescFunc.tdesc.hreftype, &refTypeInfo); RETURN_IF_FAILED(hr);
			TYPEATTR* refTypeAttr;
			hr = refTypeInfo->GetTypeAttr(&refTypeAttr); RETURN_IF_FAILED(hr);
			auto releaseRefTypeAttr = wil::scope_exit([ti=refTypeInfo.get(), refTypeAttr] { ti->ReleaseTypeAttr(refTypeAttr); });
			if (refTypeAttr->typekind == TKIND_ENUM)
			{
				LONG value;
				hr = GetEnumValueFromName (refTypeInfo, refTypeAttr, attrValue, &value); RETURN_IF_FAILED(hr);
				hr = InitVariantFromInt32 (value, &valueVariant); RETURN_IF_FAILED(hr);
			}
			else
				RETURN_HR(E_NOTIMPL);
			break;
		}
		default:
			RETURN_HR(E_NOTIMPL);
	}

	return S_OK;
}

static HRESULT LoadFromXmlInternal (IDispatch* parent, MEMBERID memid, IXmlReader* reader,
									PCWSTR elementName, IDispatch* pObjIn, IDispatch** ppObjOut)
{
	HRESULT hr;

	com_ptr<ITypeInfo> typeInfo;
	FunctionIndexMap functionIndices;
	com_ptr<IDispatch> obj;

	HRESULT hrMoveToAttribute;
	if (!pObjIn)
	{
		_ASSERT(ppObjOut);

		com_ptr<IXmlParent> objAsParent;
		hr = parent->QueryInterface(&objAsParent); RETURN_IF_FAILED(hr);

		FactoryPropsPtr factoryProps;
		ULONG factoryPropCount;
		hr = objAsParent->GetChildDeserializeInfo (memid, elementName, &typeInfo, &factoryProps, &factoryPropCount); RETURN_IF_FAILED(hr);

		hr = BuildFunctionIndexMap (typeInfo, functionIndices); RETURN_IF_FAILED(hr);

		if (factoryPropCount == 0)
		{
			hr = objAsParent->CreateChild (memid, elementName, nullptr, 0, &obj); RETURN_IF_FAILED(hr);
			hrMoveToAttribute = reader->MoveToFirstAttribute(); RETURN_IF_FAILED(hr);
		}
		else
		{
			vector_nothrow<wil::unique_variant> factoryPropValues;
			hr = reader->MoveToFirstAttribute(); RETURN_IF_FAILED(hr);
			if (hr != S_OK)
				RETURN_HR(E_UNEXPECTED); // expected more XML attributes for the factory props
			while(true)
			{
				MEMBERID id;
				wil::unique_variant valueVariant;
				hr = ReadAttributeToVariant(reader, typeInfo, functionIndices, id, valueVariant); RETURN_IF_FAILED(hr);
				RETURN_HR_IF(E_UNEXPECTED, id != factoryProps.get()[factoryPropValues.size()]);
				bool pushed = factoryPropValues.try_push_back(std::move(valueVariant)); RETURN_HR_IF(E_OUTOFMEMORY, !pushed);
				if (factoryPropValues.size() == factoryPropCount)
					break;
				hr = reader->MoveToNextAttribute(); RETURN_IF_FAILED(hr);
				if (hr != S_OK)
					RETURN_HR(E_UNEXPECTED); // expected more XML attributes for the factory props
			}

			hr = objAsParent->CreateChild (memid, elementName, factoryPropValues.data(), factoryPropValues.size(),
										   &obj); RETURN_IF_FAILED(hr);
			hrMoveToAttribute = reader->MoveToNextAttribute(); RETURN_IF_FAILED(hr);
		}
	}
	else
	{
		_ASSERT(!ppObjOut);
		hr = pObjIn->QueryInterface(&obj); RETURN_IF_FAILED(hr);
		hr = obj->GetTypeInfo(0, InvariantLCID, &typeInfo); RETURN_IF_FAILED(hr);
		hr = BuildFunctionIndexMap(typeInfo, functionIndices); RETURN_IF_FAILED(hr);
		hrMoveToAttribute = reader->MoveToFirstAttribute(); RETURN_IF_FAILED(hr);
	}

	while (hrMoveToAttribute == S_OK)
	{
		wil::unique_variant valueVariant;
		hr = ReadAttributeToVariant(reader, typeInfo, functionIndices, memid, valueVariant); RETURN_IF_FAILED(hr);

		DISPID named = DISPID_PROPERTYPUT;
		DISPPARAMS params = { .rgvarg = &valueVariant, .rgdispidNamedArgs=&named, .cArgs = 1, .cNamedArgs = 1 };
		wil::unique_variant result; // TODO: get rid of this
		EXCEPINFO exception;
		UINT uArgErr;
		hr = typeInfo->Invoke(obj, memid, DISPATCH_PROPERTYPUT, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);

		hrMoveToAttribute = reader->MoveToNextAttribute();
	}

	reader->MoveToElement();

	if (!reader->IsEmptyElement())
	{
		com_ptr<IXmlParent> objAsParent;
		obj->QueryInterface(&objAsParent);

		// Try to read child elements.
		while (true)
		{
			XmlNodeType nodeType;
			hr = reader->Read(&nodeType); RETURN_IF_FAILED(hr);
			if (nodeType == XmlNodeType_Whitespace)
			{
			}
			else if (nodeType == XmlNodeType_Element)
			{
				LPCWSTR childElemName = nullptr;
				hr = reader->GetLocalName(&childElemName, nullptr); RETURN_IF_FAILED(hr);

				MEMBERID memid;
				bool readOnly = false;
				hr = typeInfo->GetIDsOfNames(&const_cast<LPOLESTR&>(childElemName), 1, &memid); RETURN_IF_FAILED(hr);
				//VARTYPE vt;
				//hr = FindPutFunction(memid, typeInfo.get(), typeAttr, &vt); RETURN_HR_IF(hr, FAILED(hr) && hr != DISP_E_MEMBERNOTFOUND);
				auto it = functionIndices.find(memid);
				if (it == functionIndices.end())
				{
					// We have an XML element but no getter and no setter for it.
					// Either it's a malformed XML, or we have a bug.
					RETURN_HR(DISP_E_MEMBERNOTFOUND);
				}

				VARTYPE vt;
				if (it->second.putIndex >= 0)
				{
					FUNCDESC* fd;
					hr = typeInfo->GetFuncDesc(it->second.putIndex, &fd); RETURN_IF_FAILED(hr);
					auto releasefd = wil::scope_exit([typeInfo, fd] { typeInfo->ReleaseFuncDesc(fd); });
					RETURN_HR_IF(E_UNEXPECTED, fd->invkind != INVOKE_PROPERTYPUT);
					RETURN_HR_IF(E_UNEXPECTED, fd->memid != memid);
					RETURN_HR_IF(E_UNEXPECTED, fd->cParams != 1);
					vt = fd->lprgelemdescParam[0].tdesc.vt;
				}
				else
				{
					// We have an XML element with a getter and no setter. Must be a read-only object-property
					// initialized by its owner. We should be able to get a non-null value, otherwise we have a bug.
					FUNCDESC* fd;
					hr = typeInfo->GetFuncDesc(it->second.getIndex, &fd); RETURN_IF_FAILED(hr);
					auto releasefd = wil::scope_exit([typeInfo, fd] { typeInfo->ReleaseFuncDesc(fd); });
					RETURN_HR_IF(E_UNEXPECTED, fd->invkind != INVOKE_PROPERTYGET);
					RETURN_HR_IF(E_UNEXPECTED, fd->memid != memid);
					RETURN_HR_IF(E_UNEXPECTED, fd->cParams != 0);
					vt = fd->elemdescFunc.tdesc.vt;
					readOnly = true;
				}

				if (vt == VT_SAFEARRAY)
				{
					if (!readOnly)
					{
						SAFEARRAY* sa = nullptr;
						hr = LoadCollection(reader, obj, memid, &sa); RETURN_IF_FAILED(hr);
						wil::unique_variant value;
						value.vt = VT_ARRAY | VT_DISPATCH;
						value.parray = sa;
						sa = nullptr;
						DISPID named = DISPID_PROPERTYPUT;
						DISPPARAMS params = { .rgvarg = &value, .rgdispidNamedArgs=&named, .cArgs = 1, .cNamedArgs = 1 };
						wil::unique_variant result;
						EXCEPINFO exception;
						UINT uArgErr;
						hr = typeInfo->Invoke (obj, memid, DISPATCH_PROPERTYPUT, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);
					}
					else
					{
						hr = LoadReadOnlyCollection(reader, obj, memid); RETURN_IF_FAILED(hr);
					}
				}
				else if (vt == VT_PTR)
				{
					RETURN_HR(E_NOTIMPL);
					/*
					if (!readOnly)
					{
						// We get here, for example, for IProjectConfig::put_GeneralProperties
						com_ptr<IDispatch> child;
						hr = objAsParent->CreateChild(memid, childElemName, &child); RETURN_IF_FAILED(hr);
						RETURN_HR(E_NOTIMPL);
						hr = LoadFromXmlInternal (reader, childElemName, child.get()); RETURN_IF_FAILED(hr);
						wil::unique_variant value;
						hr = InitVariantFromDispatch(child.get(), &value); RETURN_IF_FAILED(hr);
						DISPID named = DISPID_PROPERTYPUT;
						DISPPARAMS params = { .rgvarg = &value, .rgdispidNamedArgs=&named, .cArgs = 1, .cNamedArgs = 1 };
						EXCEPINFO exception;
						UINT uArgErr;
						hr = typeInfo->Invoke (obj, memid, DISPATCH_PROPERTYPUT, &params, nullptr, &exception, &uArgErr); RETURN_IF_FAILED(hr);
					}
					else
					{
						DISPPARAMS params = { };
						wil::unique_variant result;
						EXCEPINFO exception;
						UINT uArgErr;
						hr = typeInfo->Invoke (obj, memid, DISPATCH_PROPERTYGET, &params, &result, &exception, &uArgErr); RETURN_IF_FAILED(hr);
						RETURN_HR_IF(E_UNEXPECTED, result.vt != VT_DISPATCH);
						hr = LoadFromXmlInternal (reader, childElemName, V_DISPATCH(&result)); RETURN_IF_FAILED(hr);
					}
					*/
				}
				else
				{
					RETURN_HR(E_NOTIMPL);
				}
			}
			else if (nodeType == XmlNodeType_EndElement)
			{
				LPCWSTR endElemName;
				hr = reader->GetLocalName(&endElemName, nullptr); RETURN_IF_FAILED(hr);
				RETURN_HR_IF(E_UNEXPECTED, wcscmp(elementName, endElemName));
				break;
			}
			else
			{
				WI_ASSERT(false); // TODO
			}
		}
	}

	if (ppObjOut)
		*ppObjOut = obj.detach();

	return S_OK;
}

HRESULT LoadFromXml (IDispatch* obj, _In_opt_ PCWSTR expectedElementName, IStream* stream, UINT nEncodingCodePage)
{
	wil::com_ptr_nothrow<IXmlReader> reader;
	auto hr = CreateXmlReader(IID_PPV_ARGS(&reader), nullptr); RETURN_IF_FAILED(hr);
	if (nEncodingCodePage != CP_UTF8)
	{
		com_ptr<IXmlReaderInput> input;
		hr = CreateXmlReaderInputWithEncodingCodePage (stream, nullptr, nEncodingCodePage, TRUE, nullptr, &input); RETURN_IF_FAILED(hr);
		hr = reader->SetInput(stream); RETURN_IF_FAILED(hr);
	}
	else
	{
		hr = reader->SetInput(stream); RETURN_IF_FAILED(hr);
	}

	XmlNodeType nodeType;
	hr = reader->Read(&nodeType); RETURN_IF_FAILED(hr);
	if (nodeType != XmlNodeType_XmlDeclaration)
		RETURN_HR((HRESULT)WC_E_XMLDECL);
	while(SUCCEEDED(reader->Read(&nodeType)) && (nodeType == XmlNodeType_Whitespace))
		;

	RETURN_HR_IF((HRESULT)WC_E_DECLELEMENT, nodeType != XmlNodeType_Element);
	hr = reader->MoveToElement(); RETURN_IF_FAILED(hr);
	LPCWSTR elemName;
	hr = reader->GetLocalName(&elemName, nullptr); RETURN_IF_FAILED(hr);
	RETURN_HR_IF((HRESULT)WC_E_DECLELEMENT, expectedElementName && wcscmp(elemName, expectedElementName));

	return LoadFromXmlInternal (nullptr, MEMBERID_NIL, reader, elemName, obj, nullptr);
}
