// ----------------------------------------------------------------------- //
//
// MODULE  : p5_movement.h
//
// PURPOSE : M-P5-05 (MODERN.md): keep the player "standing on" the world
//           across the odd frame where the engine reports nothing under
//           him. Compiled only with NOLFSRC_MODERN.
//
// ----------------------------------------------------------------------- //

#ifndef __P5_MOVEMENT_H__
#define __P5_MOVEMENT_H__

// The last surface CMoveMgr::UpdateOnGround saw under the player.
struct P5StandingOn
{
	CollisionInfo	m_Info;		// what GetStandingOn returned then
	LTBOOL			m_bValid;	// m_Info is the main world
	LTFLOAT			m_fY;		// the player's height then
	int				m_nFrames;	// frames carried since
};

void P5_ResetStandingOn(P5StandingOn & s);

// Called with what GetStandingOn returned this frame. When nothing is
// under the player, but the world was a moment ago and he has neither
// jumped, started falling nor dropped, Info gets the world back for up to
// a few frames: the engine loses contact over seams between floor
// polygons, and the player was pushed back as if by an invisible wall.
void P5_CarryStandingOn(P5StandingOn & s, CollisionInfo & Info, const LTVector & vPos, LTBOOL bAirborne);

#endif // __P5_MOVEMENT_H__
