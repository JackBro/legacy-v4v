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
#include <stdarg.h>
#include <ntstrsafe.h>
#include "xsapi.h"
#include "base.h"
#include "verinfo.h"

#ifndef _XENUTL_H
#define _XENUTL_H

// debug
void CleanupDebugHelpers(void);
void InitDebugHelpers(void);

void ConnectDebugVirq(void);
void DisconnectDebugVirq(void);

void RegisterBugcheckCallbacks(void);
void DeregisterBugcheckCallbacks(void);

void PrintAddress(ULONG_PTR sp, CHAR *buffer, ULONG length);

// iohole
PVOID XenevtchnAllocIoMemory(ULONG nr_bytes, PHYSICAL_ADDRESS *pa);
PFN_NUMBER XenevtchnAllocIoPFN(void);
void XenevtchnReleaseIoMemory(PVOID va, ULONG nr_bytes);
void XenevtchnReleaseIoPFN(PFN_NUMBER pfn);

VOID __XenevtchnShutdownIoHole(const char *module);
#define XenevtchnShutdownIoHole() \
        __XenevtchnShutdownIoHole(XENTARGET);

VOID __XenevtchnInitIoHole(const char *module, PHYSICAL_ADDRESS base, PVOID base_va, ULONG nbytes);
#define XenevtchnInitIoHole(_base, _base_va, _nbytes) \
        __XenevtchnInitIoHole(XENTARGET, (_base), (_base_va), (_nbytes))

BOOLEAN __XenevtchnIsMyIoHole(const char *module);
#define XenevtchnIsMyIoHole() \
        __XenevtchnIsMyIoHole(XENTARGET)

// registry
extern NTSTATUS XenOpenRegistryKey(
    IN  const WCHAR     *Path,
    IN  ACCESS_MASK     Access,
    OUT PHANDLE         pKey
    );

extern NTSTATUS XenReadRegistryValue(
    IN  const WCHAR                     *Path,
    IN  const WCHAR                     *Name,
    OUT PKEY_VALUE_PARTIAL_INFORMATION  *pInfo
    );

// xenbus
#define MAX_XENBUS_PATH 256

NTSTATUS
XenevtchnInitXenbus(
    VOID
);

VOID
CleanupXenbus(
    VOID
);

NTSTATUS xenbus_remove(xenbus_transaction_t xbt, const char *path);
void xenbus_recover_from_s3(void);

extern EVTCHN_PORT xenbus_evtchn;

void xenbus_fail_transaction(xenbus_transaction_t xbt, NTSTATUS status);

// evtchn
EVTCHN_PORT EvtchnBindVirq(int virq, PEVTCHN_HANDLER_CB handler, PVOID context);

EVTCHN_PORT EvtchnRegisterHandler(int xen_port,
                                  PEVTCHN_HANDLER_CB cb,
                                  void *Context);


BOOLEAN
EvtchnHandleInterrupt(
    IN PVOID Interrupt,
    IN OUT PVOID Context
    );

extern VOID
EvtchnSetVector(ULONG vector);

extern ULONG
EvtchnGetVector(VOID);

extern VOID
XenbusSetFrozen(BOOLEAN State);

extern BOOLEAN
XenbusIsFrozen(VOID);

extern VOID                 XenSetSystemPowerState(SYSTEM_POWER_STATE State);
extern SYSTEM_POWER_STATE   XenGetSystemPowerState(VOID);

#ifdef XSAPI_FUTURE_CONNECT_EVTCHN
__MAKE_WRAPPER_PRIV(ALIEN_EVTCHN_PORT, unsigned)
static __inline ALIEN_EVTCHN_PORT
wrap_ALIEN_EVTCHN_PORT(unsigned x)
{
    return __wrap_ALIEN_EVTCHN_PORT(x ^ 0xf001dead);
}
static __inline unsigned
unwrap_ALIEN_EVTCHN_PORT(ALIEN_EVTCHN_PORT x)
{
    return __unwrap_ALIEN_EVTCHN_PORT(x) ^ 0xf001dead;
}
#endif

extern unsigned xen_EVTCHN_PORT(EVTCHN_PORT port);

extern NTSTATUS EvtchnStart(VOID);
extern VOID EvtchnStop(VOID);

// xenutl
#define XUTIL_TAG 'LUTX'

extern VOID *dom0_debug_port;

typedef ULONG (*DBG_PRINT)(
    IN CHAR *,
    ...
    );

typedef ULONG (*VDBG_PRINT_EX)(
    IN ULONG,
    IN ULONG,
    IN const CHAR *,
    IN va_list
    );

extern VDBG_PRINT_EX __XenvDbgPrintEx;

extern ULONG
__XenDbgPrint(
    IN CHAR *Format,
    ...
    );

extern DBG_PRINT XenDbgPrint;

typedef VOID (*WPP_TRACE)(
    IN XEN_TRACE_LEVEL Level,
    IN CHAR *Message
    );

extern VOID SetWppTrace(
    IN WPP_TRACE Function
    );

extern NTSTATUS
XenWorkItemInit(
    VOID
    );

extern ULONG SuspendGetCount(void);

extern BOOLEAN XenPVEnabled(VOID);

#endif  // _XENUTL_H

