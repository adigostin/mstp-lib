

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0622 */
/* at Tue Jan 19 05:14:07 2038
 */
/* Compiler settings for EdgeIDL.idl:
    Oicf, W1, Zp8, env=Win64 (32b run), target_arch=AMD64 8.01.0622 
    protocol : all , ms_ext, c_ext, robust
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


#ifndef __EdgeIDL_h__
#define __EdgeIDL_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IObjectCollectionChangeEvents_FWD_DEFINED__
#define __IObjectCollectionChangeEvents_FWD_DEFINED__
typedef interface IObjectCollectionChangeEvents IObjectCollectionChangeEvents;

#endif 	/* __IObjectCollectionChangeEvents_FWD_DEFINED__ */


#ifndef __IPropertyChangeSink_FWD_DEFINED__
#define __IPropertyChangeSink_FWD_DEFINED__
typedef interface IPropertyChangeSink IPropertyChangeSink;

#endif 	/* __IPropertyChangeSink_FWD_DEFINED__ */


#ifdef __cplusplus
extern "C"{
#endif 



#ifndef __EdgeLib_LIBRARY_DEFINED__
#define __EdgeLib_LIBRARY_DEFINED__

/* library EdgeLib */
/* [uuid] */ 


enum PropertyType
    {
        Value	= 0,
        Collection	= ( Value + 1 ) 
    } ;

enum CollectionChangeType
    {
        Set	= 0,
        Insert	= ( Set + 1 ) ,
        Remove	= ( Insert + 1 ) ,
        Rotate	= ( Remove + 1 ) 
    } ;
struct ObjectCollectionChangeArgs
    {
    enum CollectionChangeType changeType;
    /* [switch_is] */ /* [switch_type] */ union 
        {
        /* [case()] */ struct 
            {
            ULONG index;
            ULONG count;
            /* [size_is] */ IDispatch *const *childObjs;
            } 	setInsertRemoveArgs;
        /* [case()] */ struct 
            {
            ULONG first;
            ULONG n_first;
            ULONG last;
            } 	rotateArgs;
        } 	;
    } ;
struct PropertyChangeArgs
    {
    enum PropertyType propertyType;
    /* [switch_is] */ /* [switch_type] */ union 
        {
        /* [case()] */ struct 
            {
            UINT32 a;
            UINT32 b;
            } 	valueChangeArgs;
        /* [case()] */ struct ObjectCollectionChangeArgs collectionChangeArgs;
        } 	;
    } ;

EXTERN_C const IID LIBID_EdgeLib;

#ifndef __IObjectCollectionChangeEvents_INTERFACE_DEFINED__
#define __IObjectCollectionChangeEvents_INTERFACE_DEFINED__

/* interface IObjectCollectionChangeEvents */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IObjectCollectionChangeEvents;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("1564986C-AB17-4F45-90A7-DA592B01BED8")
    IObjectCollectionChangeEvents : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE OnCollectionChanging( 
            IUnknown *sender,
            const struct ObjectCollectionChangeArgs *args) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnCollectionChanged( 
            IUnknown *sender,
            const struct ObjectCollectionChangeArgs *args) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IObjectCollectionChangeEventsVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IObjectCollectionChangeEvents * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IObjectCollectionChangeEvents * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IObjectCollectionChangeEvents * This);
        
        HRESULT ( STDMETHODCALLTYPE *OnCollectionChanging )( 
            IObjectCollectionChangeEvents * This,
            IUnknown *sender,
            const struct ObjectCollectionChangeArgs *args);
        
        HRESULT ( STDMETHODCALLTYPE *OnCollectionChanged )( 
            IObjectCollectionChangeEvents * This,
            IUnknown *sender,
            const struct ObjectCollectionChangeArgs *args);
        
        END_INTERFACE
    } IObjectCollectionChangeEventsVtbl;

    interface IObjectCollectionChangeEvents
    {
        CONST_VTBL struct IObjectCollectionChangeEventsVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IObjectCollectionChangeEvents_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IObjectCollectionChangeEvents_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IObjectCollectionChangeEvents_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IObjectCollectionChangeEvents_OnCollectionChanging(This,sender,args)	\
    ( (This)->lpVtbl -> OnCollectionChanging(This,sender,args) ) 

#define IObjectCollectionChangeEvents_OnCollectionChanged(This,sender,args)	\
    ( (This)->lpVtbl -> OnCollectionChanged(This,sender,args) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IObjectCollectionChangeEvents_INTERFACE_DEFINED__ */


#ifndef __IPropertyChangeSink_INTERFACE_DEFINED__
#define __IPropertyChangeSink_INTERFACE_DEFINED__

/* interface IPropertyChangeSink */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IPropertyChangeSink;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("341F4C53-65F3-443D-9D2C-172951F4A525")
    IPropertyChangeSink : public IUnknown
    {
    public:
        virtual HRESULT STDMETHODCALLTYPE OnPropertyChanging( 
            IUnknown *obj,
            DISPID dispID,
            const struct PropertyChangeArgs *args) = 0;
        
        virtual HRESULT STDMETHODCALLTYPE OnPropertyChanged( 
            IUnknown *obj,
            DISPID dispID,
            const struct PropertyChangeArgs *args) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPropertyChangeSinkVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPropertyChangeSink * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPropertyChangeSink * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPropertyChangeSink * This);
        
        HRESULT ( STDMETHODCALLTYPE *OnPropertyChanging )( 
            IPropertyChangeSink * This,
            IUnknown *obj,
            DISPID dispID,
            const struct PropertyChangeArgs *args);
        
        HRESULT ( STDMETHODCALLTYPE *OnPropertyChanged )( 
            IPropertyChangeSink * This,
            IUnknown *obj,
            DISPID dispID,
            const struct PropertyChangeArgs *args);
        
        END_INTERFACE
    } IPropertyChangeSinkVtbl;

    interface IPropertyChangeSink
    {
        CONST_VTBL struct IPropertyChangeSinkVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPropertyChangeSink_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPropertyChangeSink_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPropertyChangeSink_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPropertyChangeSink_OnPropertyChanging(This,obj,dispID,args)	\
    ( (This)->lpVtbl -> OnPropertyChanging(This,obj,dispID,args) ) 

#define IPropertyChangeSink_OnPropertyChanged(This,obj,dispID,args)	\
    ( (This)->lpVtbl -> OnPropertyChanged(This,obj,dispID,args) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPropertyChangeSink_INTERFACE_DEFINED__ */

#endif /* __EdgeLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


