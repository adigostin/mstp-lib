#pragma once

// Note AGO: Reason for this interface is the following: the XML loader needs a way to create objects
// out of the XML elements it encounters. The "traditional" way is to register somehow factories
// for each type of object, and pass these factories to the XML loader. This is already complicated,
// and moreover brings the restriction that every class must be constructible without parameters.
// (Declaring and using parameters to factories is overly complicated and not worth exploring.)
//
// After a lot of trial and error, I ended up with a model in which any serializable object which
// owns other serializable objects in its properties implements this interface. While saving to XML,
// the XML saving code calls GetChildXmlElementName. While loading from XML, the XML loading code
// calls CreateChild.
struct DECLSPEC_NOVTABLE DECLSPEC_UUID("45B35EF7-DC2B-4EE3-BB44-EC25D607BFCE") IXmlParent : IUnknown
{
	// For an object property (not a value and not a collection) that has a single possible implementation,
	// this function can return a NULL name and S_OK to signal that the XML serializer should serialize
	// the child object directly on the XML element that corresponds to the property. This improves the readability
	// of the XML file. Normally, for such properties, the serializer creates something like this:
	// <ParentClassName>
	//   ...
	//   <PropertyName>                                     <= name of the property that comes from the parent's type info
	//     <ChildObjectClassName ... child attributes ...>  <= name that comes from this function
	//   </PropertyName>
	// </ParentClassName>
	// If this function returns a NULL name, the serializer can simplify this to:
	// <ParentClassName>
	//   ...
	//   <PropertyName ... child attributes ...>
	// </ParentClassName>
	//
	// On success, the callee allocates an array of DISPIDs using CoTaskMemAlloc and
	// returns it in *pDispidsOut, with the number of elements in *cDispidsOut. The
	// caller is responsible for freeing the array using CoTaskMemFree. If no DISPIDs
	// are returned, *pDispidsOut is set to NULL and *cDispidsOut to 0.
	virtual HRESULT STDMETHODCALLTYPE GetChildSerializeInfo (
		_In_ DISPID dispidProperty,
		_In_ IDispatch* pChild,
		_Outptr_ BSTR* pbstrXmlElementName,
		_Outptr_opt_result_buffer_(*pcFactoryDispids) DISPID** ppFactoryDispids,
		_Out_ ULONG* pcFactoryDispids) = 0;

	virtual HRESULT STDMETHODCALLTYPE GetChildDeserializeInfo (
		_In_ DISPID dispidProperty,
		_In_ PCWSTR xmlElementName,
		_Outptr_ ITypeInfo** ppTypeInfo,
		_Outptr_result_buffer_(*pcFactoryDispids) DISPID** ppFactoryDispids,
		_Out_ ULONG* pcFactoryDispids) = 0;

	virtual HRESULT STDMETHODCALLTYPE CreateChild (
		_In_ DISPID dispidProperty,
		_In_ PCWSTR xmlElementName,
		_In_reads_(factoryPropCount) const VARIANT* factoryPropValues,
		_In_ ULONG factoryPropCount,
		_Outptr_ IDispatch** childOut) = 0;
};

typedef enum
{
	// Forces the serializer to generate XML even for properties with default values.
	// (This flag does not affect properties with no setter that are serializable as XML attributes;
	//  such properties are not serialized even in the presence of SAVE_XML_FORCE_SERIALIZE_DEFAULTS.)
	SAVE_XML_FORCE_SERIALIZE_DEFAULTS = 1,
} SaveXmlFlags;

HRESULT SaveToXml (IDispatch* obj, PCWSTR elementName, DWORD flags, IStream* stream, UINT nEncodingCodePage = CP_UTF8);

HRESULT LoadFromXml (IDispatch* obj, _In_opt_ PCWSTR expectedElementName, IStream* stream, UINT nEncodingCodePage = CP_UTF8);
