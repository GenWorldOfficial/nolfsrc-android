/* nolfsock.h -- Winsock's names for BSD sockets, for the native build
   (winsock.h and winsock2.h include it). */
#ifndef NOLF_NOLFSOCK_H
#define NOLF_NOLFSOCK_H
#include "nolfwin.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

typedef int SOCKET;
typedef struct sockaddr SOCKADDR;
typedef struct sockaddr* LPSOCKADDR;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr_in* LPSOCKADDR_IN;
typedef struct hostent HOSTENT;
typedef struct hostent* LPHOSTENT;
typedef struct in_addr IN_ADDR;
typedef struct linger LINGER;
typedef struct timeval TIMEVAL;

#define INVALID_SOCKET      (-1)
#define SOCKET_ERROR        (-1)
#ifndef INADDR_NONE
#define INADDR_NONE         0xffffffff

#ifdef __cplusplus
/* Winsock takes the address length as int*, POSIX as socklen_t* */
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
#define closesocket         close
#define ioctlsocket         ioctl
#define WSAEWOULDBLOCK      EWOULDBLOCK
#define WSAECONNRESET       ECONNRESET
#define WSAEMSGSIZE         EMSGSIZE
#define WSAEINPROGRESS      EINPROGRESS
#define WSAEADDRINUSE       EADDRINUSE
#define WSAENOTSOCK         ENOTSOCK
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
    if (p) { memset(p, 0, sizeof(*p)); p->wVersion = p->wHighVersion = nVersion; p->iMaxSockets = 256; p->iMaxUdpDg = 65467; }
    return 0;
}
static inline int WSACleanup(void) { return 0; }
static inline int WSAGetLastError(void) { return errno; }
static inline void WSASetLastError(int n) { errno = n; }

#ifdef __cplusplus
/* Winsock takes the address length as int*, POSIX as socklen_t* */
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
