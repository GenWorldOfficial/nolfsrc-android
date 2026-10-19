/* dinput.h -- DirectInput 5's interfaces, structs and constants, as the
 * engine's input (clientinput.cpp) and the modern build's gamepad device
 * (src/modern/engine/p7_gamepad.cpp, which implements a device itself) use
 * them. Native, DirectInputCreateA (native/plat/nolfplat_dinput.cpp) gives
 * keyboard and mouse devices over SDL; the gamepad is the modern build's
 * own, reading the pad through the XInput stand-in.
 *
 * Every user of these is compiled here, so the interfaces need DirectInput's
 * methods and meanings, not its exact binary layout; the structs the engine
 * fills in or reads by offset keep it (DIJOYSTATE for DIJOFS_*). */
#ifndef NOLF_DINPUT_H
#define NOLF_DINPUT_H
#include "nolfwin.h"
#include "nolfcom.h"

#ifndef DIRECTINPUT_VERSION
#define DIRECTINPUT_VERSION 0x0700
#endif

/* ---- results -------------------------------------------------------------------- */
#define DI_OK                   S_OK
#define DI_NOTATTACHED          S_FALSE
#define DI_BUFFEROVERFLOW       S_FALSE
#define DI_PROPNOEFFECT         S_FALSE
#define DI_NOEFFECT             S_FALSE
#define DI_POLLEDDEVICE         ((HRESULT)0x00000002L)
#define DIERR_GENERIC           E_FAIL
#define DIERR_INVALIDPARAM      E_INVALIDARG
#define DIERR_OUTOFMEMORY       E_OUTOFMEMORY
#define DIERR_UNSUPPORTED       E_NOTIMPL
#define DIERR_NOINTERFACE       E_NOINTERFACE
#define DIERR_NOTINITIALIZED    ((HRESULT)0x80070015L)
#define DIERR_NOTACQUIRED       ((HRESULT)0x8007000CL)
#define DIERR_INPUTLOST         ((HRESULT)0x8007001EL)
#define DIERR_ACQUIRED          ((HRESULT)0x800700AAL)
#define DIERR_DEVICENOTREG      ((HRESULT)0x80040154L)
#define DIERR_OTHERAPPHASPRIO   ((HRESULT)0x80070005L)
#define DIERR_OBJECTNOTFOUND    ((HRESULT)0x80070002L)
#define DIERR_OLDDIRECTINPUTVERSION ((HRESULT)0x8007047EL)
#define DIERR_BETADIRECTINPUTVERSION ((HRESULT)0x80070481L)
#define DIENUM_STOP             0
#define DIENUM_CONTINUE         1

/* ---- device types ------------------------------------------------------------------ */
#define DIDEVTYPE_DEVICE        1
#define DIDEVTYPE_MOUSE         2
#define DIDEVTYPE_KEYBOARD      3
#define DIDEVTYPE_JOYSTICK      4
#define DIDEVTYPE_HID           0x00010000
#define DIDEVTYPEMOUSE_UNKNOWN      1
#define DIDEVTYPEMOUSE_TRADITIONAL  2
#define DIDEVTYPEKEYBOARD_PCENH     4
#define DIDEVTYPEJOYSTICK_UNKNOWN   1
#define DIDEVTYPEJOYSTICK_TRADITIONAL 2
#define DIDEVTYPEJOYSTICK_GAMEPAD   5
#define GET_DIDEVICE_TYPE(dwDevType)    LOBYTE(dwDevType)
#define GET_DIDEVICE_SUBTYPE(dwDevType) HIBYTE(dwDevType)
#define DIEDFL_ALLDEVICES       0x00000000
#define DIEDFL_ATTACHEDONLY     0x00000001

/* ---- objects and data formats --------------------------------------------------------- */
#define DIDFT_ALL               0x00000000
#define DIDFT_RELAXIS           0x00000001
#define DIDFT_ABSAXIS           0x00000002
#define DIDFT_AXIS              0x00000003
#define DIDFT_PSHBUTTON         0x00000004
#define DIDFT_TGLBUTTON         0x00000008
#define DIDFT_BUTTON            0x0000000C
#define DIDFT_POV               0x00000010
#define DIDFT_COLLECTION        0x00000040
#define DIDFT_NODATA            0x00000080
#define DIDFT_ANYINSTANCE       0x00FFFF00
#define DIDFT_INSTANCEMASK      DIDFT_ANYINSTANCE
#define DIDFT_MAKEINSTANCE(n)   ((WORD)(n) << 8)
#define DIDFT_GETTYPE(n)        LOBYTE(n)
#define DIDFT_GETINSTANCE(n)    LOWORD((n) >> 8)
#define DIDFT_FFACTUATOR        0x01000000
#define DIDFT_FFEFFECTTRIGGER   0x02000000
#define DIDFT_OPTIONAL          0x80000000
#define DIDF_ABSAXIS            0x00000001
#define DIDF_RELAXIS            0x00000002
#define DIDOI_FFACTUATOR        0x00000001
#define DIDOI_POLLED            0x00008000
#define DIDOI_ASPECTPOSITION    0x00000100
#define DIDOI_ASPECTVELOCITY    0x00000200
#define DIDOI_ASPECTACCEL       0x00000300
#define DIDOI_ASPECTFORCE       0x00000400
#define DIDC_ATTACHED           0x00000001
#define DIDC_POLLEDDEVICE       0x00000002
#define DIDC_EMULATED           0x00000004
#define DIDC_POLLEDDATAFORMAT   0x00000008
#define DIDC_FORCEFEEDBACK      0x00000100
#define DISCL_EXCLUSIVE         0x00000001
#define DISCL_NONEXCLUSIVE      0x00000002
#define DISCL_FOREGROUND        0x00000004
#define DISCL_BACKGROUND        0x00000008
#define DIGDD_PEEK              0x00000001

typedef struct _DIOBJECTDATAFORMAT {
    const GUID* pguid;
    DWORD       dwOfs;
    DWORD       dwType;
    DWORD       dwFlags;
} DIOBJECTDATAFORMAT, *LPDIOBJECTDATAFORMAT;
typedef const DIOBJECTDATAFORMAT* LPCDIOBJECTDATAFORMAT;

typedef struct _DIDATAFORMAT {
    DWORD                dwSize;
    DWORD                dwObjSize;
    DWORD                dwFlags;
    DWORD                dwDataSize;
    DWORD                dwNumObjs;
    LPDIOBJECTDATAFORMAT rgodf;
} DIDATAFORMAT, *LPDIDATAFORMAT;
typedef const DIDATAFORMAT* LPCDIDATAFORMAT;

typedef struct _DIDEVICEOBJECTDATA {
    DWORD dwOfs;
    DWORD dwData;
    DWORD dwTimeStamp;
    DWORD dwSequence;
} DIDEVICEOBJECTDATA, *LPDIDEVICEOBJECTDATA;
typedef const DIDEVICEOBJECTDATA* LPCDIDEVICEOBJECTDATA;

typedef struct DIJOYSTATE {
    LONG  lX, lY, lZ;
    LONG  lRx, lRy, lRz;
    LONG  rglSlider[2];
    DWORD rgdwPOV[4];
    BYTE  rgbButtons[32];
} DIJOYSTATE, *LPDIJOYSTATE;
typedef struct _DIMOUSESTATE {
    LONG lX, lY, lZ;
    BYTE rgbButtons[4];
} DIMOUSESTATE, *LPDIMOUSESTATE;

#define DIJOFS_X            offsetof(DIJOYSTATE, lX)
#define DIJOFS_Y            offsetof(DIJOYSTATE, lY)
#define DIJOFS_Z            offsetof(DIJOYSTATE, lZ)
#define DIJOFS_RX           offsetof(DIJOYSTATE, lRx)
#define DIJOFS_RY           offsetof(DIJOYSTATE, lRy)
#define DIJOFS_RZ           offsetof(DIJOYSTATE, lRz)
#define DIJOFS_SLIDER(n)    (offsetof(DIJOYSTATE, rglSlider) + (n) * sizeof(LONG))
#define DIJOFS_POV(n)       (offsetof(DIJOYSTATE, rgdwPOV) + (n) * sizeof(DWORD))
#define DIJOFS_BUTTON(n)    (offsetof(DIJOYSTATE, rgbButtons) + (n))
#define DIMOFS_X            offsetof(DIMOUSESTATE, lX)
#define DIMOFS_Y            offsetof(DIMOUSESTATE, lY)
#define DIMOFS_Z            offsetof(DIMOUSESTATE, lZ)
#define DIMOFS_BUTTON0      (offsetof(DIMOUSESTATE, rgbButtons) + 0)
#define DIMOFS_BUTTON1      (offsetof(DIMOUSESTATE, rgbButtons) + 1)
#define DIMOFS_BUTTON2      (offsetof(DIMOUSESTATE, rgbButtons) + 2)
#define DIMOFS_BUTTON3      (offsetof(DIMOUSESTATE, rgbButtons) + 3)

/* ---- properties ----------------------------------------------------------------------- */
typedef struct DIPROPHEADER {
    DWORD dwSize;
    DWORD dwHeaderSize;
    DWORD dwObj;
    DWORD dwHow;
} DIPROPHEADER, *LPDIPROPHEADER;
typedef const DIPROPHEADER* LPCDIPROPHEADER;
typedef struct DIPROPDWORD { DIPROPHEADER diph; DWORD dwData; } DIPROPDWORD, *LPDIPROPDWORD;
typedef const DIPROPDWORD* LPCDIPROPDWORD;
typedef struct DIPROPRANGE { DIPROPHEADER diph; LONG lMin, lMax; } DIPROPRANGE, *LPDIPROPRANGE;
typedef const DIPROPRANGE* LPCDIPROPRANGE;
#define DIPH_DEVICE         0
#define DIPH_BYOFFSET       1
#define DIPH_BYID           2
#define DIPROPRANGE_NOMIN   ((LONG)0x80000000L)
#define DIPROPRANGE_NOMAX   ((LONG)0x7FFFFFFFL)
/* a property is named by a small number standing in for a GUID's address */
#define MAKEDIPROP(prop)    (*(const GUID*)(uintptr_t)(prop))
#define DIPROP_BUFFERSIZE   MAKEDIPROP(1)
#define DIPROP_AXISMODE     MAKEDIPROP(2)
#define DIPROPAXISMODE_ABS  0
#define DIPROPAXISMODE_REL  1
#define DIPROP_GRANULARITY  MAKEDIPROP(3)
#define DIPROP_RANGE        MAKEDIPROP(4)
#define DIPROP_DEADZONE     MAKEDIPROP(5)
#define DIPROP_SATURATION   MAKEDIPROP(6)
#define DIPROP_FFGAIN       MAKEDIPROP(7)
#define DIPROP_FFLOAD       MAKEDIPROP(8)
#define DIPROP_AUTOCENTER   MAKEDIPROP(9)
#define DIPROP_CALIBRATIONMODE MAKEDIPROP(10)
/* which property a REFGUID names, for an implementation */
#define NOLF_DIPROP_ID(rguid) ((uintptr_t)&(rguid))

/* ---- instances and capabilities -------------------------------------------------------- */
typedef struct DIDEVCAPS {
    DWORD dwSize;
    DWORD dwFlags;
    DWORD dwDevType;
    DWORD dwAxes;
    DWORD dwButtons;
    DWORD dwPOVs;
    DWORD dwFFSamplePeriod;
    DWORD dwFFMinTimeResolution;
    DWORD dwFirmwareRevision;
    DWORD dwHardwareRevision;
    DWORD dwFFDriverVersion;
} DIDEVCAPS, *LPDIDEVCAPS;

typedef struct DIDEVICEOBJECTINSTANCEA {
    DWORD dwSize;
    GUID  guidType;
    DWORD dwOfs;
    DWORD dwType;
    DWORD dwFlags;
    CHAR  tszName[MAX_PATH];
    DWORD dwFFMaxForce;
    DWORD dwFFForceResolution;
    WORD  wCollectionNumber;
    WORD  wDesignatorIndex;
    WORD  wUsagePage;
    WORD  wUsage;
    DWORD dwDimension;
    WORD  wExponent;
    WORD  wReserved;
} DIDEVICEOBJECTINSTANCEA, *LPDIDEVICEOBJECTINSTANCEA;
typedef const DIDEVICEOBJECTINSTANCEA* LPCDIDEVICEOBJECTINSTANCEA;

typedef struct DIDEVICEINSTANCEA {
    DWORD dwSize;
    GUID  guidInstance;
    GUID  guidProduct;
    DWORD dwDevType;
    CHAR  tszInstanceName[MAX_PATH];
    CHAR  tszProductName[MAX_PATH];
    GUID  guidFFDriver;
    WORD  wUsagePage;
    WORD  wUsage;
} DIDEVICEINSTANCEA, *LPDIDEVICEINSTANCEA;
typedef const DIDEVICEINSTANCEA* LPCDIDEVICEINSTANCEA;

typedef DIDEVICEINSTANCEA       DIDEVICEINSTANCE;
typedef LPDIDEVICEINSTANCEA     LPDIDEVICEINSTANCE;
typedef LPCDIDEVICEINSTANCEA    LPCDIDEVICEINSTANCE;
typedef DIDEVICEOBJECTINSTANCEA DIDEVICEOBJECTINSTANCE;
typedef LPDIDEVICEOBJECTINSTANCEA LPDIDEVICEOBJECTINSTANCE;
typedef LPCDIDEVICEOBJECTINSTANCEA LPCDIDEVICEOBJECTINSTANCE;

/* force feedback: declared for the interface; nothing native has any */
typedef struct DIEFFECT { DWORD dwSize; } DIEFFECT, *LPDIEFFECT;
typedef const DIEFFECT* LPCDIEFFECT;
typedef struct DIEFFECTINFOA { DWORD dwSize; GUID guid; DWORD dwEffType, dwStaticParams, dwDynamicParams; CHAR tszName[MAX_PATH]; } DIEFFECTINFOA, *LPDIEFFECTINFOA;
typedef const DIEFFECTINFOA* LPCDIEFFECTINFOA;
typedef struct DIEFFESCAPE { DWORD dwSize, dwCommand; LPVOID lpvInBuffer; DWORD cbInBuffer; LPVOID lpvOutBuffer; DWORD cbOutBuffer; } DIEFFESCAPE, *LPDIEFFESCAPE;

/* ---- callbacks ------------------------------------------------------------------------- */
typedef BOOL (*LPDIENUMDEVICESCALLBACKA)(LPCDIDEVICEINSTANCEA, LPVOID);
typedef BOOL (*LPDIENUMDEVICEOBJECTSCALLBACKA)(LPCDIDEVICEOBJECTINSTANCEA, LPVOID);
typedef BOOL (*LPDIENUMEFFECTSCALLBACKA)(LPCDIEFFECTINFOA, LPVOID);
#define LPDIENUMDEVICESCALLBACK         LPDIENUMDEVICESCALLBACKA
#define LPDIENUMDEVICEOBJECTSCALLBACK   LPDIENUMDEVICEOBJECTSCALLBACKA

/* ---- interfaces ------------------------------------------------------------------------ */
struct IDirectInputEffect : public IUnknown {
    STDMETHOD(Initialize)(HINSTANCE, DWORD, REFGUID) PURE;
    STDMETHOD(GetEffectGuid)(GUID*) PURE;
    STDMETHOD(GetParameters)(LPDIEFFECT, DWORD) PURE;
    STDMETHOD(SetParameters)(LPCDIEFFECT, DWORD) PURE;
    STDMETHOD(Start)(DWORD, DWORD) PURE;
    STDMETHOD(Stop)() PURE;
    STDMETHOD(GetEffectStatus)(LPDWORD) PURE;
    STDMETHOD(Download)() PURE;
    STDMETHOD(Unload)() PURE;
    STDMETHOD(Escape)(LPDIEFFESCAPE) PURE;
};
typedef struct IDirectInputEffect* LPDIRECTINPUTEFFECT;
typedef BOOL (*LPDIENUMCREATEDEFFECTOBJECTSCALLBACK)(LPDIRECTINPUTEFFECT, LPVOID);

struct IDirectInputDeviceA : public IUnknown {
    STDMETHOD(GetCapabilities)(LPDIDEVCAPS) PURE;
    STDMETHOD(EnumObjects)(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD) PURE;
    STDMETHOD(GetProperty)(REFGUID, LPDIPROPHEADER) PURE;
    STDMETHOD(SetProperty)(REFGUID, LPCDIPROPHEADER) PURE;
    STDMETHOD(Acquire)() PURE;
    STDMETHOD(Unacquire)() PURE;
    STDMETHOD(GetDeviceState)(DWORD, LPVOID) PURE;
    STDMETHOD(GetDeviceData)(DWORD, LPDIDEVICEOBJECTDATA, LPDWORD, DWORD) PURE;
    STDMETHOD(SetDataFormat)(LPCDIDATAFORMAT) PURE;
    STDMETHOD(SetEventNotification)(HANDLE) PURE;
    STDMETHOD(SetCooperativeLevel)(HWND, DWORD) PURE;
    STDMETHOD(GetObjectInfo)(LPDIDEVICEOBJECTINSTANCEA, DWORD, DWORD) PURE;
    STDMETHOD(GetDeviceInfo)(LPDIDEVICEINSTANCEA) PURE;
    STDMETHOD(RunControlPanel)(HWND, DWORD) PURE;
    STDMETHOD(Initialize)(HINSTANCE, DWORD, REFGUID) PURE;
};
struct IDirectInputDevice2A : public IDirectInputDeviceA {
    STDMETHOD(CreateEffect)(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT*, LPUNKNOWN) PURE;
    STDMETHOD(EnumEffects)(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD) PURE;
    STDMETHOD(GetEffectInfo)(LPDIEFFECTINFOA, REFGUID) PURE;
    STDMETHOD(GetForceFeedbackState)(LPDWORD) PURE;
    STDMETHOD(SendForceFeedbackCommand)(DWORD) PURE;
    STDMETHOD(EnumCreatedEffectObjects)(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD) PURE;
    STDMETHOD(Escape)(LPDIEFFESCAPE) PURE;
    STDMETHOD(Poll)() PURE;
    STDMETHOD(SendDeviceData)(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD) PURE;
};
typedef struct IDirectInputDeviceA*  LPDIRECTINPUTDEVICEA;
typedef struct IDirectInputDevice2A* LPDIRECTINPUTDEVICE2A;
typedef IDirectInputDeviceA  IDirectInputDevice;
typedef IDirectInputDevice2A IDirectInputDevice2;
typedef LPDIRECTINPUTDEVICEA  LPDIRECTINPUTDEVICE;
typedef LPDIRECTINPUTDEVICE2A LPDIRECTINPUTDEVICE2;

struct IDirectInputA : public IUnknown {
    STDMETHOD(CreateDevice)(REFGUID, LPDIRECTINPUTDEVICEA*, LPUNKNOWN) PURE;
    STDMETHOD(EnumDevices)(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD) PURE;
    STDMETHOD(GetDeviceStatus)(REFGUID) PURE;
    STDMETHOD(RunControlPanel)(HWND, DWORD) PURE;
    STDMETHOD(Initialize)(HINSTANCE, DWORD) PURE;
};
typedef struct IDirectInputA* LPDIRECTINPUTA;
typedef IDirectInputA  IDirectInput;
typedef LPDIRECTINPUTA LPDIRECTINPUT;

/* ---- GUIDs and the standard formats (nolfplat_dinput.cpp) ------------------------------------ */
NOLFCOM_EXTERN const GUID GUID_XAxis, GUID_YAxis, GUID_ZAxis, GUID_RxAxis, GUID_RyAxis, GUID_RzAxis;
NOLFCOM_EXTERN const GUID GUID_Slider, GUID_Button, GUID_Key, GUID_POV, GUID_Unknown;
NOLFCOM_EXTERN const GUID GUID_SysMouse, GUID_SysKeyboard, GUID_Joystick;
NOLFCOM_EXTERN const GUID IID_IDirectInputA, IID_IDirectInputDeviceA, IID_IDirectInputDevice2A;
NOLFCOM_EXTERN const DIDATAFORMAT c_dfDIJoystick, c_dfDIMouse, c_dfDIKeyboard;

NOLFCOM_EXTERN HRESULT DirectInputCreateA(HINSTANCE hInst, DWORD dwVersion, LPDIRECTINPUTA* ppDI, LPUNKNOWN pUnkOuter);
#define DirectInputCreate DirectInputCreateA

/* ---- key codes (the keyboard's scan codes) ------------------------------------------------ */

#define DIK_ESCAPE         0x01
#define DIK_1              0x02
#define DIK_2              0x03
#define DIK_3              0x04
#define DIK_4              0x05
#define DIK_5              0x06
#define DIK_6              0x07
#define DIK_7              0x08
#define DIK_8              0x09
#define DIK_9              0x0A
#define DIK_0              0x0B
#define DIK_MINUS          0x0C
#define DIK_EQUALS         0x0D
#define DIK_BACK           0x0E
#define DIK_TAB            0x0F
#define DIK_Q              0x10
#define DIK_W              0x11
#define DIK_E              0x12
#define DIK_R              0x13
#define DIK_T              0x14
#define DIK_Y              0x15
#define DIK_U              0x16
#define DIK_I              0x17
#define DIK_O              0x18
#define DIK_P              0x19
#define DIK_LBRACKET       0x1A
#define DIK_RBRACKET       0x1B
#define DIK_RETURN         0x1C
#define DIK_LCONTROL       0x1D
#define DIK_A              0x1E
#define DIK_S              0x1F
#define DIK_D              0x20
#define DIK_F              0x21
#define DIK_G              0x22
#define DIK_H              0x23
#define DIK_J              0x24
#define DIK_K              0x25
#define DIK_L              0x26
#define DIK_SEMICOLON      0x27
#define DIK_APOSTROPHE     0x28
#define DIK_GRAVE          0x29
#define DIK_LSHIFT         0x2A
#define DIK_BACKSLASH      0x2B
#define DIK_Z              0x2C
#define DIK_X              0x2D
#define DIK_C              0x2E
#define DIK_V              0x2F
#define DIK_B              0x30
#define DIK_N              0x31
#define DIK_M              0x32
#define DIK_COMMA          0x33
#define DIK_PERIOD         0x34
#define DIK_SLASH          0x35
#define DIK_RSHIFT         0x36
#define DIK_MULTIPLY       0x37
#define DIK_LMENU          0x38
#define DIK_SPACE          0x39
#define DIK_CAPITAL        0x3A
#define DIK_F1             0x3B
#define DIK_F2             0x3C
#define DIK_F3             0x3D
#define DIK_F4             0x3E
#define DIK_F5             0x3F
#define DIK_F6             0x40
#define DIK_F7             0x41
#define DIK_F8             0x42
#define DIK_F9             0x43
#define DIK_F10            0x44
#define DIK_NUMLOCK        0x45
#define DIK_SCROLL         0x46
#define DIK_NUMPAD7        0x47
#define DIK_NUMPAD8        0x48
#define DIK_NUMPAD9        0x49
#define DIK_SUBTRACT       0x4A
#define DIK_NUMPAD4        0x4B
#define DIK_NUMPAD5        0x4C
#define DIK_NUMPAD6        0x4D
#define DIK_ADD            0x4E
#define DIK_NUMPAD1        0x4F
#define DIK_NUMPAD2        0x50
#define DIK_NUMPAD3        0x51
#define DIK_NUMPAD0        0x52
#define DIK_DECIMAL        0x53
#define DIK_OEM_102        0x56
#define DIK_F11            0x57
#define DIK_F12            0x58
#define DIK_F13            0x64
#define DIK_F14            0x65
#define DIK_F15            0x66
#define DIK_KANA           0x70
#define DIK_ABNT_C1        0x73
#define DIK_CONVERT        0x79
#define DIK_NOCONVERT      0x7B
#define DIK_YEN            0x7D
#define DIK_ABNT_C2        0x7E
#define DIK_NUMPADEQUALS   0x8D
#define DIK_PREVTRACK      0x90
#define DIK_AT             0x91
#define DIK_COLON          0x92
#define DIK_UNDERLINE      0x93
#define DIK_KANJI          0x94
#define DIK_STOP           0x95
#define DIK_AX             0x96
#define DIK_UNLABELED      0x97
#define DIK_NEXTTRACK      0x99
#define DIK_NUMPADENTER    0x9C
#define DIK_RCONTROL       0x9D
#define DIK_MUTE           0xA0
#define DIK_CALCULATOR     0xA1
#define DIK_PLAYPAUSE      0xA2
#define DIK_MEDIASTOP      0xA4
#define DIK_VOLUMEDOWN     0xAE
#define DIK_VOLUMEUP       0xB0
#define DIK_WEBHOME        0xB2
#define DIK_NUMPADCOMMA    0xB3
#define DIK_DIVIDE         0xB5
#define DIK_SYSRQ          0xB7
#define DIK_RMENU          0xB8
#define DIK_PAUSE          0xC5
#define DIK_HOME           0xC7
#define DIK_UP             0xC8
#define DIK_PRIOR          0xC9
#define DIK_LEFT           0xCB
#define DIK_RIGHT          0xCD
#define DIK_END            0xCF
#define DIK_DOWN           0xD0
#define DIK_NEXT           0xD1
#define DIK_INSERT         0xD2
#define DIK_DELETE         0xD3
#define DIK_LWIN           0xDB
#define DIK_RWIN           0xDC
#define DIK_APPS           0xDD
#define DIK_POWER          0xDE
#define DIK_SLEEP          0xDF
#define DIK_WAKE           0xE3
#define DIK_BACKSPACE      DIK_BACK
#define DIK_NUMPADSTAR     DIK_MULTIPLY
#define DIK_LALT           DIK_LMENU
#define DIK_CAPSLOCK       DIK_CAPITAL
#define DIK_NUMPADMINUS    DIK_SUBTRACT
#define DIK_NUMPADPLUS     DIK_ADD
#define DIK_NUMPADPERIOD   DIK_DECIMAL
#define DIK_NUMPADSLASH    DIK_DIVIDE
#define DIK_RALT           DIK_RMENU
#define DIK_UPARROW        DIK_UP
#define DIK_PGUP           DIK_PRIOR
#define DIK_LEFTARROW      DIK_LEFT
#define DIK_RIGHTARROW     DIK_RIGHT
#define DIK_DOWNARROW      DIK_DOWN
#define DIK_PGDN           DIK_NEXT
#define DIK_CIRCUMFLEX     DIK_PREVTRACK

#endif
