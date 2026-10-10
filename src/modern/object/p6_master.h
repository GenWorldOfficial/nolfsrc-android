// ----------------------------------------------------------------------- //
//
// MODULE  : p6_master.h
//
// PURPOSE : M-P6-02 on the server: which master server a multiplayer
//           server reports to (package P6, MODERN.md).
//
// ----------------------------------------------------------------------- //

#ifndef __P6_MASTER_H__
#define __P6_MASTER_H__

class CGameSpyMgr;

// Gives the GameSpy manager the master from the console variable
// MasterServer (host or host:port; empty for master.openspy.net; "none"
// for no master). ReadNetHostSettings sets the variable from NetHost.txt.
void P6_SetServerMaster(CGameSpyMgr* pGameSpyMgr);

#endif // __P6_MASTER_H__
