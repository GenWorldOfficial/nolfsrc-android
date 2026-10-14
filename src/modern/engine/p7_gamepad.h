// ----------------------------------------------------------------------- //
//
// MODULE  : p7_gamepad.h
//
// PURPOSE : Package P7 (MODERN.md), M-P7-01: the ##gamepad input device,
//           in lithtech.exe's input code (clientinput.cpp).
//
//           An XInput pad (an Xbox-style controller; what Steam Input
//           presents on a Steam Deck, and what Wine makes of most pads)
//           as one more DirectInput device: an object behind the
//           IDirectInputDevice2A interface that answers the calls the
//           engine makes of a joystick from XInput's state.  So binding
//           (rangebind "##gamepad" "A Button" 0 0 "Jump"), the buffered
//           reads, the analog axes, the config save and the Controls
//           folder's "press a key" capture all work on it unchanged.
//           It is the modern build's only joystick: DirectInput's are left
//           out.
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#ifndef __P7_GAMEPAD_H__
#define __P7_GAMEPAD_H__

#include <windows.h>
#include <dinput.h>

// Whether the device exists: XInput could be loaded (or the test pad,
// NOLFSRC_PADTEST, is on).  Decided once.
BOOL P7Pad_Available(void);

// The device's DIDEVICEINSTANCE, as EnumDevices would report it: name
// "##gamepad", a joystick of the gamepad subtype.
void P7Pad_GetInstance(DIDEVICEINSTANCEA* pInstance);

// Is this the ##gamepad's instance GUID?
BOOL P7Pad_IsPad(REFGUID guidInstance);

// CreateDevice, for any device: ours for its GUID, DirectInput's otherwise.
HRESULT P7Pad_CreateDevice(LPDIRECTINPUTA pDI, REFGUID guid, LPDIRECTINPUTDEVICEA* ppDevice);

#endif // __P7_GAMEPAD_H__
