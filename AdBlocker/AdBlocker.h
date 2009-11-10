

/* this ALWAYS GENERATED file contains the definitions for the interfaces */


 /* File created by MIDL compiler version 7.00.0500 */
/* at Fri Nov 06 12:46:20 2009
 */
/* Compiler settings for .\AdBlocker.idl:
    Oicf, W1, Zp8, env=Win32 (32b run)
    protocol : dce , ms_ext, c_ext, robust
    error checks: allocation ref bounds_check enum stub_data 
    VC __declspec() decoration level: 
         __declspec(uuid()), __declspec(selectany), __declspec(novtable)
         DECLSPEC_UUID(), MIDL_INTERFACE()
*/
//@@MIDL_FILE_HEADING(  )

#pragma warning( disable: 4049 )  /* more than 64k source lines */


/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 475
#endif

#include "rpc.h"
#include "rpcndr.h"

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif // __RPCNDR_H_VERSION__

#ifndef COM_NO_WINDOWS_H
#include "windows.h"
#include "ole2.h"
#endif /*COM_NO_WINDOWS_H*/

#ifndef __AdBlocker_h__
#define __AdBlocker_h__

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#pragma once
#endif

/* Forward Declarations */ 

#ifndef __IIEPlugin_FWD_DEFINED__
#define __IIEPlugin_FWD_DEFINED__
typedef interface IIEPlugin IIEPlugin;
#endif 	/* __IIEPlugin_FWD_DEFINED__ */


#ifndef __AdPluginClass_FWD_DEFINED__
#define __AdPluginClass_FWD_DEFINED__

#ifdef __cplusplus
typedef class AdPluginClass AdPluginClass;
#else
typedef struct AdPluginClass AdPluginClass;
#endif /* __cplusplus */

#endif 	/* __AdPluginClass_FWD_DEFINED__ */


/* header files for imported files */
#include "oaidl.h"
#include "ocidl.h"

#ifdef __cplusplus
extern "C"{
#endif 


#ifndef __IIEPlugin_INTERFACE_DEFINED__
#define __IIEPlugin_INTERFACE_DEFINED__

/* interface IIEPlugin */
/* [unique][helpstring][dual][uuid][object] */ 


EXTERN_C const IID IID_IIEPlugin;

#if defined(__cplusplus) && !defined(CINTERFACE)
    
    MIDL_INTERFACE("9B871243-2983-495D-9FEB-D7059D0E8057")
    IIEPlugin : public IDispatch
    {
    public:
    };
    
#else 	/* C style interface */

    typedef struct IIEPluginVtbl
    {
        BEGIN_INTERFACE
        
        HRESULT ( STDMETHODCALLTYPE *QueryInterface )( 
            IIEPlugin * This,
            /* [in] */ REFIID riid,
            /* [iid_is][out] */ 
            __RPC__deref_out  void **ppvObject);
        
        ULONG ( STDMETHODCALLTYPE *AddRef )( 
            IIEPlugin * This);
        
        ULONG ( STDMETHODCALLTYPE *Release )( 
            IIEPlugin * This);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfoCount )( 
            IIEPlugin * This,
            /* [out] */ UINT *pctinfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetTypeInfo )( 
            IIEPlugin * This,
            /* [in] */ UINT iTInfo,
            /* [in] */ LCID lcid,
            /* [out] */ ITypeInfo **ppTInfo);
        
        HRESULT ( STDMETHODCALLTYPE *GetIDsOfNames )( 
            IIEPlugin * This,
            /* [in] */ REFIID riid,
            /* [size_is][in] */ LPOLESTR *rgszNames,
            /* [range][in] */ UINT cNames,
            /* [in] */ LCID lcid,
            /* [size_is][out] */ DISPID *rgDispId);
        
        /* [local] */ HRESULT ( STDMETHODCALLTYPE *Invoke )( 
            IIEPlugin * This,
            /* [in] */ DISPID dispIdMember,
            /* [in] */ REFIID riid,
            /* [in] */ LCID lcid,
            /* [in] */ WORD wFlags,
            /* [out][in] */ DISPPARAMS *pDispParams,
            /* [out] */ VARIANT *pVarResult,
            /* [out] */ EXCEPINFO *pExcepInfo,
            /* [out] */ UINT *puArgErr);
        
        END_INTERFACE
    } IIEPluginVtbl;

    interface IIEPlugin
    {
        CONST_VTBL struct IIEPluginVtbl *lpVtbl;
    };

    

#ifdef COBJMACROS


#define IIEPlugin_QueryInterface(This,riid,ppvObject)	\
    ( (This)->lpVtbl -> QueryInterface(This,riid,ppvObject) ) 

#define IIEPlugin_AddRef(This)	\
    ( (This)->lpVtbl -> AddRef(This) ) 

#define IIEPlugin_Release(This)	\
    ( (This)->lpVtbl -> Release(This) ) 


#define IIEPlugin_GetTypeInfoCount(This,pctinfo)	\
    ( (This)->lpVtbl -> GetTypeInfoCount(This,pctinfo) ) 

#define IIEPlugin_GetTypeInfo(This,iTInfo,lcid,ppTInfo)	\
    ( (This)->lpVtbl -> GetTypeInfo(This,iTInfo,lcid,ppTInfo) ) 

#define IIEPlugin_GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId)	\
    ( (This)->lpVtbl -> GetIDsOfNames(This,riid,rgszNames,cNames,lcid,rgDispId) ) 

#define IIEPlugin_Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr)	\
    ( (This)->lpVtbl -> Invoke(This,dispIdMember,riid,lcid,wFlags,pDispParams,pVarResult,pExcepInfo,puArgErr) ) 


#endif /* COBJMACROS */


#endif 	/* C style interface */




#endif 	/* __IIEPlugin_INTERFACE_DEFINED__ */



#ifndef __AdPluginLib_LIBRARY_DEFINED__
#define __AdPluginLib_LIBRARY_DEFINED__

/* library AdPluginLib */
/* [helpstring][version][uuid] */ 


EXTERN_C const IID LIBID_AdPluginLib;

EXTERN_C const CLSID CLSID_AdPluginClass;

#ifdef __cplusplus

class DECLSPEC_UUID("FFCB3198-32F3-4E8B-9539-4324694ED664")
AdPluginClass;
#endif
#endif /* __AdPluginLib_LIBRARY_DEFINED__ */

/* Additional Prototypes for ALL interfaces */

/* end of Additional Prototypes */

#ifdef __cplusplus
}
#endif

#endif


