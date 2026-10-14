// ----------------------------------------------------------------------- //
//
// MODULE  : p7_gamepad.cpp
//
// PURPOSE : Package P7 (MODERN.md), M-P7-01: the ##gamepad input device.
//           See p7_gamepad.h.
//
//           The device is a DirectInput joystick of the gamepad subtype
//           with six absolute axes and sixteen push buttons:
//
//             Left Stick X / Y, Right Stick X / Y    -1000 .. 1000, Y down
//                                                    positive (as a
//                                                    joystick's)
//             Left / Right Trigger Axis              0 .. 1000
//             A / B / X / Y Button, Left / Right Bumper, Back, Start,
//             Left / Right Stick Click, D-Pad Up / Down / Left / Right,
//             Left / Right Trigger (pressed past XInput's own threshold)
//
//           It reads the pad through p7_padread.h (the first connected
//           XInput pad, or the test pad), and does what a foreground
//           DirectInput device does: it can be acquired only while the
//           game's window is the foreground one, and losing the
//           foreground loses the acquisition.  Changes are buffered as
//           DIDEVICEOBJECTDATA in the data format the engine set,
//           standard (c_dfDIJoystick) or its own.
//
//           It is the only joystick: the engine leaves DirectInput's out
//           (under Wine an XInput pad is one of them as well).
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#include "p7_gamepad.h"
#include "p7_padread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{
	// Not in VC6's (DirectInput 5) dinput.h.
	#ifndef DIDFT_OPTIONAL
	#define DIDFT_OPTIONAL		0x80000000
	#endif

	// {4E4F4C46-5037-4741-4D45-504144000001}: "NOLF", "P7", "GAMEPAD".
	const GUID c_guidPad =
		{ 0x4e4f4c46, 0x5037, 0x4741, { 0x4d, 0x45, 0x50, 0x41, 0x44, 0x00, 0x00, 0x01 } };

	// ------------------------------------------------------------------ //
	// The objects.

	enum { P7_AXIS, P7_BUTTON };

	struct P7Object
	{
		const char*	m_pName;
		const GUID*	m_pGuid;
		int			m_nKind;
		DWORD		m_nInstance;
		DWORD		m_dwStdOfs;			// offset in c_dfDIJoystick's DIJOYSTATE
		LONG		m_nMin;
		LONG		m_nMax;
	};

	const P7Object s_Objects[] =
	{
		{ "Left Stick X",		&GUID_XAxis,	P7_AXIS,	0,	DIJOFS_X,	-1000, 1000 },
		{ "Left Stick Y",		&GUID_YAxis,	P7_AXIS,	1,	DIJOFS_Y,	-1000, 1000 },
		{ "Left Trigger Axis",	&GUID_ZAxis,	P7_AXIS,	2,	DIJOFS_Z,	0, 1000 },
		{ "Right Stick X",		&GUID_RxAxis,	P7_AXIS,	3,	DIJOFS_RX,	-1000, 1000 },
		{ "Right Stick Y",		&GUID_RyAxis,	P7_AXIS,	4,	DIJOFS_RY,	-1000, 1000 },
		{ "Right Trigger Axis",	&GUID_RzAxis,	P7_AXIS,	5,	DIJOFS_RZ,	0, 1000 },
		{ "A Button",			&GUID_Button,	P7_BUTTON,	0,	DIJOFS_BUTTON(0),	0, 0 },
		{ "B Button",			&GUID_Button,	P7_BUTTON,	1,	DIJOFS_BUTTON(1),	0, 0 },
		{ "X Button",			&GUID_Button,	P7_BUTTON,	2,	DIJOFS_BUTTON(2),	0, 0 },
		{ "Y Button",			&GUID_Button,	P7_BUTTON,	3,	DIJOFS_BUTTON(3),	0, 0 },
		{ "Left Bumper",		&GUID_Button,	P7_BUTTON,	4,	DIJOFS_BUTTON(4),	0, 0 },
		{ "Right Bumper",		&GUID_Button,	P7_BUTTON,	5,	DIJOFS_BUTTON(5),	0, 0 },
		{ "Back",				&GUID_Button,	P7_BUTTON,	6,	DIJOFS_BUTTON(6),	0, 0 },
		{ "Start",				&GUID_Button,	P7_BUTTON,	7,	DIJOFS_BUTTON(7),	0, 0 },
		{ "Left Stick Click",	&GUID_Button,	P7_BUTTON,	8,	DIJOFS_BUTTON(8),	0, 0 },
		{ "Right Stick Click",	&GUID_Button,	P7_BUTTON,	9,	DIJOFS_BUTTON(9),	0, 0 },
		{ "D-Pad Up",			&GUID_Button,	P7_BUTTON,	10,	DIJOFS_BUTTON(10),	0, 0 },
		{ "D-Pad Down",			&GUID_Button,	P7_BUTTON,	11,	DIJOFS_BUTTON(11),	0, 0 },
		{ "D-Pad Left",			&GUID_Button,	P7_BUTTON,	12,	DIJOFS_BUTTON(12),	0, 0 },
		{ "D-Pad Right",		&GUID_Button,	P7_BUTTON,	13,	DIJOFS_BUTTON(13),	0, 0 },
		{ "Left Trigger",		&GUID_Button,	P7_BUTTON,	14,	DIJOFS_BUTTON(14),	0, 0 },
		{ "Right Trigger",		&GUID_Button,	P7_BUTTON,	15,	DIJOFS_BUTTON(15),	0, 0 },
	};

	enum
	{
		P7_OBJECTS	= sizeof(s_Objects) / sizeof(s_Objects[0]),
		P7_AXES		= 6,
		P7_BUTTONS	= 16,
		P7_BUFFER	= 256				// the most events a buffer holds
	};

	// The button each XInput bit is, in s_Objects' order from 6.
	const WORD s_ButtonBits[P7_BUTTONS - 2] =
	{
		P7PAD_A, P7PAD_B, P7PAD_X, P7PAD_Y, P7PAD_LEFT_SHOULDER, P7PAD_RIGHT_SHOULDER, P7PAD_BACK, P7PAD_START,
		P7PAD_LEFT_THUMB, P7PAD_RIGHT_THUMB, P7PAD_DPAD_UP, P7PAD_DPAD_DOWN, P7PAD_DPAD_LEFT, P7PAD_DPAD_RIGHT
	};

	DWORD ObjectType(int i)
	{
		return (s_Objects[i].m_nKind == P7_AXIS ? DIDFT_ABSAXIS : DIDFT_PSHBUTTON) |
		       DIDFT_MAKEINSTANCE(s_Objects[i].m_nInstance);
	}

	// ------------------------------------------------------------------ //
	// The pad's state as the objects' values.

	BOOL ReadPad(P7PadState* pPad)
	{
		return P7PadRead_State(pPad) ? TRUE : FALSE;
	}

	LONG ScaleThumb(SHORT n, BOOL bFlip)
	{
		LONG v = ((LONG)n * 1000) / 32767;

		if (bFlip)
			v = -v;

		return v < -1000 ? -1000 : v > 1000 ? 1000 : v;
	}

	// Every object's value from the pad's state, in s_Objects' order.
	void PadValues(const P7PadState& pad, LONG* pValues)
	{
		int i;

		pValues[0] = ScaleThumb(pad.sThumbLX, FALSE);
		pValues[1] = ScaleThumb(pad.sThumbLY, TRUE);
		pValues[2] = ((LONG)pad.bLeftTrigger * 1000) / 255;
		pValues[3] = ScaleThumb(pad.sThumbRX, FALSE);
		pValues[4] = ScaleThumb(pad.sThumbRY, TRUE);
		pValues[5] = ((LONG)pad.bRightTrigger * 1000) / 255;

		for (i = 0; i < P7_BUTTONS - 2; i++)
			pValues[P7_AXES + i] = (pad.wButtons & s_ButtonBits[i]) ? 0x80 : 0;

		pValues[P7_AXES + 14] = pad.bLeftTrigger > P7PAD_TRIGGER_THRESHOLD ? 0x80 : 0;
		pValues[P7_AXES + 15] = pad.bRightTrigger > P7PAD_TRIGGER_THRESHOLD ? 0x80 : 0;
	}

	// The object a property header names, or -1.
	int FindObject(LPCDIPROPHEADER pHeader, const int* pFormatOfs)
	{
		int i;

		for (i = 0; i < P7_OBJECTS; i++)
		{
			if (pHeader->dwHow == DIPH_BYID && pHeader->dwObj == ObjectType(i))
				return i;

			if (pHeader->dwHow == DIPH_BYOFFSET && pFormatOfs[i] >= 0 &&
			    pHeader->dwObj == (DWORD)pFormatOfs[i])
				return i;
		}

		return -1;
	}

	// Is the window, or one it belongs to, the foreground one?
	BOOL IsForeground(HWND hWnd)
	{
		HWND h;

		if (!hWnd)
			return TRUE;

		for (h = GetForegroundWindow(); h; h = GetParent(h))
		{
			if (h == hWnd)
				return TRUE;
		}

		return FALSE;
	}

	// ------------------------------------------------------------------ //
	// The device.

	class CP7PadDevice : public IDirectInputDevice2A
	{
	public:
		CP7PadDevice()
		{
			int i;

			m_nRef       = 1;
			m_hWnd       = NULL;
			m_dwCoop     = 0;
			m_bAcquired  = FALSE;
			m_bLost      = FALSE;
			m_bFormat    = FALSE;
			m_dwBufSize  = 0;
			m_nBuf       = 0;
			m_bOverflow  = FALSE;
			m_dwSequence = 0;

			for (i = 0; i < P7_OBJECTS; i++)
			{
				m_FormatOfs[i] = -1;
				m_Value[i]     = 0;
				m_Range[i][0]  = s_Objects[i].m_nMin;
				m_Range[i][1]  = s_Objects[i].m_nMax;
			}
		}

		// IUnknown
		STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppv)
		{
			if (IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IDirectInputDeviceA) ||
			    IsEqualGUID(riid, IID_IDirectInputDevice2A))
			{
				*ppv = (IDirectInputDevice2A*)this;
				AddRef();
				return S_OK;
			}

			*ppv = NULL;
			return E_NOINTERFACE;
		}

		STDMETHODIMP_(ULONG) AddRef()
		{
			return ++m_nRef;
		}

		STDMETHODIMP_(ULONG) Release()
		{
			ULONG n = --m_nRef;

			if (n == 0)
				delete this;

			return n;
		}

		// IDirectInputDeviceA
		STDMETHODIMP GetCapabilities(LPDIDEVCAPS pCaps)
		{
			P7PadState pad;
			DIDEVICEINSTANCEA inst;

			if (!pCaps || pCaps->dwSize < sizeof(DIDEVCAPS))
				return DIERR_INVALIDPARAM;

			P7Pad_GetInstance(&inst);
			memset(pCaps, 0, sizeof(DIDEVCAPS));
			pCaps->dwSize     = sizeof(DIDEVCAPS);
			pCaps->dwFlags    = (ReadPad(&pad) ? DIDC_ATTACHED : 0) | DIDC_POLLEDDATAFORMAT;
			pCaps->dwDevType  = inst.dwDevType;
			pCaps->dwAxes     = P7_AXES;
			pCaps->dwButtons  = P7_BUTTONS;
			pCaps->dwPOVs     = 0;
			return DI_OK;
		}

		STDMETHODIMP EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA pCallback, LPVOID pvRef, DWORD dwFlags)
		{
			DIDEVICEOBJECTINSTANCEA doi;
			int i;

			if (!pCallback)
				return DIERR_INVALIDPARAM;

			for (i = 0; i < P7_OBJECTS; i++)
			{
				if (dwFlags != DIDFT_ALL && !(DIDFT_GETTYPE(dwFlags) & DIDFT_GETTYPE(ObjectType(i))))
					continue;

				GetObject(i, &doi);

				if (pCallback(&doi, pvRef) == DIENUM_STOP)
					break;
			}

			return DI_OK;
		}

		STDMETHODIMP GetProperty(REFGUID rguidProp, LPDIPROPHEADER pHeader)
		{
			int i;

			if (!pHeader)
				return DIERR_INVALIDPARAM;

			if (&rguidProp == &DIPROP_BUFFERSIZE)
			{
				((LPDIPROPDWORD)pHeader)->dwData = m_dwBufSize;
				return DI_OK;
			}

			if (&rguidProp == &DIPROP_RANGE)
			{
				i = FindObject(pHeader, m_FormatOfs);

				if (i < 0 || s_Objects[i].m_nKind != P7_AXIS)
					return DIERR_OBJECTNOTFOUND;

				((LPDIPROPRANGE)pHeader)->lMin = m_Range[i][0];
				((LPDIPROPRANGE)pHeader)->lMax = m_Range[i][1];
				return DI_OK;
			}

			return DIERR_UNSUPPORTED;
		}

		STDMETHODIMP SetProperty(REFGUID rguidProp, LPCDIPROPHEADER pHeader)
		{
			int i;

			if (!pHeader)
				return DIERR_INVALIDPARAM;

			if (&rguidProp == &DIPROP_BUFFERSIZE)
			{
				if (m_bAcquired)
					return DIERR_ACQUIRED;

				m_dwBufSize = ((LPCDIPROPDWORD)pHeader)->dwData;

				if (m_dwBufSize > P7_BUFFER)
					m_dwBufSize = P7_BUFFER;

				m_nBuf = 0;
				return DI_OK;
			}

			if (&rguidProp == &DIPROP_RANGE)
			{
				LONG nMin = ((LPCDIPROPRANGE)pHeader)->lMin;
				LONG nMax = ((LPCDIPROPRANGE)pHeader)->lMax;

				for (i = 0; i < P7_OBJECTS; i++)
				{
					if (s_Objects[i].m_nKind != P7_AXIS)
						continue;

					if (pHeader->dwHow == DIPH_DEVICE || FindObject(pHeader, m_FormatOfs) == i)
					{
						m_Range[i][0] = nMin;
						m_Range[i][1] = nMax;
					}
				}

				return DI_OK;
			}

			// Dead zone, saturation, ...: the game applies its own.
			return DI_PROPNOEFFECT;
		}

		STDMETHODIMP Acquire()
		{
			P7PadState pad;

			if (!m_bFormat)
				return DIERR_INVALIDPARAM;

			if ((m_dwCoop & DISCL_FOREGROUND) && !IsForeground(m_hWnd))
				return DIERR_OTHERAPPHASPRIO;

			if (!m_bAcquired)
			{
				// What is held now is the state the buffer's changes start
				// from, as DirectInput's.
				ReadPad(&pad);
				PadValues(pad, m_Value);

				m_bAcquired = TRUE;
				m_bLost     = FALSE;
				m_nBuf      = 0;
				m_bOverflow = FALSE;
			}

			return DI_OK;
		}

		STDMETHODIMP Unacquire()
		{
			if (!m_bAcquired)
				return DI_NOEFFECT;

			m_bAcquired = FALSE;
			m_nBuf      = 0;
			m_bOverflow = FALSE;
			return DI_OK;
		}

		STDMETHODIMP GetDeviceState(DWORD cbData, LPVOID pvData)
		{
			int i;

			if (!pvData)
				return DIERR_INVALIDPARAM;

			if (!Update())
				return m_bLost ? DIERR_INPUTLOST : DIERR_NOTACQUIRED;

			memset(pvData, 0, cbData);

			for (i = 0; i < P7_OBJECTS; i++)
			{
				DWORD dwOfs = (DWORD)m_FormatOfs[i];

				if (m_FormatOfs[i] < 0)
					continue;

				if (s_Objects[i].m_nKind == P7_AXIS)
				{
					if (dwOfs + sizeof(LONG) <= cbData)
						*(LONG*)((BYTE*)pvData + dwOfs) = Scaled(i, m_Value[i]);
				}
				else if (dwOfs < cbData)
				{
					*((BYTE*)pvData + dwOfs) = (BYTE)m_Value[i];
				}
			}

			return DI_OK;
		}

		STDMETHODIMP GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut,
		                           DWORD dwFlags)
		{
			DWORD   n;
			DWORD   i;
			HRESULT hr;

			if (!pdwInOut)
				return DIERR_INVALIDPARAM;

			if (!Update())
				return m_bLost ? DIERR_INPUTLOST : DIERR_NOTACQUIRED;

			hr = m_bOverflow ? DI_BUFFEROVERFLOW : DI_OK;

			// A null array and INFINITE: throw the buffer away.
			if (!rgdod && *pdwInOut == INFINITE)
			{
				*pdwInOut   = m_nBuf;
				m_nBuf      = 0;
				m_bOverflow = FALSE;
				return hr;
			}

			n = *pdwInOut < m_nBuf ? *pdwInOut : m_nBuf;

			if (rgdod)
			{
				for (i = 0; i < n; i++)
					memcpy((BYTE*)rgdod + i * cbObjectData, &m_Buf[i],
					       cbObjectData < sizeof(DIDEVICEOBJECTDATA) ? cbObjectData : sizeof(DIDEVICEOBJECTDATA));
			}

			if (!(dwFlags & DIGDD_PEEK))
			{
				memmove(m_Buf, m_Buf + n, (m_nBuf - n) * sizeof(DIDEVICEOBJECTDATA));
				m_nBuf     -= n;
				m_bOverflow = FALSE;
			}

			*pdwInOut = n;
			return hr;
		}

		STDMETHODIMP SetDataFormat(LPCDIDATAFORMAT pFormat)
		{
			BOOL  bUsed[P7_OBJECTS];
			DWORD i;
			int   j;

			if (!pFormat)
				return DIERR_INVALIDPARAM;

			if (m_bAcquired)
				return DIERR_ACQUIRED;

			for (j = 0; j < P7_OBJECTS; j++)
			{
				m_FormatOfs[j] = -1;
				bUsed[j]       = FALSE;
			}

			// Each entry takes the first object not yet taken that it
			// describes: by type, instance (or any) and GUID (or any).
			for (i = 0; i < pFormat->dwNumObjs; i++)
			{
				const DIOBJECTDATAFORMAT* pEntry =
					(const DIOBJECTDATAFORMAT*)((const BYTE*)pFormat->rgodf + i * pFormat->dwObjSize);
				DWORD dwType = pEntry->dwType;

				for (j = 0; j < P7_OBJECTS; j++)
				{
					if (bUsed[j])
						continue;

					if (DIDFT_GETTYPE(dwType) && !(DIDFT_GETTYPE(dwType) & DIDFT_GETTYPE(ObjectType(j))))
						continue;

					if ((dwType & DIDFT_INSTANCEMASK) != DIDFT_ANYINSTANCE &&
					    DIDFT_GETINSTANCE(dwType) != s_Objects[j].m_nInstance)
						continue;

					if (pEntry->pguid && !IsEqualGUID(*pEntry->pguid, *s_Objects[j].m_pGuid))
						continue;

					bUsed[j]       = TRUE;
					m_FormatOfs[j] = (int)pEntry->dwOfs;
					break;
				}
			}

			m_bFormat = TRUE;
			m_nBuf    = 0;
			return DI_OK;
		}

		STDMETHODIMP SetEventNotification(HANDLE)
		{
			return DI_OK;
		}

		STDMETHODIMP SetCooperativeLevel(HWND hWnd, DWORD dwFlags)
		{
			m_hWnd   = hWnd;
			m_dwCoop = dwFlags;
			return DI_OK;
		}

		STDMETHODIMP GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA pdoi, DWORD dwObj, DWORD dwHow)
		{
			DIPROPHEADER header;
			int i;

			if (!pdoi)
				return DIERR_INVALIDPARAM;

			header.dwObj = dwObj;
			header.dwHow = dwHow;
			i = FindObject(&header, m_FormatOfs);

			if (i < 0)
				return DIERR_OBJECTNOTFOUND;

			GetObject(i, pdoi);
			return DI_OK;
		}

		STDMETHODIMP GetDeviceInfo(LPDIDEVICEINSTANCEA pdidi)
		{
			if (!pdidi)
				return DIERR_INVALIDPARAM;

			P7Pad_GetInstance(pdidi);
			return DI_OK;
		}

		STDMETHODIMP RunControlPanel(HWND, DWORD)
		{
			return DI_OK;
		}

		STDMETHODIMP Initialize(HINSTANCE, DWORD, REFGUID)
		{
			return DI_OK;
		}

		// IDirectInputDevice2A: no force feedback (rumble is XInput's, later).
		STDMETHODIMP CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT* ppEffect, LPUNKNOWN)
		{
			if (ppEffect)
				*ppEffect = NULL;

			return DIERR_UNSUPPORTED;
		}

		STDMETHODIMP EnumEffects(LPDIENUMEFFECTSCALLBACKA, LPVOID, DWORD)
		{
			return DI_OK;
		}

		STDMETHODIMP GetEffectInfo(LPDIEFFECTINFOA, REFGUID)
		{
			return DIERR_UNSUPPORTED;
		}

		STDMETHODIMP GetForceFeedbackState(LPDWORD)
		{
			return DIERR_UNSUPPORTED;
		}

		STDMETHODIMP SendForceFeedbackCommand(DWORD)
		{
			return DIERR_UNSUPPORTED;
		}

		STDMETHODIMP EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK, LPVOID, DWORD)
		{
			return DI_OK;
		}

		STDMETHODIMP Escape(LPDIEFFESCAPE)
		{
			return DIERR_UNSUPPORTED;
		}

		STDMETHODIMP Poll()
		{
			if (!Update())
				return m_bLost ? DIERR_INPUTLOST : DIERR_NOTACQUIRED;

			return DI_OK;
		}

		STDMETHODIMP SendDeviceData(DWORD, LPCDIDEVICEOBJECTDATA, LPDWORD, DWORD)
		{
			return DIERR_UNSUPPORTED;
		}

	private:
		~CP7PadDevice()
		{
		}

		void GetObject(int i, LPDIDEVICEOBJECTINSTANCEA pdoi)
		{
			memset(pdoi, 0, sizeof(DIDEVICEOBJECTINSTANCEA));
			pdoi->dwSize   = sizeof(DIDEVICEOBJECTINSTANCEA);
			pdoi->guidType = *s_Objects[i].m_pGuid;
			pdoi->dwOfs    = m_FormatOfs[i] >= 0 ? (DWORD)m_FormatOfs[i] : s_Objects[i].m_dwStdOfs;
			pdoi->dwType   = ObjectType(i);
			pdoi->dwFlags  = s_Objects[i].m_nKind == P7_AXIS ? DIDOI_ASPECTPOSITION : 0;
			strncpy(pdoi->tszName, s_Objects[i].m_pName, sizeof(pdoi->tszName) - 1);
		}

		// An axis value in the range the application set (the native one
		// unless it set another).
		LONG Scaled(int i, LONG nValue)
		{
			LONG nMin = s_Objects[i].m_nMin, nMax = s_Objects[i].m_nMax;

			if (m_Range[i][0] == nMin && m_Range[i][1] == nMax)
				return nValue;

			return m_Range[i][0] +
			       (LONG)(((double)(nValue - nMin) * (m_Range[i][1] - m_Range[i][0])) / (nMax - nMin));
		}

		// Read the pad and buffer what changed; FALSE when not acquired.
		BOOL Update()
		{
			P7PadState pad;
			LONG       values[P7_OBJECTS];
			DWORD      dwNow;
			int        i;

			if (!m_bAcquired)
				return FALSE;

			if ((m_dwCoop & DISCL_FOREGROUND) && !IsForeground(m_hWnd))
			{
				m_bAcquired = FALSE;
				m_bLost     = TRUE;
				m_nBuf      = 0;
				return FALSE;
			}

			ReadPad(&pad);
			PadValues(pad, values);
			dwNow = GetTickCount();

			for (i = 0; i < P7_OBJECTS; i++)
			{
				if (values[i] == m_Value[i])
					continue;

				m_Value[i] = values[i];

				if (m_FormatOfs[i] < 0 || !m_dwBufSize)
					continue;

				if (m_nBuf >= m_dwBufSize)
				{
					m_bOverflow = TRUE;
					continue;
				}

				m_Buf[m_nBuf].dwOfs       = (DWORD)m_FormatOfs[i];
				m_Buf[m_nBuf].dwData      = s_Objects[i].m_nKind == P7_AXIS ? (DWORD)Scaled(i, values[i])
				                                                            : (DWORD)values[i];
				m_Buf[m_nBuf].dwTimeStamp = dwNow;
				m_Buf[m_nBuf].dwSequence  = ++m_dwSequence;
				m_nBuf++;
			}

			return TRUE;
		}

		ULONG				m_nRef;
		HWND				m_hWnd;
		DWORD				m_dwCoop;
		BOOL				m_bAcquired;
		BOOL				m_bLost;
		BOOL				m_bFormat;
		int					m_FormatOfs[P7_OBJECTS];	// -1: not in the data format
		LONG				m_Value[P7_OBJECTS];		// as last buffered, native range
		LONG				m_Range[P7_OBJECTS][2];
		DWORD				m_dwBufSize;
		DIDEVICEOBJECTDATA	m_Buf[P7_BUFFER];
		DWORD				m_nBuf;
		BOOL				m_bOverflow;
		DWORD				m_dwSequence;
	};
}

BOOL P7Pad_Available(void)
{
	return P7PadRead_Available() ? TRUE : FALSE;
}

void P7Pad_GetInstance(DIDEVICEINSTANCEA* pInstance)
{
	memset(pInstance, 0, sizeof(DIDEVICEINSTANCEA));
	pInstance->dwSize       = sizeof(DIDEVICEINSTANCEA);
	pInstance->guidInstance = c_guidPad;
	pInstance->guidProduct  = c_guidPad;
	pInstance->dwDevType    = DIDEVTYPE_JOYSTICK | (DIDEVTYPEJOYSTICK_GAMEPAD << 8) | DIDEVTYPE_HID;
	strcpy(pInstance->tszInstanceName, "##gamepad");
	strcpy(pInstance->tszProductName, "XInput Gamepad");
}

BOOL P7Pad_IsPad(REFGUID guidInstance)
{
	return IsEqualGUID(guidInstance, c_guidPad);
}

HRESULT P7Pad_CreateDevice(LPDIRECTINPUTA pDI, REFGUID guid, LPDIRECTINPUTDEVICEA* ppDevice)
{
	if (P7Pad_IsPad(guid))
	{
		*ppDevice = (LPDIRECTINPUTDEVICEA)(IDirectInputDevice2A*)new CP7PadDevice;
		return *ppDevice ? DI_OK : DIERR_OUTOFMEMORY;
	}

	return pDI->CreateDevice(guid, ppDevice, NULL);
}
