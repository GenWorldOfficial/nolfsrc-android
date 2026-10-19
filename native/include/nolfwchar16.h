/* nolfwchar16.h -- Windows' 16-bit wide strings, for the engine.
 *
 * The engine (dmextra.cpp, for DirectMusic's names) declares the C
 * runtime's wcscmp, wcscpy, wcscat, wcslen, wcsncpy and wcstombs itself, on
 * uint16 characters, as Windows' 16-bit wchar_t is. Here wchar_t is 32 bits
 * and those names are the C library's, so they are renamed to 16-bit
 * versions in native/plat/nolfplat_crt.cpp. The C library's own headers are
 * read first, before the renaming. Included first in the engine's
 * compiles, after nolfcrt.h.
 */
#ifndef NOLF_NOLFWCHAR16_H
#define NOLF_NOLFWCHAR16_H
#include <stdlib.h>
#include <wchar.h>
#define wcscmp      nolf_wcscmp16
#define wcscpy      nolf_wcscpy16
#define wcscat      nolf_wcscat16
#define wcslen      nolf_wcslen16
#define wcsncpy     nolf_wcsncpy16
#define wcstombs    nolf_wcstombs16
#define mbstowcs    nolf_mbstowcs16
#ifdef __cplusplus
extern "C" {
#endif
int            nolf_wcscmp16(const unsigned short* a, const unsigned short* b);
unsigned short* nolf_wcscpy16(unsigned short* d, const unsigned short* s);
unsigned short* nolf_wcscat16(unsigned short* d, const unsigned short* s);
unsigned int   nolf_wcslen16(const unsigned short* s);
unsigned short* nolf_wcsncpy16(unsigned short* d, const unsigned short* s, unsigned int n);
unsigned int   nolf_wcstombs16(char* d, const unsigned short* s, unsigned int n);
unsigned int   nolf_mbstowcs16(unsigned short* d, const char* s, unsigned int n);
#ifdef __cplusplus
}
#endif
#endif
