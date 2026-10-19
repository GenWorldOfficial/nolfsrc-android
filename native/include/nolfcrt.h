/* nolfcrt.h -- the Microsoft C runtime's own names, for the native build.
 *
 * On Windows stdlib.h, string.h and io.h declare these (stricmp, _MAX_PATH,
 * _splitpath, _findfirst, ...), so code uses them without including
 * windows.h. The native build includes this file first in every compile
 * (-include), and nolfwin.h includes it too. The functions are in
 * native/plat/nolfplat_crt.cpp.
 */

#ifndef NOLFCRT_H
#define NOLFCRT_H

#include <alloca.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef __cplusplus
#define NOLFCRT_EXTERN extern "C"
#else
#define NOLFCRT_EXTERN extern
#endif

#define _MAX_PATH       260
#define _MAX_DRIVE      3
#define _MAX_DIR        256
#define _MAX_FNAME      256
#define _MAX_EXT        256

/* ---- the C runtime's Microsoft names ------------------------------------------------- */

#define stricmp         strcasecmp
#define _stricmp        strcasecmp
#define strcmpi         strcasecmp
#define _strcmpi        strcasecmp
#define strnicmp        strncasecmp
#define _strnicmp       strncasecmp
#define _strdup         strdup
#define _snprintf       nolf_snprintf
#define _vsnprintf      nolf_vsnprintf
#define _mbscmp(a, b)   strcmp((const char*)(a), (const char*)(b))
#define _mbsicmp(a, b)  strcasecmp((const char*)(a), (const char*)(b))
#define _tcsinc(p)      ((p) + 1)
#define _tcsninc(p, n)  ((p) + (n))
#define _tcsdec(s, p)   ((p) - 1)
#define _tcslen         strlen
#define _tcscpy         strcpy
#define _tcscmp         strcmp
#define _tcsicmp        strcasecmp
#define _T(x)           x
#define _TEXT(x)        x
#define TEXT(x)         x
#define _access         access
#define _unlink         unlink
#define _getcwd         getcwd
#define _chdir          chdir
#define _rmdir          rmdir
#define _fileno         fileno
#define _stat           stat
#define _fstat          fstat
#define _isnan          isnan
#define _finite         isfinite
#define _hypot          hypot
#define _alloca         alloca
#define __min(a, b)     (((a) < (b)) ? (a) : (b))
#define __max(a, b)     (((a) > (b)) ? (a) : (b))
#define _S_IFDIR        S_IFDIR
#define _S_IFREG        S_IFREG
#define _S_IREAD        S_IRUSR
#define _S_IWRITE       S_IWUSR
#define _O_BINARY       0
#define O_BINARY        0
#define _O_TEXT         0

NOLFCRT_EXTERN int   nolf_snprintf(char* pBuf, size_t nSize, const char* pFormat, ...);
NOLFCRT_EXTERN int   nolf_vsnprintf(char* pBuf, size_t nSize, const char* pFormat, va_list args);
NOLFCRT_EXTERN char* strupr(char* p);
NOLFCRT_EXTERN char* strlwr(char* p);
NOLFCRT_EXTERN char* _strupr(char* p);
NOLFCRT_EXTERN char* _strlwr(char* p);
NOLFCRT_EXTERN char* itoa(int nValue, char* pBuf, int nRadix);
NOLFCRT_EXTERN char* _itoa(int nValue, char* pBuf, int nRadix);
NOLFCRT_EXTERN char* ltoa(long nValue, char* pBuf, int nRadix);
NOLFCRT_EXTERN char* _ltoa(long nValue, char* pBuf, int nRadix);
NOLFCRT_EXTERN char* ultoa(unsigned long nValue, char* pBuf, int nRadix);
NOLFCRT_EXTERN char* _ultoa(unsigned long nValue, char* pBuf, int nRadix);
NOLFCRT_EXTERN void  _splitpath(const char* pPath, char* pDrive, char* pDir, char* pName, char* pExt);
NOLFCRT_EXTERN void  _makepath(char* pPath, const char* pDrive, const char* pDir, const char* pName, const char* pExt);
NOLFCRT_EXTERN char* _fullpath(char* pAbs, const char* pRel, size_t nMax);
NOLFCRT_EXTERN int   _mkdir(const char* pDir);
NOLFCRT_EXTERN int   mkdir_win(const char* pDir);
NOLFCRT_EXTERN long  _filelength(int fd);
NOLFCRT_EXTERN long  filelength(int fd);

/* _findfirst and friends (io.h): matched without case, as Windows does */
#define _A_NORMAL   0x00
#define _A_RDONLY   0x01
#define _A_HIDDEN   0x02
#define _A_SYSTEM   0x04
#define _A_SUBDIR   0x10
#define _A_ARCH     0x20
struct _finddata_t {
    unsigned attrib;
    time_t   time_create, time_access, time_write;
    uint32_t size;
    char     name[260];
};
NOLFCRT_EXTERN intptr_t _findfirst(const char* pSpec, struct _finddata_t* pData);
NOLFCRT_EXTERN int      _findnext(intptr_t hFind, struct _finddata_t* pData);
NOLFCRT_EXTERN int      _findclose(intptr_t hFind);


/* multibyte strings: the game's text is single-byte, so these are the
   plain string functions */
#define _mbstrlen(s)            strlen((const char*)(s))
#define _mbslen(s)              strlen((const char*)(s))
#define _mbscpy(d, s)           ((unsigned char*)strcpy((char*)(d), (const char*)(s)))
#define _mbsncpy(d, s, n)       ((unsigned char*)strncpy((char*)(d), (const char*)(s), (n)))
#define _mbscat(d, s)           ((unsigned char*)strcat((char*)(d), (const char*)(s)))
#define _mbsnbcmp(a, b, n)      strncmp((const char*)(a), (const char*)(b), (n))
#define _mbsnbicmp(a, b, n)     strncasecmp((const char*)(a), (const char*)(b), (n))
#define _mbsncmp(a, b, n)       strncmp((const char*)(a), (const char*)(b), (n))
#define _mbsnbcnt(s, n)         ((size_t)(n))
#define _mbsinc(p)              ((p) + 1)
#define _mbsdec(s, p)           ((p) - 1)
#define _mbschr(s, c)           ((unsigned char*)strchr((const char*)(s), (c)))
#define _mbsrchr(s, c)          ((unsigned char*)strrchr((const char*)(s), (c)))
#define _mbsstr(s, t)           ((unsigned char*)strstr((const char*)(s), (const char*)(t)))
#define _mbsupr(s)              ((unsigned char*)strupr((char*)(s)))
#define _mbslwr(s)              ((unsigned char*)strlwr((char*)(s)))
#define _ismbblead(c)           0
#define _ismbcspace(c)          isspace(c)
#define _ismbcdigit(c)          isdigit(c)

/* wsprintf: sprintf without floating point; the game's uses are plain */
#define wsprintf                sprintf
#define wsprintfA               sprintf
#define wvsprintf               vsprintf


/* Helpers for what only VC accepts, used in the source as NOLF_xxx(...): on
   Windows each expands to the original code (ltbasedefs.h's fallbacks). */
#define NOLF_DEFARG(x)                  /* no default arguments on function pointers */
#define NOLF_CSTR(x)    ((const char*)(x))  /* a CString through '...' */
#ifdef __cplusplus
template<class T> inline T& nolf_lvalue(const T& t) { return const_cast<T&>(t); }
#define NOLF_LVALUE(x)  nolf_lvalue(x)      /* a temporary to a non-const reference */
#endif

#endif /* NOLFCRT_H */
