/* Copyright (c) Citrix Systems Inc.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, 
 * with or without modification, are permitted provided 
 * that the following conditions are met:
 * 
 * *   Redistributions of source code must retain the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer.
 * *   Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the 
 *     following disclaimer in the documentation and/or other 
 *     materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND 
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF 
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING 
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 */

#include <ntddk.h>
#include "xsapi.h"

#include "hypercall.h"

extern hypercall_trap_gate *hypercall_page;

__declspec(inline) ULONG_PTR
__hypercall2(
    unsigned long ordinal,
    ULONG_PTR arg1,
    ULONG_PTR arg2)
{
    ULONG_PTR retval;
    ULONG_PTR addr = (ULONG_PTR)&hypercall_page[ordinal];

    _asm
    {
        mov ecx, arg2;
        mov ebx, arg1;
        mov eax, addr;
        call eax;
        mov retval, eax;
    }
    return retval;
}

__declspec(inline) ULONG_PTR
__hypercall3(
    unsigned long ordinal,
    ULONG_PTR arg1,
    ULONG_PTR arg2,
    ULONG_PTR arg3)
{
    ULONG_PTR retval;
    ULONG_PTR addr = (ULONG_PTR)&hypercall_page[ordinal];

    _asm
    {
        mov edx, arg3;
        mov ecx, arg2;
        mov ebx, arg1;
        mov eax, addr;
        call eax;
        mov retval, eax;
    }
    return retval;
}

#pragma warning(push)
#pragma warning(disable: 4731)

__declspec(inline) ULONG_PTR
__hypercall6(
    unsigned long ordinal,
    ULONG_PTR arg1,
    ULONG_PTR arg2,
    ULONG_PTR arg3,
    ULONG_PTR arg4,
    ULONG_PTR arg5,
    ULONG_PTR arg6)
{
    ULONG_PTR retval;
    ULONG_PTR addr = (ULONG_PTR)&hypercall_page[ordinal];

    _asm
    {
        mov edi, arg5;
        mov esi, arg4;
        mov edx, arg3;
        mov ecx, arg2;
        mov ebx, arg1;
        mov eax, addr;
        /* Handle ebp carefully */
        push ebp;
        push arg6;
        pop ebp;
        call eax;
        pop ebp;
        mov retval, eax;
    }
    return retval;
}
#pragma warning(pop)
