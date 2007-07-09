#ifndef FTTypes_h__
#define FTTypes_h__

typedef void * PVOID;
typedef void * LPVOID;
typedef unsigned long ULONG;
typedef ULONG DWORD;
typedef DWORD *LPDWORD;
typedef unsigned short USHORT;
typedef unsigned char UCHAR;
typedef unsigned short WORD;
typedef WORD *LPWORD;
typedef unsigned char UCHAR;
typedef UCHAR *PUCHAR;
typedef UCHAR BYTE;
typedef char CHAR;
typedef CHAR *PCHAR;
typedef CHAR *LPSTR;
typedef int BOOL;
typedef ULONG *PULONG;
#define LPCSTR LPSTR
#define WINAPI __stdcall
struct _OVERLAPPED;
typedef _OVERLAPPED * LPOVERLAPPED;
typedef struct _SECURITY_ATTRIBUTES *LPSECURITY_ATTRIBUTES;
typedef PVOID HANDLE;

#endif // FTTypes_h__
