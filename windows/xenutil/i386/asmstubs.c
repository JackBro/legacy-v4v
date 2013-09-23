#include <wdm.h>
#include "xsapi.h"
#include "base.h"
#include "hvm.h"

VOID
_cpuid(ULONG leaf, ULONG *peax, ULONG *pebx, ULONG *pecx,
       ULONG *pedx)
{
    ULONG reax, rebx, recx, redx;
    _asm {
        mov ebx, 0x72746943;
        mov ecx, 0x582f7869;
        mov edx, 0x56506e65;
        mov eax, leaf;
        cpuid;
        mov reax, eax;
        mov rebx, ebx;
        mov recx, ecx;
        mov redx, edx;
    };
    *peax = reax;
    *pebx = rebx;
    *pecx = recx;
    *pedx = redx;
}

VOID
_wrmsr(uint32_t msr, uint32_t lowbits, uint32_t highbits)
{
    _asm {
        mov eax, lowbits;
        mov edx, highbits;
        mov ecx, msr;
        wrmsr;
    };
}

