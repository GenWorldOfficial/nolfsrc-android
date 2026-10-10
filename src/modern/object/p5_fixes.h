// ----------------------------------------------------------------------- //
//
// MODULE  : p5_fixes.h
//
// PURPOSE : The modern build's AI and object fixes in Object.lto
//           (package P5 of MODERN.md). Compiled only with NOLFSRC_MODERN.
//
// ----------------------------------------------------------------------- //

#ifndef __P5_FIXES_H__
#define __P5_FIXES_H__

// M-P5-01: GetMovementData(afData, cData, ...) without reading past the
// table: afData holds cData+1 samples; fTime and fTimePrev are fractions of
// the animation, clamped to 0..1 (and 0 when not a number).
LTFLOAT P5_GetMovementData(const float* afData, uint32 cData, float fTimePrev, float fTime);

// M-P5-03, M-P5-04: normalise v unless it has zero length (or is not a
// number), which VEC_NORM would turn into NaNs. Returns LTFALSE, leaving
// v as it was, when it cannot be normalised.
LTBOOL P5_NormalizeIfNonZero(LTVector& v);

// M-P5-03: TRUE if any component of v is not a number.
LTBOOL P5_IsNaN(const LTVector& v);

// M-P5-03: repair a door's movement vectors read from a save written by
// a build without M-P5-03 (a zero MoveDir normalised to NaNs, and the open
// position computed from it). Returns LTTRUE if anything was repaired.
LTBOOL P5_RepairLoadedDoor(LTVector& vMoveDir, LTVector& vOpenPos, const LTVector& vClosedPos, LTFLOAT fMoveDist);

#endif // __P5_FIXES_H__
