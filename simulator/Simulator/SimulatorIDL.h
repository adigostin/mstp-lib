

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 8.01.0622 */
/* at Tue Jan 19 05:14:07 2038
 */
/* Compiler settings for SimulatorIDL.idl:
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


#ifndef __SimulatorIDL_h__
#define __SimulatorIDL_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IBridgeProperties_FWD_DEFINED__
#define __IBridgeProperties_FWD_DEFINED__
typedef interface IBridgeProperties IBridgeProperties;

#endif 	/* __IBridgeProperties_FWD_DEFINED__ */


#ifndef __IProjectProperties_FWD_DEFINED__
#define __IProjectProperties_FWD_DEFINED__
typedef interface IProjectProperties IProjectProperties;

#endif 	/* __IProjectProperties_FWD_DEFINED__ */


#ifndef __IBridgeTreeProperties_FWD_DEFINED__
#define __IBridgeTreeProperties_FWD_DEFINED__
typedef interface IBridgeTreeProperties IBridgeTreeProperties;

#endif 	/* __IBridgeTreeProperties_FWD_DEFINED__ */


#ifndef __IMSTConfigProperties_FWD_DEFINED__
#define __IMSTConfigProperties_FWD_DEFINED__
typedef interface IMSTConfigProperties IMSTConfigProperties;

#endif 	/* __IMSTConfigProperties_FWD_DEFINED__ */


#ifndef __IPortTreeProperties_FWD_DEFINED__
#define __IPortTreeProperties_FWD_DEFINED__
typedef interface IPortTreeProperties IPortTreeProperties;

#endif 	/* __IPortTreeProperties_FWD_DEFINED__ */


#ifndef __IPortProperties_FWD_DEFINED__
#define __IPortProperties_FWD_DEFINED__
typedef interface IPortProperties IPortProperties;

#endif 	/* __IPortProperties_FWD_DEFINED__ */


#ifndef __IWireProperties_FWD_DEFINED__
#define __IWireProperties_FWD_DEFINED__
typedef interface IWireProperties IWireProperties;

#endif 	/* __IWireProperties_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"
#include "EdgeIDL.h"

#ifdef __cplusplus
extern "C"{
#endif 


/* interface __MIDL_itf_SimulatorIDL_0000_0000 */
/* [local] */ 

#pragma once
DEFINE_GUID(guidMSTConfigIdEditor, 0x533D2CFB, 0x2CE4, 0x46EF, 0x97, 0xEC, 0xD8, 0x12, 0x69, 0x72, 0xEE, 0xEA);
#define	dispidBridges	( 1 )

#define	dispidWires	( 2 )

#define	dispidPortTrees	( 3 )

#define	dispidStpEnabled	( 4 )

#define	dispidBridgeWidth	( 5 )

#define	dispidBridgeHeight	( 6 )

#define	dispidPortAdminP2P	( 7 )

#define	dispidPortOperP2P	( 8 )

#define	dispidBridgePrio	( 9 )

#define	dispidAdminInternalPortPathCost	( 10 )

#define	dispidInternalPortPathCost	( 11 )

#define	dispidMigrateTime	( 12 )

#define	dispidBridgeHelloTime	( 13 )

#define	dispidBridgeMaxAge	( 14 )

#define	dispidBridgeForwardDelay	( 15 )

#define	dispidTxHoldCount	( 16 )

#define	dispidMaxHops	( 17 )

#define	dispidBridgeAddress	( 18 )

#define	dispidStpVersion	( 19 )

#define	dispidPortCount	( 20 )

#define	dispidMstiCount	( 21 )

#define	dispidMstConfigName	( 22 )

#define	dispidMstConfigRevLevel	( 23 )

#define	dispidMstConfigTable	( 24 )

#define	dispidPortLearning	( 25 )

#define	dispidPortForwarding	( 26 )

#define	dispidPortRole	( 27 )

#define	dispidPortPriority	( 28 )

#define	dispidSupportedSpeed	( 29 )

#define	dispidWireEndP0	( 30 )

#define	dispidWireEndP1	( 31 )

#define	dispidWireEndBridgeIndex	( 32 )

#define	dispidWireEndPortIndex	( 33 )

#define	dispidWireEndX	( 34 )

#define	dispidWireEndY	( 35 )

#define	dispidBridgeX	( 36 )

#define	dispidBridgeY	( 37 )

#define	dispidTopologyChangeCount	( 38 )

#define	dispidAdminEdge	( 39 )

#define	dispidOperEdge	( 40 )

#define	dispidPorts	( 41 )

#define	dispidBridgeTrees	( 42 )

#define	dispidPortSide	( 43 )

#define	dispidPortOffset	( 44 )

#define	dispidPortActualSpeed	( 45 )

#define	dispidPortMacOperational	( 46 )

#define	dispidPortDetectedP2P	( 47 )

#define	dispidRootId	( 48 )

#define	dispidExternalRootPathCost	( 49 )

#define	dispidRegionalRootId	( 50 )

#define	dispidInternalRootPathCost	( 51 )

#define	dispidDesignatedBridgeId	( 52 )

#define	dispidDesignatedPortId	( 53 )

#define	dispidReceivingPortId	( 54 )

#define	dispidHelloTime	( 55 )

#define	dispidMaxAge	( 56 )

#define	dispidForwardDelay	( 57 )

#define	dispidMessageAge	( 58 )

#define	dispidRemainingHops	( 59 )

#define	dispidMstConfig	( 60 )



extern RPC_IF_HANDLE __MIDL_itf_SimulatorIDL_0000_0000_v0_0_c_ifspec;
extern RPC_IF_HANDLE __MIDL_itf_SimulatorIDL_0000_0000_v0_0_s_ifspec;


#ifndef __SimulatorLib_LIBRARY_DEFINED__
#define __SimulatorLib_LIBRARY_DEFINED__

/* library SimulatorLib */
/* [version][uuid] */ 



enum BridgePriority
    {
        BridgePriority1000	= 0x1000,
        BridgePriority2000	= 0x2000,
        BridgePriority3000	= 0x3000,
        BridgePriority4000	= 0x4000,
        BridgePriority5000	= 0x5000,
        BridgePriority6000	= 0x6000,
        BridgePriority7000	= 0x7000,
        BridgePriority8000	= 0x8000,
        BridgePriority9000	= 0x9000,
        BridgePriorityA000	= 0xa000,
        BridgePriorityB000	= 0xb000,
        BridgePriorityC000	= 0xc000,
        BridgePriorityD000	= 0xd000,
        BridgePriorityE000	= 0xe000,
        BridgePriorityF000	= 0xf000
    } ;

enum PortPriority
    {
        PortPriority10	= 0x10,
        PortPriority20	= 0x20,
        PortPriority30	= 0x30,
        PortPriority40	= 0x40,
        PortPriority50	= 0x50,
        PortPriority60	= 0x60,
        PortPriority70	= 0x70,
        PortPriority80	= 0x80,
        PortPriority90	= 0x90,
        PortPriorityA0	= 0xa0,
        PortPriorityB0	= 0xb0,
        PortPriorityC0	= 0xc0,
        PortPriorityD0	= 0xd0,
        PortPriorityE0	= 0xe0,
        PortPriorityF0	= 0xf0
    } ;

enum PortSpeed
    {
        PortSpeed10M	= 10,
        PortSpeed100M	= 100,
        PortSpeed1G	= 1000,
        PortSpeed10G	= 10000
    } ;
/* [uuid] */ 
enum  DECLSPEC_UUID("6528CAE0-6B18-4EB1-97AA-66B865275523") STPVersion
    {
        STPVersionLegacySTP	= 0,
        STPVersionRSTP	= 2,
        STPVersionMSTP	= 3
    } ;
/* [uuid] */ 
enum  DECLSPEC_UUID("13FD1054-1CD4-413D-92B5-9E57F4D5E053") PortRole
    {
        Undefined	= 0,
        Disabled	= ( Undefined + 1 ) ,
        Root	= ( Disabled + 1 ) ,
        Designated	= ( Root + 1 ) ,
        Alternate	= ( Designated + 1 ) ,
        Backup	= ( Alternate + 1 ) ,
        Master	= ( Backup + 1 ) 
    } ;
/* [uuid] */ 
enum  DECLSPEC_UUID("BA952BF8-50F7-46AF-8066-34180560587B") PortSide
    {
        Left	= 0,
        Top	= ( Left + 1 ) ,
        Right	= ( Top + 1 ) ,
        Bottom	= ( Right + 1 ) 
    } ;
/* [uuid] */ 
enum  DECLSPEC_UUID("0A279EF7-DCB4-435F-AA4C-1005B658BAA1") AdminPointToPoint
    {
        ForceTrue	= 1,
        ForceFalse	= 2,
        Auto	= 3
    } ;

EXTERN_C const IID LIBID_SimulatorLib;

#ifndef __IBridgeProperties_INTERFACE_DEFINED__
#define __IBridgeProperties_INTERFACE_DEFINED__

/* interface IBridgeProperties */
/* [unique][nonextensible][dual][uuid][object] */ 

#define	STPEnabledDefaultValue	( FALSE )

#define	STPVersionDefaultValue	( STPVersionRSTP )

#define	MigrateTimeDefaultValue	( 3 )

#define	BridgeHelloTimeDefaultValue	( 2 )

#define	BridgeMaxAgeDefaultValue	( 20 )

#define	BridgeForwardDelayDefaultValue	( 15 )

#define	TxHoldCountDefaultValue	( 6 )

#define	MaxHopsDefaultValue	( 20 )

#define	MSTConfigRevLevelDefaultValue	( 0 )


EXTERN_C const IID IID_IBridgeProperties;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("B7934040-1A24-4AB8-B3B0-D35972DACB1A")
    IBridgeProperties : public IDispatch
    {
    public:
        virtual /* [nonbrowsable][propget] */ HRESULT STDMETHODCALLTYPE get_Name( 
            /* [retval][out] */ BSTR *pName) = 0;
        
        virtual /* [nonbrowsable][propget] */ HRESULT STDMETHODCALLTYPE get_ClassName( 
            /* [retval][out] */ BSTR *pClassName) = 0;
        
        virtual /* [nonbrowsable][id][propget] */ HRESULT STDMETHODCALLTYPE get_X( 
            /* [retval][out] */ LONG *pX) = 0;
        
        virtual /* [nonbrowsable][id][propput] */ HRESULT STDMETHODCALLTYPE put_X( 
            LONG x) = 0;
        
        virtual /* [nonbrowsable][id][propget] */ HRESULT STDMETHODCALLTYPE get_Y( 
            /* [retval][out] */ LONG *pY) = 0;
        
        virtual /* [nonbrowsable][id][propput] */ HRESULT STDMETHODCALLTYPE put_Y( 
            LONG y) = 0;
        
        virtual /* [nonbrowsable][id][propget] */ HRESULT STDMETHODCALLTYPE get_Width( 
            /* [retval][out] */ LONG *pWidth) = 0;
        
        virtual /* [nonbrowsable][id][propput] */ HRESULT STDMETHODCALLTYPE put_Width( 
            LONG width) = 0;
        
        virtual /* [nonbrowsable][id][propget] */ HRESULT STDMETHODCALLTYPE get_Height( 
            /* [retval][out] */ LONG *pHeight) = 0;
        
        virtual /* [nonbrowsable][id][propput] */ HRESULT STDMETHODCALLTYPE put_Height( 
            LONG height) = 0;
        
        virtual /* [nonbrowsable][id][propget] */ HRESULT STDMETHODCALLTYPE get_Ports( 
            /* [retval][ref][out] */ SAFEARRAY * *ppsaItems) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_BridgeAddress( 
            /* [retval][out] */ BSTR *pbstrBridgeAddress) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_BridgeAddress( 
            BSTR bstrBridgeAddress) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_STPEnabled( 
            /* [retval][out] */ VARIANT_BOOL *pbEnabled) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_STPEnabled( 
            VARIANT_BOOL bEnabled) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_STPVersion( 
            /* [retval][out] */ enum STPVersion *pVersion) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_STPVersion( 
            enum STPVersion version) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_PortCount( 
            /* [retval][out] */ DWORD *pdwPortCount) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MSTICount( 
            /* [retval][out] */ DWORD *pdwMstiCount) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MigrateTime( 
            /* [retval][out] */ DWORD *pdwMigrateTime) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_BridgeHelloTime( 
            /* [retval][out] */ DWORD *pdwBridgeHelloTime) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_BridgeMaxAge( 
            /* [retval][out] */ DWORD *pdwBridgeMaxAge) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_BridgeMaxAge( 
            DWORD dwBridgeMaxAge) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_BridgeForwardDelay( 
            /* [retval][out] */ DWORD *pdwBridgeForwardDelay) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_BridgeForwardDelay( 
            DWORD dwBridgeForwardDelay) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_TxHoldCount( 
            /* [retval][out] */ DWORD *pdwTxHoldCount) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_TxHoldCount( 
            DWORD dwTxHoldCount) = 0;
        
        virtual /* [helpstring][custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MaxHops( 
            /* [retval][out] */ DWORD *pdwMaxHops) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MSTConfigName( 
            /* [retval][out] */ BSTR *pbstrName) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_MSTConfigName( 
            BSTR bstrName) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MSTConfigRevLevel( 
            /* [retval][out] */ WORD *pwConfigRevLevel) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_MSTConfigRevLevel( 
            WORD wRevLevel) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MSTConfigDigest( 
            /* [retval][out] */ BSTR *pbstrDigest) = 0;
        
        virtual /* [nonbrowsable][id][propget] */ HRESULT STDMETHODCALLTYPE get_MSTConfig( 
            /* [retval][out] */ IMSTConfigProperties **ppMSTConfig) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IBridgePropertiesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IBridgeProperties * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IBridgeProperties * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IBridgeProperties * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IBridgeProperties * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IBridgeProperties * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IBridgeProperties * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IBridgeProperties * This,
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
        
        /* [nonbrowsable][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Name )( 
            IBridgeProperties * This,
            /* [retval][out] */ BSTR *pName);
        
        /* [nonbrowsable][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ClassName )( 
            IBridgeProperties * This,
            /* [retval][out] */ BSTR *pClassName);
        
        /* [nonbrowsable][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_X )( 
            IBridgeProperties * This,
            /* [retval][out] */ LONG *pX);
        
        /* [nonbrowsable][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_X )( 
            IBridgeProperties * This,
            LONG x);
        
        /* [nonbrowsable][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Y )( 
            IBridgeProperties * This,
            /* [retval][out] */ LONG *pY);
        
        /* [nonbrowsable][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_Y )( 
            IBridgeProperties * This,
            LONG y);
        
        /* [nonbrowsable][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Width )( 
            IBridgeProperties * This,
            /* [retval][out] */ LONG *pWidth);
        
        /* [nonbrowsable][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_Width )( 
            IBridgeProperties * This,
            LONG width);
        
        /* [nonbrowsable][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Height )( 
            IBridgeProperties * This,
            /* [retval][out] */ LONG *pHeight);
        
        /* [nonbrowsable][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_Height )( 
            IBridgeProperties * This,
            LONG height);
        
        /* [nonbrowsable][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Ports )( 
            IBridgeProperties * This,
            /* [retval][ref][out] */ SAFEARRAY * *ppsaItems);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_BridgeAddress )( 
            IBridgeProperties * This,
            /* [retval][out] */ BSTR *pbstrBridgeAddress);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_BridgeAddress )( 
            IBridgeProperties * This,
            BSTR bstrBridgeAddress);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_STPEnabled )( 
            IBridgeProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pbEnabled);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_STPEnabled )( 
            IBridgeProperties * This,
            VARIANT_BOOL bEnabled);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_STPVersion )( 
            IBridgeProperties * This,
            /* [retval][out] */ enum STPVersion *pVersion);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_STPVersion )( 
            IBridgeProperties * This,
            enum STPVersion version);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_PortCount )( 
            IBridgeProperties * This,
            /* [retval][out] */ DWORD *pdwPortCount);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MSTICount )( 
            IBridgeProperties * This,
            /* [retval][out] */ DWORD *pdwMstiCount);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MigrateTime )( 
            IBridgeProperties * This,
            /* [retval][out] */ DWORD *pdwMigrateTime);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_BridgeHelloTime )( 
            IBridgeProperties * This,
            /* [retval][out] */ DWORD *pdwBridgeHelloTime);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_BridgeMaxAge )( 
            IBridgeProperties * This,
            /* [retval][out] */ DWORD *pdwBridgeMaxAge);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_BridgeMaxAge )( 
            IBridgeProperties * This,
            DWORD dwBridgeMaxAge);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_BridgeForwardDelay )( 
            IBridgeProperties * This,
            /* [retval][out] */ DWORD *pdwBridgeForwardDelay);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_BridgeForwardDelay )( 
            IBridgeProperties * This,
            DWORD dwBridgeForwardDelay);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_TxHoldCount )( 
            IBridgeProperties * This,
            /* [retval][out] */ DWORD *pdwTxHoldCount);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_TxHoldCount )( 
            IBridgeProperties * This,
            DWORD dwTxHoldCount);
        
        /* [helpstring][custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MaxHops )( 
            IBridgeProperties * This,
            /* [retval][out] */ DWORD *pdwMaxHops);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MSTConfigName )( 
            IBridgeProperties * This,
            /* [retval][out] */ BSTR *pbstrName);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_MSTConfigName )( 
            IBridgeProperties * This,
            BSTR bstrName);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MSTConfigRevLevel )( 
            IBridgeProperties * This,
            /* [retval][out] */ WORD *pwConfigRevLevel);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_MSTConfigRevLevel )( 
            IBridgeProperties * This,
            WORD wRevLevel);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MSTConfigDigest )( 
            IBridgeProperties * This,
            /* [retval][out] */ BSTR *pbstrDigest);
        
        /* [nonbrowsable][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MSTConfig )( 
            IBridgeProperties * This,
            /* [retval][out] */ IMSTConfigProperties **ppMSTConfig);
        
        END_INTERFACE
    } IBridgePropertiesVtbl;

    interface IBridgeProperties
    {
        CONST_VTBL struct IBridgePropertiesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBridgeProperties_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IBridgeProperties_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IBridgeProperties_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IBridgeProperties_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IBridgeProperties_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IBridgeProperties_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IBridgeProperties_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IBridgeProperties_get_Name(This,pName)	\
    ( (This)->lpVtbl -> get_Name(This,pName) ) 

#define IBridgeProperties_get_ClassName(This,pClassName)	\
    ( (This)->lpVtbl -> get_ClassName(This,pClassName) ) 

#define IBridgeProperties_get_X(This,pX)	\
    ( (This)->lpVtbl -> get_X(This,pX) ) 

#define IBridgeProperties_put_X(This,x)	\
    ( (This)->lpVtbl -> put_X(This,x) ) 

#define IBridgeProperties_get_Y(This,pY)	\
    ( (This)->lpVtbl -> get_Y(This,pY) ) 

#define IBridgeProperties_put_Y(This,y)	\
    ( (This)->lpVtbl -> put_Y(This,y) ) 

#define IBridgeProperties_get_Width(This,pWidth)	\
    ( (This)->lpVtbl -> get_Width(This,pWidth) ) 

#define IBridgeProperties_put_Width(This,width)	\
    ( (This)->lpVtbl -> put_Width(This,width) ) 

#define IBridgeProperties_get_Height(This,pHeight)	\
    ( (This)->lpVtbl -> get_Height(This,pHeight) ) 

#define IBridgeProperties_put_Height(This,height)	\
    ( (This)->lpVtbl -> put_Height(This,height) ) 

#define IBridgeProperties_get_Ports(This,ppsaItems)	\
    ( (This)->lpVtbl -> get_Ports(This,ppsaItems) ) 

#define IBridgeProperties_get_BridgeAddress(This,pbstrBridgeAddress)	\
    ( (This)->lpVtbl -> get_BridgeAddress(This,pbstrBridgeAddress) ) 

#define IBridgeProperties_put_BridgeAddress(This,bstrBridgeAddress)	\
    ( (This)->lpVtbl -> put_BridgeAddress(This,bstrBridgeAddress) ) 

#define IBridgeProperties_get_STPEnabled(This,pbEnabled)	\
    ( (This)->lpVtbl -> get_STPEnabled(This,pbEnabled) ) 

#define IBridgeProperties_put_STPEnabled(This,bEnabled)	\
    ( (This)->lpVtbl -> put_STPEnabled(This,bEnabled) ) 

#define IBridgeProperties_get_STPVersion(This,pVersion)	\
    ( (This)->lpVtbl -> get_STPVersion(This,pVersion) ) 

#define IBridgeProperties_put_STPVersion(This,version)	\
    ( (This)->lpVtbl -> put_STPVersion(This,version) ) 

#define IBridgeProperties_get_PortCount(This,pdwPortCount)	\
    ( (This)->lpVtbl -> get_PortCount(This,pdwPortCount) ) 

#define IBridgeProperties_get_MSTICount(This,pdwMstiCount)	\
    ( (This)->lpVtbl -> get_MSTICount(This,pdwMstiCount) ) 

#define IBridgeProperties_get_MigrateTime(This,pdwMigrateTime)	\
    ( (This)->lpVtbl -> get_MigrateTime(This,pdwMigrateTime) ) 

#define IBridgeProperties_get_BridgeHelloTime(This,pdwBridgeHelloTime)	\
    ( (This)->lpVtbl -> get_BridgeHelloTime(This,pdwBridgeHelloTime) ) 

#define IBridgeProperties_get_BridgeMaxAge(This,pdwBridgeMaxAge)	\
    ( (This)->lpVtbl -> get_BridgeMaxAge(This,pdwBridgeMaxAge) ) 

#define IBridgeProperties_put_BridgeMaxAge(This,dwBridgeMaxAge)	\
    ( (This)->lpVtbl -> put_BridgeMaxAge(This,dwBridgeMaxAge) ) 

#define IBridgeProperties_get_BridgeForwardDelay(This,pdwBridgeForwardDelay)	\
    ( (This)->lpVtbl -> get_BridgeForwardDelay(This,pdwBridgeForwardDelay) ) 

#define IBridgeProperties_put_BridgeForwardDelay(This,dwBridgeForwardDelay)	\
    ( (This)->lpVtbl -> put_BridgeForwardDelay(This,dwBridgeForwardDelay) ) 

#define IBridgeProperties_get_TxHoldCount(This,pdwTxHoldCount)	\
    ( (This)->lpVtbl -> get_TxHoldCount(This,pdwTxHoldCount) ) 

#define IBridgeProperties_put_TxHoldCount(This,dwTxHoldCount)	\
    ( (This)->lpVtbl -> put_TxHoldCount(This,dwTxHoldCount) ) 

#define IBridgeProperties_get_MaxHops(This,pdwMaxHops)	\
    ( (This)->lpVtbl -> get_MaxHops(This,pdwMaxHops) ) 

#define IBridgeProperties_get_MSTConfigName(This,pbstrName)	\
    ( (This)->lpVtbl -> get_MSTConfigName(This,pbstrName) ) 

#define IBridgeProperties_put_MSTConfigName(This,bstrName)	\
    ( (This)->lpVtbl -> put_MSTConfigName(This,bstrName) ) 

#define IBridgeProperties_get_MSTConfigRevLevel(This,pwConfigRevLevel)	\
    ( (This)->lpVtbl -> get_MSTConfigRevLevel(This,pwConfigRevLevel) ) 

#define IBridgeProperties_put_MSTConfigRevLevel(This,wRevLevel)	\
    ( (This)->lpVtbl -> put_MSTConfigRevLevel(This,wRevLevel) ) 

#define IBridgeProperties_get_MSTConfigDigest(This,pbstrDigest)	\
    ( (This)->lpVtbl -> get_MSTConfigDigest(This,pbstrDigest) ) 

#define IBridgeProperties_get_MSTConfig(This,ppMSTConfig)	\
    ( (This)->lpVtbl -> get_MSTConfig(This,ppMSTConfig) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IBridgeProperties_INTERFACE_DEFINED__ */


#ifndef __IProjectProperties_INTERFACE_DEFINED__
#define __IProjectProperties_INTERFACE_DEFINED__

/* interface IProjectProperties */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IProjectProperties;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("5E25724D-23F8-4896-93C4-E6E584D23FF2")
    IProjectProperties : public IDispatch
    {
    public:
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Bridges( 
            /* [retval][ref][out] */ SAFEARRAY * *ppsaItems) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Bridges( 
            SAFEARRAY * psaItems) = 0;
        
        virtual /* [propget][id] */ HRESULT STDMETHODCALLTYPE get_Wires( 
            /* [retval][ref][out] */ SAFEARRAY * *ppsaItems) = 0;
        
        virtual /* [propput][id] */ HRESULT STDMETHODCALLTYPE put_Wires( 
            SAFEARRAY * psaItems) = 0;
        
        virtual /* [propget] */ HRESULT STDMETHODCALLTYPE get_NextBridgeAddress( 
            /* [retval][out] */ BSTR *pbstrNextBridgeAddress) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_NextBridgeAddress( 
            BSTR bstrNextBridgeAddress) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IProjectPropertiesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IProjectProperties * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IProjectProperties * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IProjectProperties * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IProjectProperties * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IProjectProperties * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IProjectProperties * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IProjectProperties * This,
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
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Bridges )( 
            IProjectProperties * This,
            /* [retval][ref][out] */ SAFEARRAY * *ppsaItems);
        
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Bridges )( 
            IProjectProperties * This,
            SAFEARRAY * psaItems);
        
        /* [propget][id] */ HRESULT ( STDMETHODCALLTYPE *get_Wires )( 
            IProjectProperties * This,
            /* [retval][ref][out] */ SAFEARRAY * *ppsaItems);
        
        /* [propput][id] */ HRESULT ( STDMETHODCALLTYPE *put_Wires )( 
            IProjectProperties * This,
            SAFEARRAY * psaItems);
        
        /* [propget] */ HRESULT ( STDMETHODCALLTYPE *get_NextBridgeAddress )( 
            IProjectProperties * This,
            /* [retval][out] */ BSTR *pbstrNextBridgeAddress);
        
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_NextBridgeAddress )( 
            IProjectProperties * This,
            BSTR bstrNextBridgeAddress);
        
        END_INTERFACE
    } IProjectPropertiesVtbl;

    interface IProjectProperties
    {
        CONST_VTBL struct IProjectPropertiesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IProjectProperties_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IProjectProperties_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IProjectProperties_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IProjectProperties_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IProjectProperties_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IProjectProperties_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IProjectProperties_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IProjectProperties_get_Bridges(This,ppsaItems)	\
    ( (This)->lpVtbl -> get_Bridges(This,ppsaItems) ) 

#define IProjectProperties_put_Bridges(This,psaItems)	\
    ( (This)->lpVtbl -> put_Bridges(This,psaItems) ) 

#define IProjectProperties_get_Wires(This,ppsaItems)	\
    ( (This)->lpVtbl -> get_Wires(This,ppsaItems) ) 

#define IProjectProperties_put_Wires(This,psaItems)	\
    ( (This)->lpVtbl -> put_Wires(This,psaItems) ) 

#define IProjectProperties_get_NextBridgeAddress(This,pbstrNextBridgeAddress)	\
    ( (This)->lpVtbl -> get_NextBridgeAddress(This,pbstrNextBridgeAddress) ) 

#define IProjectProperties_put_NextBridgeAddress(This,bstrNextBridgeAddress)	\
    ( (This)->lpVtbl -> put_NextBridgeAddress(This,bstrNextBridgeAddress) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IProjectProperties_INTERFACE_DEFINED__ */


#ifndef __IBridgeTreeProperties_INTERFACE_DEFINED__
#define __IBridgeTreeProperties_INTERFACE_DEFINED__

/* interface IBridgeTreeProperties */
/* [unique][nonextensible][dual][uuid][object] */ 

#define	BridgePriorityDefaultValue	( BridgePriority8000 )


EXTERN_C const IID IID_IBridgeTreeProperties;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("BC1115A7-567F-4F72-ACA3-940723B6AE30")
    IBridgeTreeProperties : public IDispatch
    {
    public:
        virtual /* [nonbrowsable][propget] */ HRESULT STDMETHODCALLTYPE get_Name( 
            /* [retval][out] */ BSTR *pName) = 0;
        
        virtual /* [nonbrowsable][propget] */ HRESULT STDMETHODCALLTYPE get_ClassName( 
            /* [retval][out] */ BSTR *pClassName) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_BridgePriority( 
            /* [retval][out] */ enum BridgePriority *pPrio) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_BridgePriority( 
            enum BridgePriority prio) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_TopologyChangeCount( 
            /* [retval][out] */ DWORD *pCount) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_RootId( 
            /* [retval][out] */ BSTR *pbstrRootId) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_ExternalRootPathCost( 
            /* [retval][out] */ DWORD *pdwExternalRootPathCost) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_RegionalRootId( 
            /* [retval][out] */ BSTR *pbstrRegionalRootId) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_InternalRootPathCost( 
            /* [retval][out] */ DWORD *pdwInternalRootPathCost) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_DesignatedBridgeId( 
            /* [retval][out] */ BSTR *pbstrDesignatedBridgeId) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_DesignatedPortId( 
            /* [retval][out] */ BSTR *pbstrDesignatedPortId) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_ReceivingPortId( 
            /* [retval][out] */ BSTR *pbstrReceivingPortId) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_HelloTime( 
            /* [retval][out] */ DWORD *pdwHelloTime) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MaxAge( 
            /* [retval][out] */ DWORD *pdwMaxAge) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_ForwardDelay( 
            /* [retval][out] */ DWORD *pdwForwardDelay) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MessageAge( 
            /* [retval][out] */ DWORD *pdwMessageAge) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_remainingHops( 
            /* [retval][out] */ DWORD *pdwRemainingHops) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IBridgeTreePropertiesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IBridgeTreeProperties * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IBridgeTreeProperties * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IBridgeTreeProperties * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IBridgeTreeProperties * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IBridgeTreeProperties * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IBridgeTreeProperties * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IBridgeTreeProperties * This,
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
        
        /* [nonbrowsable][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Name )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ BSTR *pName);
        
        /* [nonbrowsable][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ClassName )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ BSTR *pClassName);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_BridgePriority )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ enum BridgePriority *pPrio);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_BridgePriority )( 
            IBridgeTreeProperties * This,
            enum BridgePriority prio);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_TopologyChangeCount )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ DWORD *pCount);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_RootId )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ BSTR *pbstrRootId);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ExternalRootPathCost )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ DWORD *pdwExternalRootPathCost);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_RegionalRootId )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ BSTR *pbstrRegionalRootId);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_InternalRootPathCost )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ DWORD *pdwInternalRootPathCost);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_DesignatedBridgeId )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ BSTR *pbstrDesignatedBridgeId);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_DesignatedPortId )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ BSTR *pbstrDesignatedPortId);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ReceivingPortId )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ BSTR *pbstrReceivingPortId);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_HelloTime )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ DWORD *pdwHelloTime);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MaxAge )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ DWORD *pdwMaxAge);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ForwardDelay )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ DWORD *pdwForwardDelay);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MessageAge )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ DWORD *pdwMessageAge);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_remainingHops )( 
            IBridgeTreeProperties * This,
            /* [retval][out] */ DWORD *pdwRemainingHops);
        
        END_INTERFACE
    } IBridgeTreePropertiesVtbl;

    interface IBridgeTreeProperties
    {
        CONST_VTBL struct IBridgeTreePropertiesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IBridgeTreeProperties_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IBridgeTreeProperties_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IBridgeTreeProperties_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IBridgeTreeProperties_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IBridgeTreeProperties_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IBridgeTreeProperties_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IBridgeTreeProperties_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IBridgeTreeProperties_get_Name(This,pName)	\
    ( (This)->lpVtbl -> get_Name(This,pName) ) 

#define IBridgeTreeProperties_get_ClassName(This,pClassName)	\
    ( (This)->lpVtbl -> get_ClassName(This,pClassName) ) 

#define IBridgeTreeProperties_get_BridgePriority(This,pPrio)	\
    ( (This)->lpVtbl -> get_BridgePriority(This,pPrio) ) 

#define IBridgeTreeProperties_put_BridgePriority(This,prio)	\
    ( (This)->lpVtbl -> put_BridgePriority(This,prio) ) 

#define IBridgeTreeProperties_get_TopologyChangeCount(This,pCount)	\
    ( (This)->lpVtbl -> get_TopologyChangeCount(This,pCount) ) 

#define IBridgeTreeProperties_get_RootId(This,pbstrRootId)	\
    ( (This)->lpVtbl -> get_RootId(This,pbstrRootId) ) 

#define IBridgeTreeProperties_get_ExternalRootPathCost(This,pdwExternalRootPathCost)	\
    ( (This)->lpVtbl -> get_ExternalRootPathCost(This,pdwExternalRootPathCost) ) 

#define IBridgeTreeProperties_get_RegionalRootId(This,pbstrRegionalRootId)	\
    ( (This)->lpVtbl -> get_RegionalRootId(This,pbstrRegionalRootId) ) 

#define IBridgeTreeProperties_get_InternalRootPathCost(This,pdwInternalRootPathCost)	\
    ( (This)->lpVtbl -> get_InternalRootPathCost(This,pdwInternalRootPathCost) ) 

#define IBridgeTreeProperties_get_DesignatedBridgeId(This,pbstrDesignatedBridgeId)	\
    ( (This)->lpVtbl -> get_DesignatedBridgeId(This,pbstrDesignatedBridgeId) ) 

#define IBridgeTreeProperties_get_DesignatedPortId(This,pbstrDesignatedPortId)	\
    ( (This)->lpVtbl -> get_DesignatedPortId(This,pbstrDesignatedPortId) ) 

#define IBridgeTreeProperties_get_ReceivingPortId(This,pbstrReceivingPortId)	\
    ( (This)->lpVtbl -> get_ReceivingPortId(This,pbstrReceivingPortId) ) 

#define IBridgeTreeProperties_get_HelloTime(This,pdwHelloTime)	\
    ( (This)->lpVtbl -> get_HelloTime(This,pdwHelloTime) ) 

#define IBridgeTreeProperties_get_MaxAge(This,pdwMaxAge)	\
    ( (This)->lpVtbl -> get_MaxAge(This,pdwMaxAge) ) 

#define IBridgeTreeProperties_get_ForwardDelay(This,pdwForwardDelay)	\
    ( (This)->lpVtbl -> get_ForwardDelay(This,pdwForwardDelay) ) 

#define IBridgeTreeProperties_get_MessageAge(This,pdwMessageAge)	\
    ( (This)->lpVtbl -> get_MessageAge(This,pdwMessageAge) ) 

#define IBridgeTreeProperties_get_remainingHops(This,pdwRemainingHops)	\
    ( (This)->lpVtbl -> get_remainingHops(This,pdwRemainingHops) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IBridgeTreeProperties_INTERFACE_DEFINED__ */


#ifndef __IMSTConfigProperties_INTERFACE_DEFINED__
#define __IMSTConfigProperties_INTERFACE_DEFINED__

/* interface IMSTConfigProperties */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IMSTConfigProperties;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("4B88CE7F-13A2-45FA-9E64-4F8254C2AD2F")
    IMSTConfigProperties : public IDispatch
    {
    public:
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_Name( 
            /* [retval][out] */ BSTR *pbstrName) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_Name( 
            BSTR bstrName) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_RevisionLevel( 
            /* [retval][out] */ WORD *pwRevisionLevel) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_RevisionLevel( 
            WORD wRevisionLevel) = 0;
        
        virtual /* [id][propget] */ HRESULT STDMETHODCALLTYPE get_Values( 
            /* [retval][out] */ SAFEARRAY * *pValues) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_Values( 
            SAFEARRAY * values) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IMSTConfigPropertiesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IMSTConfigProperties * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IMSTConfigProperties * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IMSTConfigProperties * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IMSTConfigProperties * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IMSTConfigProperties * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IMSTConfigProperties * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IMSTConfigProperties * This,
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
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Name )( 
            IMSTConfigProperties * This,
            /* [retval][out] */ BSTR *pbstrName);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_Name )( 
            IMSTConfigProperties * This,
            BSTR bstrName);
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_RevisionLevel )( 
            IMSTConfigProperties * This,
            /* [retval][out] */ WORD *pwRevisionLevel);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_RevisionLevel )( 
            IMSTConfigProperties * This,
            WORD wRevisionLevel);
        
        /* [id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Values )( 
            IMSTConfigProperties * This,
            /* [retval][out] */ SAFEARRAY * *pValues);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_Values )( 
            IMSTConfigProperties * This,
            SAFEARRAY * values);
        
        END_INTERFACE
    } IMSTConfigPropertiesVtbl;

    interface IMSTConfigProperties
    {
        CONST_VTBL struct IMSTConfigPropertiesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IMSTConfigProperties_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IMSTConfigProperties_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IMSTConfigProperties_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IMSTConfigProperties_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IMSTConfigProperties_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IMSTConfigProperties_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IMSTConfigProperties_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IMSTConfigProperties_get_Name(This,pbstrName)	\
    ( (This)->lpVtbl -> get_Name(This,pbstrName) ) 

#define IMSTConfigProperties_put_Name(This,bstrName)	\
    ( (This)->lpVtbl -> put_Name(This,bstrName) ) 

#define IMSTConfigProperties_get_RevisionLevel(This,pwRevisionLevel)	\
    ( (This)->lpVtbl -> get_RevisionLevel(This,pwRevisionLevel) ) 

#define IMSTConfigProperties_put_RevisionLevel(This,wRevisionLevel)	\
    ( (This)->lpVtbl -> put_RevisionLevel(This,wRevisionLevel) ) 

#define IMSTConfigProperties_get_Values(This,pValues)	\
    ( (This)->lpVtbl -> get_Values(This,pValues) ) 

#define IMSTConfigProperties_put_Values(This,values)	\
    ( (This)->lpVtbl -> put_Values(This,values) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IMSTConfigProperties_INTERFACE_DEFINED__ */


#ifndef __IPortTreeProperties_INTERFACE_DEFINED__
#define __IPortTreeProperties_INTERFACE_DEFINED__

/* interface IPortTreeProperties */
/* [unique][nonextensible][dual][uuid][object] */ 

#define	AdminInternalPortPathCostDefaultValue	( 0 )


EXTERN_C const IID IID_IPortTreeProperties;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("AC34AAD3-89BF-4E3B-BF20-4C5D07A6C5C5")
    IPortTreeProperties : public IDispatch
    {
    public:
        virtual /* [nonbrowsable][propget] */ HRESULT STDMETHODCALLTYPE get_Name( 
            /* [retval][out] */ BSTR *pName) = 0;
        
        virtual /* [nonbrowsable][propget] */ HRESULT STDMETHODCALLTYPE get_ClassName( 
            /* [retval][out] */ BSTR *pClassName) = 0;
        
        virtual /* [custom][custom][helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_PortPriority( 
            /* [retval][out] */ enum PortPriority *pPriority) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_PortPriority( 
            enum PortPriority priority) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_learning( 
            /* [retval][out] */ VARIANT_BOOL *pbLearning) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_forwarding( 
            /* [retval][out] */ VARIANT_BOOL *pbForwarding) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_role( 
            /* [retval][out] */ enum PortRole *pRole) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_AdminInternalPortPathCost( 
            /* [retval][out] */ DWORD *pdwAdminInternalPortPathCost) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_AdminInternalPortPathCost( 
            DWORD dwAdminInternalPortPathCost) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_InternalPortPathCost( 
            /* [retval][out] */ DWORD *pdwInternalPortPathCost) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPortTreePropertiesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPortTreeProperties * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPortTreeProperties * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPortTreeProperties * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IPortTreeProperties * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IPortTreeProperties * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IPortTreeProperties * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IPortTreeProperties * This,
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
        
        /* [nonbrowsable][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Name )( 
            IPortTreeProperties * This,
            /* [retval][out] */ BSTR *pName);
        
        /* [nonbrowsable][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ClassName )( 
            IPortTreeProperties * This,
            /* [retval][out] */ BSTR *pClassName);
        
        /* [custom][custom][helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_PortPriority )( 
            IPortTreeProperties * This,
            /* [retval][out] */ enum PortPriority *pPriority);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_PortPriority )( 
            IPortTreeProperties * This,
            enum PortPriority priority);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_learning )( 
            IPortTreeProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pbLearning);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_forwarding )( 
            IPortTreeProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pbForwarding);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_role )( 
            IPortTreeProperties * This,
            /* [retval][out] */ enum PortRole *pRole);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_AdminInternalPortPathCost )( 
            IPortTreeProperties * This,
            /* [retval][out] */ DWORD *pdwAdminInternalPortPathCost);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_AdminInternalPortPathCost )( 
            IPortTreeProperties * This,
            DWORD dwAdminInternalPortPathCost);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_InternalPortPathCost )( 
            IPortTreeProperties * This,
            /* [retval][out] */ DWORD *pdwInternalPortPathCost);
        
        END_INTERFACE
    } IPortTreePropertiesVtbl;

    interface IPortTreeProperties
    {
        CONST_VTBL struct IPortTreePropertiesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPortTreeProperties_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPortTreeProperties_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPortTreeProperties_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPortTreeProperties_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IPortTreeProperties_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IPortTreeProperties_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IPortTreeProperties_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IPortTreeProperties_get_Name(This,pName)	\
    ( (This)->lpVtbl -> get_Name(This,pName) ) 

#define IPortTreeProperties_get_ClassName(This,pClassName)	\
    ( (This)->lpVtbl -> get_ClassName(This,pClassName) ) 

#define IPortTreeProperties_get_PortPriority(This,pPriority)	\
    ( (This)->lpVtbl -> get_PortPriority(This,pPriority) ) 

#define IPortTreeProperties_put_PortPriority(This,priority)	\
    ( (This)->lpVtbl -> put_PortPriority(This,priority) ) 

#define IPortTreeProperties_get_learning(This,pbLearning)	\
    ( (This)->lpVtbl -> get_learning(This,pbLearning) ) 

#define IPortTreeProperties_get_forwarding(This,pbForwarding)	\
    ( (This)->lpVtbl -> get_forwarding(This,pbForwarding) ) 

#define IPortTreeProperties_get_role(This,pRole)	\
    ( (This)->lpVtbl -> get_role(This,pRole) ) 

#define IPortTreeProperties_get_AdminInternalPortPathCost(This,pdwAdminInternalPortPathCost)	\
    ( (This)->lpVtbl -> get_AdminInternalPortPathCost(This,pdwAdminInternalPortPathCost) ) 

#define IPortTreeProperties_put_AdminInternalPortPathCost(This,dwAdminInternalPortPathCost)	\
    ( (This)->lpVtbl -> put_AdminInternalPortPathCost(This,dwAdminInternalPortPathCost) ) 

#define IPortTreeProperties_get_InternalPortPathCost(This,pdwInternalPortPathCost)	\
    ( (This)->lpVtbl -> get_InternalPortPathCost(This,pdwInternalPortPathCost) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPortTreeProperties_INTERFACE_DEFINED__ */


#ifndef __IPortProperties_INTERFACE_DEFINED__
#define __IPortProperties_INTERFACE_DEFINED__

/* interface IPortProperties */
/* [unique][nonextensible][dual][uuid][object] */ 

#define	SupportedSpeedDefaultValue	( PortSpeed100M )

#define	AutoEdgeDefaultValue	( TRUE )

#define	AdminEdgeDefaultValue	( FALSE )

#define	AdminExternalPortPathCostDefaultValue	( 0 )

#define	adminPointToPointMACDefaultValue	( Auto )


EXTERN_C const IID IID_IPortProperties;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("D94755FC-1586-4B2E-94CE-343F8DC2C679")
    IPortProperties : public IDispatch
    {
    public:
        virtual /* [nonbrowsable][propget] */ HRESULT STDMETHODCALLTYPE get_Name( 
            /* [retval][out] */ BSTR *pName) = 0;
        
        virtual /* [nonbrowsable][propget] */ HRESULT STDMETHODCALLTYPE get_ClassName( 
            /* [retval][out] */ BSTR *pClassName) = 0;
        
        virtual /* [nonbrowsable][id][propget] */ HRESULT STDMETHODCALLTYPE get_Side( 
            /* [retval][out] */ enum PortSide *pSide) = 0;
        
        virtual /* [nonbrowsable][id][propput] */ HRESULT STDMETHODCALLTYPE put_Side( 
            enum PortSide side) = 0;
        
        virtual /* [nonbrowsable][id][propget] */ HRESULT STDMETHODCALLTYPE get_Offset( 
            /* [retval][out] */ LONG *pOffset) = 0;
        
        virtual /* [nonbrowsable][id][propput] */ HRESULT STDMETHODCALLTYPE put_Offset( 
            LONG offset) = 0;
        
        virtual /* [custom][custom][helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_SupportedSpeed( 
            /* [retval][out] */ enum PortSpeed *pSpeed) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_SupportedSpeed( 
            enum PortSpeed speed) = 0;
        
        virtual /* [custom][helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_ActualSpeed( 
            /* [retval][out] */ DWORD *pdwActualSpeed) = 0;
        
        virtual /* [custom][custom][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_AutoEdge( 
            /* [retval][out] */ VARIANT_BOOL *pbAutoEdge) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_AutoEdge( 
            VARIANT_BOOL bAutoEdge) = 0;
        
        virtual /* [custom][custom][helpstring][id][propget] */ HRESULT STDMETHODCALLTYPE get_AdminEdge( 
            /* [retval][out] */ VARIANT_BOOL *pbAdminEdge) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_AdminEdge( 
            VARIANT_BOOL bAdminEdge) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_operEdge( 
            /* [retval][out] */ VARIANT_BOOL *pboperEdge) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_MAC_Operational( 
            /* [retval][out] */ VARIANT_BOOL *pbMAC_Operational) = 0;
        
        virtual /* [custom][helpstring][propget] */ HRESULT STDMETHODCALLTYPE get_DetectedPortPathCost( 
            /* [retval][out] */ DWORD *pDetectedPortPathCost) = 0;
        
        virtual /* [custom][custom][propget] */ HRESULT STDMETHODCALLTYPE get_AdminExternalPortPathCost( 
            /* [retval][out] */ DWORD *pdwAdminExternalPortPathCost) = 0;
        
        virtual /* [propput] */ HRESULT STDMETHODCALLTYPE put_AdminExternalPortPathCost( 
            DWORD dwAdminExternalPortPathCost) = 0;
        
        virtual /* [custom][propget] */ HRESULT STDMETHODCALLTYPE get_ExternalPortPathCost( 
            /* [retval][out] */ DWORD *pExternalPortPathCost) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_detectedPointToPointMAC( 
            /* [retval][out] */ VARIANT_BOOL *pbDetectedPointToPointMAC) = 0;
        
        virtual /* [custom][custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_adminPointToPointMAC( 
            /* [retval][out] */ enum AdminPointToPoint *pAdminP2P) = 0;
        
        virtual /* [id][propput] */ HRESULT STDMETHODCALLTYPE put_adminPointToPointMAC( 
            enum AdminPointToPoint adminP2P) = 0;
        
        virtual /* [custom][id][propget] */ HRESULT STDMETHODCALLTYPE get_operPointToPointMAC( 
            /* [retval][out] */ VARIANT_BOOL *pbOperPointToPointMAC) = 0;
        
        virtual /* [propget][nonbrowsable][id] */ HRESULT STDMETHODCALLTYPE get_Trees( 
            /* [retval][ref][out] */ SAFEARRAY * *ppsaItems) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IPortPropertiesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IPortProperties * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IPortProperties * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IPortProperties * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IPortProperties * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IPortProperties * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IPortProperties * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IPortProperties * This,
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
        
        /* [nonbrowsable][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Name )( 
            IPortProperties * This,
            /* [retval][out] */ BSTR *pName);
        
        /* [nonbrowsable][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ClassName )( 
            IPortProperties * This,
            /* [retval][out] */ BSTR *pClassName);
        
        /* [nonbrowsable][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Side )( 
            IPortProperties * This,
            /* [retval][out] */ enum PortSide *pSide);
        
        /* [nonbrowsable][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_Side )( 
            IPortProperties * This,
            enum PortSide side);
        
        /* [nonbrowsable][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_Offset )( 
            IPortProperties * This,
            /* [retval][out] */ LONG *pOffset);
        
        /* [nonbrowsable][id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_Offset )( 
            IPortProperties * This,
            LONG offset);
        
        /* [custom][custom][helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_SupportedSpeed )( 
            IPortProperties * This,
            /* [retval][out] */ enum PortSpeed *pSpeed);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_SupportedSpeed )( 
            IPortProperties * This,
            enum PortSpeed speed);
        
        /* [custom][helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ActualSpeed )( 
            IPortProperties * This,
            /* [retval][out] */ DWORD *pdwActualSpeed);
        
        /* [custom][custom][helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_AutoEdge )( 
            IPortProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pbAutoEdge);
        
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_AutoEdge )( 
            IPortProperties * This,
            VARIANT_BOOL bAutoEdge);
        
        /* [custom][custom][helpstring][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_AdminEdge )( 
            IPortProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pbAdminEdge);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_AdminEdge )( 
            IPortProperties * This,
            VARIANT_BOOL bAdminEdge);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_operEdge )( 
            IPortProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pboperEdge);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_MAC_Operational )( 
            IPortProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pbMAC_Operational);
        
        /* [custom][helpstring][propget] */ HRESULT ( STDMETHODCALLTYPE *get_DetectedPortPathCost )( 
            IPortProperties * This,
            /* [retval][out] */ DWORD *pDetectedPortPathCost);
        
        /* [custom][custom][propget] */ HRESULT ( STDMETHODCALLTYPE *get_AdminExternalPortPathCost )( 
            IPortProperties * This,
            /* [retval][out] */ DWORD *pdwAdminExternalPortPathCost);
        
        /* [propput] */ HRESULT ( STDMETHODCALLTYPE *put_AdminExternalPortPathCost )( 
            IPortProperties * This,
            DWORD dwAdminExternalPortPathCost);
        
        /* [custom][propget] */ HRESULT ( STDMETHODCALLTYPE *get_ExternalPortPathCost )( 
            IPortProperties * This,
            /* [retval][out] */ DWORD *pExternalPortPathCost);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_detectedPointToPointMAC )( 
            IPortProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pbDetectedPointToPointMAC);
        
        /* [custom][custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_adminPointToPointMAC )( 
            IPortProperties * This,
            /* [retval][out] */ enum AdminPointToPoint *pAdminP2P);
        
        /* [id][propput] */ HRESULT ( STDMETHODCALLTYPE *put_adminPointToPointMAC )( 
            IPortProperties * This,
            enum AdminPointToPoint adminP2P);
        
        /* [custom][id][propget] */ HRESULT ( STDMETHODCALLTYPE *get_operPointToPointMAC )( 
            IPortProperties * This,
            /* [retval][out] */ VARIANT_BOOL *pbOperPointToPointMAC);
        
        /* [propget][nonbrowsable][id] */ HRESULT ( STDMETHODCALLTYPE *get_Trees )( 
            IPortProperties * This,
            /* [retval][ref][out] */ SAFEARRAY * *ppsaItems);
        
        END_INTERFACE
    } IPortPropertiesVtbl;

    interface IPortProperties
    {
        CONST_VTBL struct IPortPropertiesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IPortProperties_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IPortProperties_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IPortProperties_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IPortProperties_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IPortProperties_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IPortProperties_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IPortProperties_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IPortProperties_get_Name(This,pName)	\
    ( (This)->lpVtbl -> get_Name(This,pName) ) 

#define IPortProperties_get_ClassName(This,pClassName)	\
    ( (This)->lpVtbl -> get_ClassName(This,pClassName) ) 

#define IPortProperties_get_Side(This,pSide)	\
    ( (This)->lpVtbl -> get_Side(This,pSide) ) 

#define IPortProperties_put_Side(This,side)	\
    ( (This)->lpVtbl -> put_Side(This,side) ) 

#define IPortProperties_get_Offset(This,pOffset)	\
    ( (This)->lpVtbl -> get_Offset(This,pOffset) ) 

#define IPortProperties_put_Offset(This,offset)	\
    ( (This)->lpVtbl -> put_Offset(This,offset) ) 

#define IPortProperties_get_SupportedSpeed(This,pSpeed)	\
    ( (This)->lpVtbl -> get_SupportedSpeed(This,pSpeed) ) 

#define IPortProperties_put_SupportedSpeed(This,speed)	\
    ( (This)->lpVtbl -> put_SupportedSpeed(This,speed) ) 

#define IPortProperties_get_ActualSpeed(This,pdwActualSpeed)	\
    ( (This)->lpVtbl -> get_ActualSpeed(This,pdwActualSpeed) ) 

#define IPortProperties_get_AutoEdge(This,pbAutoEdge)	\
    ( (This)->lpVtbl -> get_AutoEdge(This,pbAutoEdge) ) 

#define IPortProperties_put_AutoEdge(This,bAutoEdge)	\
    ( (This)->lpVtbl -> put_AutoEdge(This,bAutoEdge) ) 

#define IPortProperties_get_AdminEdge(This,pbAdminEdge)	\
    ( (This)->lpVtbl -> get_AdminEdge(This,pbAdminEdge) ) 

#define IPortProperties_put_AdminEdge(This,bAdminEdge)	\
    ( (This)->lpVtbl -> put_AdminEdge(This,bAdminEdge) ) 

#define IPortProperties_get_operEdge(This,pboperEdge)	\
    ( (This)->lpVtbl -> get_operEdge(This,pboperEdge) ) 

#define IPortProperties_get_MAC_Operational(This,pbMAC_Operational)	\
    ( (This)->lpVtbl -> get_MAC_Operational(This,pbMAC_Operational) ) 

#define IPortProperties_get_DetectedPortPathCost(This,pDetectedPortPathCost)	\
    ( (This)->lpVtbl -> get_DetectedPortPathCost(This,pDetectedPortPathCost) ) 

#define IPortProperties_get_AdminExternalPortPathCost(This,pdwAdminExternalPortPathCost)	\
    ( (This)->lpVtbl -> get_AdminExternalPortPathCost(This,pdwAdminExternalPortPathCost) ) 

#define IPortProperties_put_AdminExternalPortPathCost(This,dwAdminExternalPortPathCost)	\
    ( (This)->lpVtbl -> put_AdminExternalPortPathCost(This,dwAdminExternalPortPathCost) ) 

#define IPortProperties_get_ExternalPortPathCost(This,pExternalPortPathCost)	\
    ( (This)->lpVtbl -> get_ExternalPortPathCost(This,pExternalPortPathCost) ) 

#define IPortProperties_get_detectedPointToPointMAC(This,pbDetectedPointToPointMAC)	\
    ( (This)->lpVtbl -> get_detectedPointToPointMAC(This,pbDetectedPointToPointMAC) ) 

#define IPortProperties_get_adminPointToPointMAC(This,pAdminP2P)	\
    ( (This)->lpVtbl -> get_adminPointToPointMAC(This,pAdminP2P) ) 

#define IPortProperties_put_adminPointToPointMAC(This,adminP2P)	\
    ( (This)->lpVtbl -> put_adminPointToPointMAC(This,adminP2P) ) 

#define IPortProperties_get_operPointToPointMAC(This,pbOperPointToPointMAC)	\
    ( (This)->lpVtbl -> get_operPointToPointMAC(This,pbOperPointToPointMAC) ) 

#define IPortProperties_get_Trees(This,ppsaItems)	\
    ( (This)->lpVtbl -> get_Trees(This,ppsaItems) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IPortProperties_INTERFACE_DEFINED__ */


#ifndef __IWireProperties_INTERFACE_DEFINED__
#define __IWireProperties_INTERFACE_DEFINED__

/* interface IWireProperties */
/* [unique][nonextensible][dual][uuid][object] */ 


EXTERN_C const IID IID_IWireProperties;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("7635CC2E-4489-4893-B4F3-1B27F53B4EC1")
    IWireProperties : public IDispatch
    {
    public:
        virtual /* [propget][nonbrowsable][id] */ HRESULT STDMETHODCALLTYPE get_P0( 
            /* [retval][out] */ BSTR *pbstrEnd) = 0;
        
        virtual /* [propput][nonbrowsable][id] */ HRESULT STDMETHODCALLTYPE put_P0( 
            BSTR bstrEnd) = 0;
        
        virtual /* [propget][nonbrowsable][id] */ HRESULT STDMETHODCALLTYPE get_P1( 
            /* [retval][out] */ BSTR *pbstrEnd) = 0;
        
        virtual /* [propput][nonbrowsable][id] */ HRESULT STDMETHODCALLTYPE put_P1( 
            BSTR bstrEnd) = 0;
        
    };
    
    
#else 	/* C style interface */

    typedef struct IWirePropertiesVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IWireProperties * This,
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */ 
            _COM_Outptr_  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IWireProperties * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IWireProperties * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IWireProperties * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IWireProperties * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IWireProperties * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IWireProperties * This,
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
        
        /* [propget][nonbrowsable][id] */ HRESULT ( STDMETHODCALLTYPE *get_P0 )( 
            IWireProperties * This,
            /* [retval][out] */ BSTR *pbstrEnd);
        
        /* [propput][nonbrowsable][id] */ HRESULT ( STDMETHODCALLTYPE *put_P0 )( 
            IWireProperties * This,
            BSTR bstrEnd);
        
        /* [propget][nonbrowsable][id] */ HRESULT ( STDMETHODCALLTYPE *get_P1 )( 
            IWireProperties * This,
            /* [retval][out] */ BSTR *pbstrEnd);
        
        /* [propput][nonbrowsable][id] */ HRESULT ( STDMETHODCALLTYPE *put_P1 )( 
            IWireProperties * This,
            BSTR bstrEnd);
        
        END_INTERFACE
    } IWirePropertiesVtbl;

    interface IWireProperties
    {
        CONST_VTBL struct IWirePropertiesVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IWireProperties_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IWireProperties_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IWireProperties_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IWireProperties_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IWireProperties_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IWireProperties_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IWireProperties_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#define IWireProperties_get_P0(This,pbstrEnd)	\
    ( (This)->lpVtbl -> get_P0(This,pbstrEnd) ) 

#define IWireProperties_put_P0(This,bstrEnd)	\
    ( (This)->lpVtbl -> put_P0(This,bstrEnd) ) 

#define IWireProperties_get_P1(This,pbstrEnd)	\
    ( (This)->lpVtbl -> get_P1(This,pbstrEnd) ) 

#define IWireProperties_put_P1(This,bstrEnd)	\
    ( (This)->lpVtbl -> put_P1(This,bstrEnd) ) 

#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IWireProperties_INTERFACE_DEFINED__ */

#endif /* __SimulatorLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


