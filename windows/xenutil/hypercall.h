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

#ifndef __HYPERCALL_H__
#include <xen/xen.h>

typedef char hypercall_trap_gate[32];

extern ULONG_PTR __hypercall2(unsigned long ordinal,
                              ULONG_PTR arg1,
                              ULONG_PTR arg2);
extern ULONG_PTR __hypercall3(unsigned long ordinal,
                              ULONG_PTR arg1,
                              ULONG_PTR arg2,
                              ULONG_PTR arg3);
extern ULONG_PTR __hypercall6(unsigned long ordinal,
                              ULONG_PTR arg1,
                              ULONG_PTR arg2,
                              ULONG_PTR arg3,
                              ULONG_PTR arg4,
                              ULONG_PTR arg5,
                              ULONG_PTR arg6);

#define _hypercall2(type, name, arg1, arg2) \
    ((type)__hypercall2(__HYPERVISOR_##name, (ULONG_PTR)arg1, (ULONG_PTR)arg2))
#define _hypercall3(type, name, arg1, arg2, arg3) \
    ((type)__hypercall3(__HYPERVISOR_##name, (ULONG_PTR)arg1, (ULONG_PTR)arg2, (ULONG_PTR)arg3))
#define _hypercall6(type, name, arg1, arg2, arg3, arg4, arg5, arg6) \
    ((type)__hypercall6(__HYPERVISOR_##name, (ULONG_PTR)arg1, (ULONG_PTR)arg2, (ULONG_PTR)arg3, (ULONG_PTR)arg4, (ULONG_PTR)arg5, (ULONG_PTR)arg6))

__declspec(inline) int
HYPERVISOR_event_channel_op(int cmd, void *op)
{
    return _hypercall2(int, event_channel_op, cmd, op);
}

__declspec(inline) int
HYPERVISOR_sched_op(
    int cmd, void *arg)
{
    return _hypercall2(int, sched_op, cmd, arg);
}

_declspec(inline) ULONG_PTR
HYPERVISOR_hvm_op(
    int op, void *arg)
{
    return _hypercall2(ULONG_PTR, hvm_op, op, arg);
}

_declspec(inline) int
HYPERVISOR_memory_op(
    unsigned int cmd, void *arg)
{
    return _hypercall2(int, memory_op, cmd, arg);
}

__declspec(inline) int
HYPERVISOR_grant_table_op(
    unsigned int cmd, void *arg, unsigned nr_operations)
{
    return _hypercall3(int, grant_table_op, cmd, arg, nr_operations);
}

__declspec(inline) int
HYPERVISOR_v4v_op(
    unsigned int cmd, void *arg2, void *arg3, void *arg4, ULONG32 arg5, ULONG32 arg6)
{
    return _hypercall6(int, v4v_op, cmd, arg2, arg3, arg4, arg5, arg6);
}

#endif /* __HYPERCALL_H__ */
