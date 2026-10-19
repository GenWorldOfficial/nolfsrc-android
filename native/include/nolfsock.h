/* nolfsock.h -- Winsock, for the native build (winsock.h and winsock2.h
 * include it).
 *
 * The engine reads addresses the Winsock way (sin_addr.S_un.S_un_b.s_b1,
 * sin_addr.S_un.S_addr), which the system's struct in_addr cannot give. So
 * this declares Winsock's own structs and the BSD socket calls on them,
 * and never includes <netinet/in.h>, <arpa/inet.h> or <netdb.h>: Windows'
 * sockaddr_in and in_addr have the same bytes in the same places as Linux's
 * and Android's, and the calls take them by pointer, which the C library
 * does not look behind. <sys/socket.h> brings sockaddr and the constants.
 */
#ifndef NOLF_NOLFSOCK_H
#define NOLF_NOLFSOCK_H
#include "nolfwin.h"
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

/* u_char, u_short and u_long come from <sys/types.h>; addresses are
   uint32_t here, as Winsock's u_long is 32 bits wherever long is not */

/* netinet/in.h, as Windows has it */
struct in_addr {
    union {
        struct { u_char s_b1, s_b2, s_b3, s_b4; } S_un_b;
        struct { u_short s_w1, s_w2; } S_un_w;
        uint32_t S_addr;
    } S_un;
};
#define s_addr  S_un.S_addr
#define s_host  S_un.S_un_b.s_b2
#define s_net   S_un.S_un_b.s_b1
#define s_imp   S_un.S_un_w.s_w2
#define s_impno S_un.S_un_b.s_b4
#define s_lh    S_un.S_un_b.s_b3
struct sockaddr_in {
    unsigned short sin_family;
    u_short        sin_port;
    struct in_addr sin_addr;
    char           sin_zero[8];
};
#define IPPROTO_IP          0
#define IPPROTO_TCP         6
#define IPPROTO_UDP         17
#define INADDR_ANY          ((uint32_t)0x00000000)
#define INADDR_LOOPBACK     0x7f000001
#define INADDR_BROADCAST    ((uint32_t)0xffffffff)
#define INADDR_NONE         0xffffffff
#define TCP_NODELAY         1

/* netdb.h: the layout the C libraries share */
struct hostent {
    char*  h_name;
    char** h_aliases;
    int    h_addrtype;
    int    h_length;
    char** h_addr_list;
};
#define h_addr h_addr_list[0]

#ifdef __cplusplus
extern "C" {
#endif
uint32_t        inet_addr(const char* p);
char*           inet_ntoa(struct in_addr a);
struct hostent* gethostbyname(const char* pName);
struct hostent* gethostbyaddr(const void* pAddr, socklen_t nLen, int nType);
#ifdef __cplusplus
}
#endif
static inline u_short nolf_bswap16(u_short v) { return (u_short)((v >> 8) | (v << 8)); }
static inline uint32_t nolf_bswap32(uint32_t v) { return __builtin_bswap32(v); }
#define htons(v) nolf_bswap16((u_short)(v))
#define ntohs(v) nolf_bswap16((u_short)(v))
#define htonl(v) nolf_bswap32((uint32_t)(v))
#define ntohl(v) nolf_bswap32((uint32_t)(v))

typedef int SOCKET;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr* LPSOCKADDR;
typedef struct sockaddr* PSOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr_in* LPSOCKADDR_IN;
typedef struct sockaddr_in* PSOCKADDR_IN;
typedef struct hostent HOSTENT;
typedef struct hostent* LPHOSTENT;
typedef struct hostent* PHOSTENT;
typedef struct in_addr IN_ADDR;
typedef struct linger LINGER;
typedef struct timeval TIMEVAL;

#define INVALID_SOCKET      (-1)
#define SOCKET_ERROR        (-1)
#define closesocket         close
#define ioctlsocket(s, cmd, arg) ioctl((s), (cmd), (arg))

/* Winsock's errors are the BSD ones, numbered 10000 up; here errno's own */
#define WSABASEERR          10000
#define WSAEINTR            EINTR
#define WSAEBADF            EBADF
#define WSAEACCES           EACCES
#define WSAEFAULT           EFAULT
#define WSAEINVAL           EINVAL
#define WSAEMFILE           EMFILE
#define WSAEWOULDBLOCK      EWOULDBLOCK
#define WSAEINPROGRESS      EINPROGRESS
#define WSAEALREADY         EALREADY
#define WSAENOTSOCK         ENOTSOCK
#define WSAEDESTADDRREQ     EDESTADDRREQ
#define WSAEMSGSIZE         EMSGSIZE
#define WSAEPROTOTYPE       EPROTOTYPE
#define WSAENOPROTOOPT      ENOPROTOOPT
#define WSAEPROTONOSUPPORT  EPROTONOSUPPORT
#define WSAESOCKTNOSUPPORT  ESOCKTNOSUPPORT
#define WSAEOPNOTSUPP       EOPNOTSUPP
#define WSAEPFNOSUPPORT     EPFNOSUPPORT
#define WSAEAFNOSUPPORT     EAFNOSUPPORT
#define WSAEADDRINUSE       EADDRINUSE
#define WSAEADDRNOTAVAIL    EADDRNOTAVAIL
#define WSAENETDOWN         ENETDOWN
#define WSAENETUNREACH      ENETUNREACH
#define WSAENETRESET        ENETRESET
#define WSAECONNABORTED     ECONNABORTED
#define WSAECONNRESET       ECONNRESET
#define WSAENOBUFS          ENOBUFS
#define WSAEISCONN          EISCONN
#define WSAENOTCONN         ENOTCONN
#define WSAESHUTDOWN        ESHUTDOWN
#define WSAETOOMANYREFS     ETOOMANYREFS
#define WSAETIMEDOUT        ETIMEDOUT
#define WSAECONNREFUSED     ECONNREFUSED
#define WSAELOOP            ELOOP
#define WSAENAMETOOLONG     ENAMETOOLONG
#define WSAEHOSTDOWN        EHOSTDOWN
#define WSAEHOSTUNREACH     EHOSTUNREACH
#define WSAENOTEMPTY        ENOTEMPTY
#define WSAEPROCLIM         (WSABASEERR + 67)
#define WSAEUSERS           EUSERS
#define WSAEDQUOT           EDQUOT
#define WSAESTALE           ESTALE
#define WSAEREMOTE          EREMOTE
#define WSAEDISCON          (WSABASEERR + 101)
#define WSASYSNOTREADY      (WSABASEERR + 91)
#define WSAVERNOTSUPPORTED  (WSABASEERR + 92)
#define WSANOTINITIALISED   (WSABASEERR + 93)
#define WSAHOST_NOT_FOUND   (WSABASEERR + 1001)
#define WSATRY_AGAIN        (WSABASEERR + 1002)
#define WSANO_RECOVERY      (WSABASEERR + 1003)
#define WSANO_DATA          (WSABASEERR + 1004)
#define WSADESCRIPTION_LEN  256
#define WSASYS_STATUS_LEN   128

typedef struct WSAData {
    WORD  wVersion, wHighVersion;
    char  szDescription[WSADESCRIPTION_LEN + 1];
    char  szSystemStatus[WSASYS_STATUS_LEN + 1];
    unsigned short iMaxSockets, iMaxUdpDg;
    char* lpVendorInfo;
} WSADATA, *LPWSADATA;

static inline int WSAStartup(WORD nVersion, WSADATA* p)
{
    if (p)
    {
        memset(p, 0, sizeof(*p));
        p->wVersion = p->wHighVersion = nVersion;
        p->iMaxSockets = 256;
        p->iMaxUdpDg = 65467;
    }
    return 0;
}
static inline int  WSACleanup(void) { return 0; }
static inline int  WSAGetLastError(void) { return errno; }
static inline void WSASetLastError(int n) { errno = n; }

#ifdef __cplusplus
/* Winsock takes an address length as int*, POSIX as socklen_t* */
static inline ssize_t recvfrom(int s, void* b, size_t n, int f, struct sockaddr* a, int* pLen)
{ socklen_t l = pLen ? (socklen_t)*pLen : 0; ssize_t r = recvfrom(s, b, n, f, a, pLen ? &l : 0); if (pLen) *pLen = (int)l; return r; }
static inline int accept(int s, struct sockaddr* a, int* pLen)
{ socklen_t l = pLen ? (socklen_t)*pLen : 0; int r = accept(s, a, pLen ? &l : 0); if (pLen) *pLen = (int)l; return r; }
static inline int getsockname(int s, struct sockaddr* a, int* pLen)
{ socklen_t l = pLen ? (socklen_t)*pLen : 0; int r = getsockname(s, a, pLen ? &l : 0); if (pLen) *pLen = (int)l; return r; }
static inline int getpeername(int s, struct sockaddr* a, int* pLen)
{ socklen_t l = pLen ? (socklen_t)*pLen : 0; int r = getpeername(s, a, pLen ? &l : 0); if (pLen) *pLen = (int)l; return r; }
static inline int getsockopt(int s, int lv, int o, char* v, int* pLen)
{ socklen_t l = pLen ? (socklen_t)*pLen : 0; int r = getsockopt(s, lv, o, v, pLen ? &l : 0); if (pLen) *pLen = (int)l; return r; }
#endif
#endif
