/* nolfcom.h -- the little of COM the engine uses: IUnknown and the
 * STDMETHOD macros (DirectInput's interfaces, and the engine's DirectMusic
 * calls, which find no DirectMusic natively: CoCreateInstance fails). */
#ifndef NOLF_NOLFCOM_H
#define NOLF_NOLFCOM_H
#include "nolfwin.h"

#ifdef __cplusplus
#define NOLFCOM_EXTERN extern "C"
#else
#define NOLFCOM_EXTERN extern
#endif

typedef const GUID& REFIID;
typedef const GUID& REFCLSID;
#define E_NOINTERFACE           ((HRESULT)0x80004002L)
#define E_POINTER               ((HRESULT)0x80004003L)
#define E_UNEXPECTED            ((HRESULT)0x8000FFFFL)
#define CLASS_E_NOAGGREGATION   ((HRESULT)0x80040110L)
#define REGDB_E_CLASSNOTREG     ((HRESULT)0x80040154L)
#define CLSCTX_INPROC_SERVER    0x1
#define CLSCTX_INPROC           0x3
#define CLSCTX_ALL              0x17
#define COINIT_MULTITHREADED    0x0
#define COINIT_APARTMENTTHREADED 0x2

#define STDMETHODCALLTYPE
#define STDMETHOD(method)       virtual HRESULT method
#define STDMETHOD_(type, method) virtual type method
#define STDMETHODIMP            HRESULT
#define STDMETHODIMP_(type)     type
#define PURE                    = 0
#define THIS_
#define THIS
#define DECLARE_INTERFACE(iface)            struct iface
#define DECLARE_INTERFACE_(iface, base)     struct iface : public base

struct IUnknown {
    virtual HRESULT QueryInterface(REFIID riid, void** ppvObject) = 0;
    virtual ULONG   AddRef() = 0;
    virtual ULONG   Release() = 0;
};
typedef IUnknown* LPUNKNOWN;
NOLFCOM_EXTERN const GUID IID_IUnknown;
NOLFCOM_EXTERN const GUID GUID_NULL;

static inline BOOL IsEqualGUID(REFGUID a, REFGUID b) { return !memcmp(&a, &b, sizeof(GUID)); }
#define IsEqualIID(a, b)    IsEqualGUID(a, b)
#define IsEqualCLSID(a, b)  IsEqualGUID(a, b)
#ifndef __cplusplus
#error nolfcom.h is C++
#endif
static inline bool operator==(const GUID& a, const GUID& b) { return IsEqualGUID(a, b) != 0; }
static inline bool operator!=(const GUID& a, const GUID& b) { return !IsEqualGUID(a, b); }

NOLFCOM_EXTERN HRESULT CoInitialize(LPVOID pReserved);
NOLFCOM_EXTERN HRESULT CoInitializeEx(LPVOID pReserved, DWORD nCoInit);
NOLFCOM_EXTERN void    CoUninitialize(void);
NOLFCOM_EXTERN HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid,
                                        LPVOID* ppv);
NOLFCOM_EXTERN void*   CoTaskMemAlloc(size_t n);
NOLFCOM_EXTERN void    CoTaskMemFree(void* p);
#endif
