//{{NO_DEPENDENCIES}}
// resource.h -- NolfServ.exe's resource IDs (nolfserv.rc).
//
// The numbers are the retail NolfServ.exe's. The names are Monolith's where
// a later server of theirs reuses the number for the same thing (AvP2's
// AVP2Serv, a descendant of this one, kept most of them); the rest are ours.
//

// String table
#define IDS_NETERR_GENERIC              1
#define IDS_NETERR_SELECTSERVICE        2
#define IDS_NETERR_FILLSERVICE          3
#define IDS_NETERR_JOINSESSION          4
#define IDS_NETERR_HOSTSESSION          5
#define IDS_NETERR_INIT                 6
#define IDS_NETERR_SETNETSERVICE        7
#define IDS_APPREGNAME                  8
#define IDS_APPNAME                     9
#define IDS_ERROR_LOADREZ               103
#define IDS_ERROR_MEMORY                106
#define IDS_ERROR_BINARIES              107
#define IDS_ERROR_CANTSTARTSERVER       109
#define IDS_CHECKEXIT                   121
#define IDS_CONSOLE_INITSERVER          122
#define IDS_CONSOLE_SERVERINITED        123
#define IDS_CONSOLE_UNABLETOHOST        124
#define IDS_CONSOLE_SERVERRUNNING       125
#define IDS_CONSOLE_SERVERSTOPPED       143
#define IDS_GOODVSEVIL                  145
#define IDS_ERROR_WORLDS                146
#define IDS_COLUMNHEADER_NAME           147
#define IDS_COLUMNHEADER_PING           148
#define IDS_COLUMNHEADER_KILLS          149
#define IDS_COLUMNHEADER_WORLD          151
#define IDS_GAMETYPE_ASSAULT            152
#define IDS_GAMETYPE_DEATHMATCH         153
#define IDS_FILTER_RETAIL               154
#define IDS_FILTER_CUSTOM               155
#define IDS_NONE                        156
#define IDS_COLUMNHEADER_TIME           157
#define IDS_WIZARD_TITLE                160
#define IDS_ERROR_CANTSAVEOPTIONS       161
#define IDS_NEWPLAYER                   162
#define IDS_BANDWIDTH_56K               163
#define IDS_BANDWIDTH_CABLE             164
#define IDS_BANDWIDTH_DSL               165
#define IDS_BANDWIDTH_T1                166
#define IDS_BANDWIDTH_T3                167

// Icons and bitmaps
#define IDR_MAINFRAME                   128
#define IDI_SELECTED                    133
#define IDB_WIZARD                      139

// Dialogs
#define IDD_GAMESERVER                  129
#define IDD_SESSION                     135
#define IDD_WORLDS                      136
#define IDD_OPTIONS_DEATHMATCH          137
#define IDD_OPTIONS_ASSAULT             138
#define IDD_OPTIONS                     139
#define IDD_CUSTOM                      141

// The server dialog (IDD_GAMESERVER)
#define IDC_SERVER_NAME                 1003
#define IDC_PLAYER_BOOT                 1016
#define IDC_CONSOLE_COMMAND             1018
#define IDC_CONSOLE_WINDOW              1021
#define IDC_SERVER_TIME                 1037
#define IDC_GAME_PLAYERS                1039
#define IDC_CONSOLE_CLEAR               1044
#define IDC_CONSOLE_SEND                1045
#define IDC_COMMANDS_NEXTLEVEL          1046
#define IDC_TIMEINLEVEL                 1047
#define IDC_COMMANDS_SELLEVEL           1048
#define IDC_STOPSERVER                  1049
#define IDC_LEVELS                      1072
#define IDC_PLAYERS                     1083
#define IDC_PEAKPLAYERS                 1129
#define IDC_AVERAGEPING                 1130
#define IDC_TOTALPLAYERS                1131

// The session page (IDD_SESSION)
#define IDC_MAXPLAYERS                  1044
#define IDC_SESSIONNAME                 1075
#define IDC_PASSWORD                    1076
#define IDC_PORT                        1077
#define IDC_BANDWIDTH_CUSTOM            1079
#define IDC_GAMETYPE                    1088
#define IDC_BANDWIDTH                   1090
#define IDC_USEPASSWORD                 1104
#define IDC_MAXPLAYERS_SLIDER           1128

// The worlds page (IDD_WORLDS)
#define IDC_REMOVE                      1034
#define IDC_ADD                         1052
#define IDC_ADDALL                      1053
#define IDC_REMOVEALL                   1054
#define IDC_SELECTEDWORLDS              1078
#define IDC_AVAILABLEWORLDS             1079
#define IDC_LEVELFILTER                 1116

// The option pages (IDD_OPTIONS_DEATHMATCH, _ASSAULT, IDD_OPTIONS)
#define IDC_ENDCONDITION                1107
#define IDC_FRAGLIMIT                   1108
#define IDC_TIMELIMIT                   1109
#define IDC_RUNSPEED                    1110
#define IDC_RESPAWNSCALE                1111
#define IDC_DEFAULTWEAPON               1112
#define IDC_WEAPONSSTAY                 1113
#define IDC_HITLOCATION                 1114
#define IDC_AUDIOTAUNTS                 1115
#define IDC_FALLDAMAGE                  1116
#define IDC_ARMORHEALTH                 1117
#define IDC_WINNINGSCORE                1118
#define IDC_INTELSCORE                  1119
#define IDC_FRIENDLYFIRE                1121
#define IDC_FRAGSCORING                 1122
#define IDC_SLIDER1                     1124
#define IDC_SLIDER2                     1125
#define IDC_SLIDER3                     1126
#define IDC_SLIDER4                     1127

// The customization page (IDD_CUSTOM): the launcher's numbers, which its
// dlgcustom.cpp, compiled in here too, uses
#define IDC_CUSTOM_AVAILABLE            1039
#define IDC_CUSTOM_SELECTED             1040
#define IDC_CUSTOM_ALWAYS               1043

// Next default values for new objects
//
#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        142
#define _APS_NEXT_COMMAND_VALUE         32771
#define _APS_NEXT_CONTROL_VALUE         1132
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
