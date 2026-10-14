// ----------------------------------------------------------------------- //
//
// MODULE  : p1_frametime.h
//
// PURPOSE : Package P1 (MODERN.md), M-P1-23: a frame's time in whole
//           milliseconds, as the engine's animation trackers take it,
//           without losing the fraction.  The engine truncates
//           (int)(frame time * 1000) every frame and drops the rest: up to
//           a millisecond a frame, which at 300 frames a second (3.3 ms
//           each) slowed animations by about a sixth.  Here the fraction
//           is carried into the next frame.
//
//           Modern build only (NOLFSRC_MODERN).
//
// ----------------------------------------------------------------------- //

#ifndef __P1_FRAMETIME_H__
#define __P1_FRAMETIME_H__

// Once a frame: the whole milliseconds, the fraction kept in *pCarry.
inline unsigned long P1_FrameMs(float fFrameTime, float* pCarry)
{
	float fMs = fFrameTime * 1000.0f + *pCarry;
	unsigned long nMs;

	if (fMs < 0.0f)
		fMs = 0.0f;

	nMs = (unsigned long)fMs;
	*pCarry = fMs - (float)nMs;
	return nMs;
}

// The same for code that runs once per object each tick: the tick is
// told apart by its time stamp, and every object of a tick gets the same
// milliseconds.
struct P1TickMs
{
	float			m_fTime;
	float			m_fCarry;
	unsigned long	m_nMs;
};

inline unsigned long P1_TickMs(P1TickMs* pState, float fFrameTime, float fTime)
{
	if (fTime != pState->m_fTime)
	{
		pState->m_fTime = fTime;
		pState->m_nMs = P1_FrameMs(fFrameTime, &pState->m_fCarry);
	}

	return pState->m_nMs;
}

#endif // __P1_FRAMETIME_H__
