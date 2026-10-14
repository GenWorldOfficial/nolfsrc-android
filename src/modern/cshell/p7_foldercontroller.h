// p7_foldercontroller.h -- the Controller options folder (MODERN.md,
// M-P7-06). Modern build only: reached from Controls.

#ifndef P7_FOLDERCONTROLLER_H
#define P7_FOLDERCONTROLLER_H

#include "BaseFolder.h"

class CFolderController : public CBaseFolder
{
public:
	CFolderController();
	virtual ~CFolderController();

	// Build the folder
    LTBOOL  Build();
    void    OnFocus(LTBOOL bFocus);
    uint32  OnCommand(uint32 dwCommand, uint32 dwParam1, uint32 dwParam2);

	void	ConfirmRestore(LTBOOL bYes);

protected:
	int		m_nLookSpeed;		// PadLookSpeed, in percent
	LTBOOL	m_bInvertLook;		// PadInvertY
	int		m_nLookDeadZone;	// PadLookDeadZone, in percent
	int		m_nMoveDeadZone;	// PadMoveDeadZone, in percent
};

#endif // P7_FOLDERCONTROLLER_H
