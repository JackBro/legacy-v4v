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

#ifndef _HVM_H_
#define _HVM_H_

#include <ntddk.h>
#include <xen/xen.h>

extern shared_info_t *HYPERVISOR_shared_info;
extern int HvmInterruptNumber;

//
// Function prototypes.
//
NTSTATUS
InitHvm(void);

VOID
CleanupHvm(
       VOID
);

int
AddPageToPhysmap(PFN_NUMBER pfn,
         unsigned space,
         unsigned long offset);

ULONG_PTR
HvmGetParameter(int param_nr);

VOID _wrmsr(uint32_t msr, uint32_t lowbits, uint32_t highbits);

void UnquiesceSystem(KIRQL OldIrql);
KIRQL QuiesceSystem(VOID);

VOID KillSuspendThread(VOID);
VOID SuspendPreInit(VOID);

int __HvmSetCallbackIrq(const char *caller, int irq);
#define HvmSetCallbackIrq(_irq) __HvmSetCallbackIrq(__FUNCTION__, (_irq))

void DescheduleVcpu(unsigned ms);

NTSTATUS HvmResume(VOID *ignore, SUSPEND_TOKEN token);

extern void EvtchnLaunchSuspendThread(VOID);

#ifndef AMD64
/* Force a processor pipeline flush, so that self-modifying code
   becomes safe to run. */
#define FLUSH_PIPELINE() _asm { cpuid }

#else
/* AMD64 -> no binary patches -> FLUSH_PIPELINE is a no-op */
#define FLUSH_PIPELINE() do {} while (0)
#endif

#endif /* _HVM_H_ */

