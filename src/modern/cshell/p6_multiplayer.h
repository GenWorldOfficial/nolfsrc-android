// ----------------------------------------------------------------------- //
//
// MODULE  : p6_multiplayer.h
//
// PURPOSE : The modern build's multiplayer changes on the client (package
//           P6, MODERN.md): the master server setting for the browser,
//           untrusted network text, the rejoin after a level-change timeout,
//           the join log and the join password.
//
// ----------------------------------------------------------------------- //

#ifndef __P6_MULTIPLAYER_H__
#define __P6_MULTIPLAYER_H__

#include "ltbasedefs.h"
#include "cres/p6.h"

// M-P6-02: point the server browser at the master named by the console
// variable MasterServer (host or host:port; default master.openspy.net;
// "none" for no master). Call before CGameSpyClientMgr::Init.
void	P6_SetBrowserMaster();

// M-P6-01: a name received from the server, with any format characters
// removed. Returns hStr itself when it is clean; otherwise frees hStr and
// returns a filtered copy.
HSTRING	P6_FilterNetString(HSTRING hStr);

// M-P6-05: called on a multiplayer disconnect with the server's address,
// before it is cleared. If the disconnect was a timeout and retries are
// left, schedules a rejoin of the same server and returns LTTRUE: the
// caller then skips the usual disconnect handling (message box, menus).
LTBOOL	P6_RejoinAfterTimeout(const char* sAddress, int nPort);

// M-P6-05: a world was entered; the retries are available again.
void	P6_ResetRejoin();

// M-P6-05: once per client update: performs a scheduled rejoin.
void	P6_UpdateRejoin();

// M-P6-06: console lines for each join attempt and its outcome.
void	P6_LogJoinAttempt(const char* sAddress, int nAttempt, int nAttempts);
void	P6_LogJoinResult(const char* sAddress, LTBOOL bJoined);

// M-P6-10: the password for the server being joined (what the player typed
// in the join folders; empty for none), sent in the handshake.
void		P6_SetJoinPassword(const char* sPassword);
const char*	P6_GetJoinPassword();

#endif // __P6_MULTIPLAYER_H__
