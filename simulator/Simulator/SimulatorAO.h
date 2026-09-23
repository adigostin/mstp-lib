

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0622 */
/* at Tue Jan 19 05:14:07 2038
 */
/* Compiler settings for Automation\SimulatorAO.idl:
    Oicf, W1, Zp8, env=Win32 (32b run), target_arch=X86 8.01.0622 
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
/* @@MIDL_FILE_HEADING(  ) */



/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */


#ifndef __SimulatorAO_h__
#define __SimulatorAO_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __ISimulatorAppAO_FWD_DEFINED__
#define __ISimulatorAppAO_FWD_DEFINED__
typedef interface ISimulatorAppAO ISimulatorAppAO;

#endif 	/* __ISimulatorAppAO_FWD_DEFINED__ */


#ifndef __IProjectAO_FWD_DEFINED__
#define __IProjectAO_FWD_DEFINED__
typedef interface IProjectAO IProjectAO;

#endif 	/* __IProjectAO_FWD_DEFINED__ */


#ifndef __IBridgeAO_FWD_DEFINED__
#define __IBridgeAO_FWD_DEFINED__
typedef interface IBridgeAO IBridgeAO;

#endif 	/* __IBridgeAO_FWD_DEFINED__ */


#ifndef __IPortAO_FWD_DEFINED__
#define __IPortAO_FWD_DEFINED__
typedef interface IPortAO IPortAO;

#endif 	/* __IPortAO_FWD_DEFINED__ */


#ifndef __IWireAO_FWD_DEFINED__
#define __IWireAO_FWD_DEFINED__
typedef interface IWireAO IWireAO;

#endif 	/* __IWireAO_FWD_DEFINED__ */


#ifndef __IProjectWindowAO_FWD_DEFINED__
#define __IProjectWindowAO_FWD_DEFINED__
typedef interface IProjectWindowAO IProjectWindowAO;

#endif 	/* __IProjectWindowAO_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "SimulatorIDL.h"

#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __SimulatorAutomationLib_LIBRARY_DEFINED__
#define __SimulatorAutomationLib_LIBRARY_DEFINED__

/* library SimulatorAutomationLib */
/* [version][uuid] */ 






EXTERN_C const IID LIBID_SimulatorAutomationLib;

#ifndef __ISimulatorAppAO_INTERFACE_DEFINED__
#define __ISimulatorAppAO_INTERFACE_DEFINED__

/* interface ISimulatorAppAO */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_ISimulatorAppAO;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("62BAA56C-5C16-4A6C-9D26-9D2C2E95A901")
    ISimulatorAppAO : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetProjectWindow( 
            LONG hWnd,
            /* [retval][out] */ IProjectWindowAO **ppProjectWindow) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OpenWindowForVlan( 
            /* [in] */ IProjectAO *projectAO,
            DWORD vlanNumber,
            /* [retval][out] */ LONG *pHWnd) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE EnableFailFastOnAssertions( void) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct ISimulatorAppAOVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            ISimulatorAppAO * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            ISimulatorAppAO * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            ISimulatorAppAO * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            ISimulatorAppAO * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            ISimulatorAppAO * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            ISimulatorAppAO * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            ISimulatorAppAO * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        HRESULT ( STDMETHODCALLTYPE *GetProjectWindow )( 
            ISimulatorAppAO * This,
            LONG hWnd,
            /* [retval][out] */ IProjectWindowAO **ppProjectWindow);
        
        HRESULT ( STDMETHODCALLTYPE *OpenWindowForVlan )( 
            ISimulatorAppAO * This,
            /* [in] */ IProjectAO *projectAO,
            DWORD vlanNumber,
            /* [retval][out] */ LONG *pHWnd);
        
        HRESULT ( STDMETHODCALLTYPE *EnableFailFastOnAssertions )( 
            ISimulatorAppAO * This);
        
        END_INTERFACE
    } ISimulatorAppAOVtbl;

    interface ISimulatorAppAO
    {
        CONST_VTBL struct ISimulatorAppAOVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define ISimulatorAppAO_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define ISimulatorAppAO_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define ISimulatorAppAO_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define ISimulatorAppAO_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define ISimulatorAppAO_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define ISimulatorAppAO_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define ISimulatorAppAO_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define ISimulatorAppAO_GetProjectWindow(This,hWnd,ppProjectWindow)	\
    ( (This)->lpVtbl -> GetProjectWindow(This,hWnd,ppProjectWindow) ) 

#define ISimulatorAppAO_OpenWindowForVlan(This,projectAO,vlanNumber,pHWnd)	\
    ( (This)->lpVtbl -> OpenWindowForVlan(This,projectAO,vlanNumber,pHWnd) ) 

#define ISimulatorAppAO_EnableFailFastOnAssertions(This)	\
    ( (This)->lpVtbl -> EnableFailFastOnAssertions(This) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __ISimulatorAppAO_INTERFACE_DEFINED__ */


#ifndef __IProjectAO_INTERFACE_DEFINED__
#define __IProjectAO_INTERFACE_DEFINED__

/* interface IProjectAO */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IProjectAO;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7B6D20D3-BF5D-4A98-9D32-3B9E30BCE902")
    IProjectAO : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE AddBridge( 
            DWORD portCount,
            DWORD mstiCount,
            /* [retval][out] */ IBridgeAO **ppBridgeAO) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE AddWire( 
            IPortAO *port0,
            IPortAO *port1,
            /* [retval][out] */ IWireAO **ppWire) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IProjectAOVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IProjectAO * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IProjectAO * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IProjectAO * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IProjectAO * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IProjectAO * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IProjectAO * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IProjectAO * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        HRESULT ( STDMETHODCALLTYPE *AddBridge )( 
            IProjectAO * This,
            DWORD portCount,
            DWORD mstiCount,
            /* [retval][out] */ IBridgeAO **ppBridgeAO);
        
        HRESULT ( STDMETHODCALLTYPE *AddWire )( 
            IProjectAO * This,
            IPortAO *port0,
            IPortAO *port1,
            /* [retval][out] */ IWireAO **ppWire);
        
        END_INTERFACE
    } IProjectAOVtbl;

    interface IProjectAO
    {
        CONST_VTBL struct IProjectAOVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IProjectAO_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IProjectAO_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IProjectAO_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IProjectAO_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IProjectAO_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IProjectAO_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IProjectAO_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IProjectAO_AddBridge(This,portCount,mstiCount,ppBridgeAO)	\
    ( (This)->lpVtbl -> AddBridge(This,portCount,mstiCount,ppBridgeAO) ) 

#define IProjectAO_AddWire(This,port0,port1,ppWire)	\
    ( (This)->lpVtbl -> AddWire(This,port0,port1,ppWire) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IProjectAO_INTERFACE_DEFINED__ */


#ifndef __IBridgeAO_INTERFACE_DEFINED__
#define __IBridgeAO_INTERFACE_DEFINED__

/* interface IBridgeAO */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IBridgeAO;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7D45685F-5836-41A2-AA0C-6B567EF957A6")
    IBridgeAO : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetPort( 
            DWORD portIndex,
            /* [retval][out] */ IPortAO **ppPort) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE LoadTestMstConfig1( void) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_STPVersion( 
            /* [retval][out] */ enum STPVersion *pVersion) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_STPVersion( 
            enum STPVersion version) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IBridgeAOVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IBridgeAO * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IBridgeAO * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IBridgeAO * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IBridgeAO * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IBridgeAO * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IBridgeAO * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IBridgeAO * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        HRESULT ( STDMETHODCALLTYPE *GetPort )( 
            IBridgeAO * This,
            DWORD portIndex,
            /* [retval][out] */ IPortAO **ppPort);
        
        HRESULT ( STDMETHODCALLTYPE *LoadTestMstConfig1 )( 
            IBridgeAO * This);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_STPVersion )( 
            IBridgeAO * This,
            /* [retval][out] */ enum STPVersion *pVersion);
        
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_STPVersion )( 
            IBridgeAO * This,
            enum STPVersion version);
        
        END_INTERFACE
    } IBridgeAOVtbl;

    interface IBridgeAO
    {
        CONST_VTBL struct IBridgeAOVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBridgeAO_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IBridgeAO_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IBridgeAO_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IBridgeAO_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IBridgeAO_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IBridgeAO_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IBridgeAO_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IBridgeAO_GetPort(This,portIndex,ppPort)	\
    ( (This)->lpVtbl -> GetPort(This,portIndex,ppPort) ) 

#define IBridgeAO_LoadTestMstConfig1(This)	\
    ( (This)->lpVtbl -> LoadTestMstConfig1(This) ) 

#define IBridgeAO_get_STPVersion(This,pVersion)	\
    ( (This)->lpVtbl -> get_STPVersion(This,pVersion) ) 

#define IBridgeAO_put_STPVersion(This,version)	\
    ( (This)->lpVtbl -> put_STPVersion(This,version) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IBridgeAO_INTERFACE_DEFINED__ */


#ifndef __IPortAO_INTERFACE_DEFINED__
#define __IPortAO_INTERFACE_DEFINED__

/* interface IPortAO */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IPortAO;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("8C0A4A49-5C72-4D8E-9F0E-4A53E4DC8D31")
    IPortAO : public IDispatch
    {
    public:
    };
    
    
#else 	/* C style interface */

    typedef struct IPortAOVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPortAO * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPortAO * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPortAO * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IPortAO * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IPortAO * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IPortAO * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IPortAO * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        END_INTERFACE
    } IPortAOVtbl;

    interface IPortAO
    {
        CONST_VTBL struct IPortAOVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPortAO_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPortAO_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPortAO_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPortAO_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IPortAO_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IPortAO_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IPortAO_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPortAO_INTERFACE_DEFINED__ */


#ifndef __IWireAO_INTERFACE_DEFINED__
#define __IWireAO_INTERFACE_DEFINED__

/* interface IWireAO */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IWireAO;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4D1E4B6D-7D2C-4E76-9A65-9D6A4BD6B0B1")
    IWireAO : public IDispatch
    {
    public:
    };
    
    
#else 	/* C style interface */

    typedef struct IWireAOVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IWireAO * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IWireAO * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IWireAO * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IWireAO * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IWireAO * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IWireAO * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IWireAO * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        END_INTERFACE
    } IWireAOVtbl;

    interface IWireAO
    {
        CONST_VTBL struct IWireAOVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWireAO_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IWireAO_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IWireAO_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IWireAO_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IWireAO_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IWireAO_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IWireAO_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IWireAO_INTERFACE_DEFINED__ */


#ifndef __IProjectWindowAO_INTERFACE_DEFINED__
#define __IProjectWindowAO_INTERFACE_DEFINED__

/* interface IProjectWindowAO */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IProjectWindowAO;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("AD903F61-09AE-4901-A176-B8D3E3AF372A")
    IProjectWindowAO : public IDispatch
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE GetProject( 
            /* [retval][out] */ IProjectAO **projectAO) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SelectBridge( 
            /* [in] */ IBridgeAO *bridgeAO) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SelectWire( 
            /* [in] */ IWireAO *wire) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE ClearSelection( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE DeleteSelection( void) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE SelectVlan( 
            DWORD vlanNumber) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IProjectWindowAOVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IProjectWindowAO * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IProjectWindowAO * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IProjectWindowAO * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IProjectWindowAO * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IProjectWindowAO * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IProjectWindowAO * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IProjectWindowAO * This,
            /* [annotation][in] */ 
            _In_  DISPID dispIdMember,
            /* [annotation][in] */ 
            _In_  REFIID riid,
            /* [annotation][in] */ 
            _In_  LCID lcid,
            /* [annotation][in] */ 
            _In_  WORD wFlags,
            /* [annotation][out][in] */ 
            _In_  DISPPARAMS *pDispParams,
            /* [annotation][out] */ 
            _Out_opt_  VARIANT *pVarResult,
            /* [annotation][out] */ 
            _Out_opt_  EXCEPINFO *pExcepInfo,
            /* [annotation][out] */ 
            _Out_opt_  UINT *puArgErr);
        
        HRESULT ( STDMETHODCALLTYPE *GetProject )( 
            IProjectWindowAO * This,
            /* [retval][out] */ IProjectAO **projectAO);
        
        HRESULT ( STDMETHODCALLTYPE *SelectBridge )( 
            IProjectWindowAO * This,
            /* [in] */ IBridgeAO *bridgeAO);
        
        HRESULT ( STDMETHODCALLTYPE *SelectWire )( 
            IProjectWindowAO * This,
            /* [in] */ IWireAO *wire);
        
        HRESULT ( STDMETHODCALLTYPE *ClearSelection )( 
            IProjectWindowAO * This);
        
        HRESULT ( STDMETHODCALLTYPE *DeleteSelection )( 
            IProjectWindowAO * This);
        
        HRESULT ( STDMETHODCALLTYPE *SelectVlan )( 
            IProjectWindowAO * This,
            DWORD vlanNumber);
        
        END_INTERFACE
    } IProjectWindowAOVtbl;

    interface IProjectWindowAO
    {
        CONST_VTBL struct IProjectWindowAOVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IProjectWindowAO_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IProjectWindowAO_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IProjectWindowAO_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IProjectWindowAO_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IProjectWindowAO_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IProjectWindowAO_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IProjectWindowAO_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IProjectWindowAO_GetProject(This,projectAO)	\
    ( (This)->lpVtbl -> GetProject(This,projectAO) ) 

#define IProjectWindowAO_SelectBridge(This,bridgeAO)	\
    ( (This)->lpVtbl -> SelectBridge(This,bridgeAO) ) 

#define IProjectWindowAO_SelectWire(This,wire)	\
    ( (This)->lpVtbl -> SelectWire(This,wire) ) 

#define IProjectWindowAO_ClearSelection(This)	\
    ( (This)->lpVtbl -> ClearSelection(This) ) 

#define IProjectWindowAO_DeleteSelection(This)	\
    ( (This)->lpVtbl -> DeleteSelection(This) ) 

#define IProjectWindowAO_SelectVlan(This,vlanNumber)	\
    ( (This)->lpVtbl -> SelectVlan(This,vlanNumber) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IProjectWindowAO_INTERFACE_DEFINED__ */

#endif /* __SimulatorAutomationLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


