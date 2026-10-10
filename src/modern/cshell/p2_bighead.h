// ----------------------------------------------------------------------- //
//
// MODULE  : p2_bighead.h
//
// PURPOSE : M-P2-10, big head mode -- the client half (NOLFSRC_MODERN only)
//
// ----------------------------------------------------------------------- //
//
// "BigHeadMode <scale>" draws every character's head (model node
// "head_node") 1.8 x <scale> times its size: living characters through
// their node controller (NodeController.cpp), dead bodies through a node
// control of their own (BodyFX.cpp). 0 is off, and it is always off in
// multiplayer, whatever the variable says. The server half (head
// attachments, head hit radius) is object/p2_server.cpp.

#ifndef __P2_BIGHEAD_H__
#define __P2_BIGHEAD_H__

void   P2BigHead_Init();

// The head scale factor to apply now, or 0 for none.
LTFLOAT P2BigHead_Factor();

// From a node control callback: scale pGlobalMat if hNode is hObj's head.
void   P2BigHead_ScaleNode(HOBJECT hObj, HMODELNODE hNode, LTMatrix* pGlobalMat);

// CBodyFX::Update: install or remove the body's node control as big head
// mode goes on or off. *pbInstalled is the body's own record of it.
void   P2BigHead_UpdateBody(HOBJECT hBody, LTBOOL* pbInstalled);

// Single player or host: send the client's BigHeadMode to the server.
void   P2BigHead_SendToServer();

#endif // __P2_BIGHEAD_H__
