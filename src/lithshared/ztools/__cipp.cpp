// zTools: __caller_ip — returns the return address of the calling
// function's frame. Reconstructed from the shipped __cipp.obj: the inline
// asm forces a stack frame and reads the saved return address at [ebp+4].
#include "ztools.h"

unsigned long __caller_ip()
{
#ifdef NOLF_NATIVE
	return (unsigned long)(uintptr_t)__builtin_return_address(0);
#else
	unsigned long ip;

	__asm
	{
		mov eax, [ebp+4]
		mov ip, eax
	}

	return ip;
#endif
}
