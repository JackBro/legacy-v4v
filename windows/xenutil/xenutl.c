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

#include "xenutl.h"
#include "aux_klib.h"
#include "hvm.h"
#include "xenutl.h"
#include "xsmtcapi.h"

#define DEBUG_DEFAULT_FEATURE_SET (                               \
                                    DEBUG_TRAP_DBGPRINT         | \
                                    DEBUG_DISABLE_LICENSE_CHECK | \
                                    0                             \
                                  )

static ULONG    _g_PVFeatureFlags   = DEBUG_DEFAULT_FEATURE_SET;

static BOOLEAN done_unplug; /* True if we've done a device unplug.
                               This is not reset when we hibernate, so
                               is only an approximation to whether the
                               emulated devices are actually
                               present. */

extern PULONG InitSafeBootMode;

static const CHAR *
FeatureFlagName(
    IN  ULONG   Bit
    )
{
#define _FLAG_NAME(_flag)                                       \
    case DEBUG_ ## _flag:                                       \
        return #_flag;

    XM_ASSERT(Bit < 32);
    switch (1ul << Bit) {
    _FLAG_NAME(NO_PARAVIRT);
    _FLAG_NAME(VERY_LOUD);
    _FLAG_NAME(VERY_QUIET);
    _FLAG_NAME(HA_SAFEMODE);
    _FLAG_NAME(TRAP_DBGPRINT);
    _FLAG_NAME(DISABLE_LICENSE_CHECK);
    default:
        break;
    }

    return "UNKNOWN";
}

static VOID
DumpFeatureFlags(
    VOID
    )
{
    ULONG   Flags = _g_PVFeatureFlags;
    ULONG   Bit;

    TraceNotice(("PV feature flags:\n"));
    Bit = 0;
    while (Flags != 0) {
        if (Flags & 0x00000001)
            TraceNotice(("- %s\n", FeatureFlagName(Bit)));

        Flags >>= 1;
        Bit++;
    }
}

static ULONG
wcstoix(__in PWCHAR a, __out PBOOLEAN err)
{
    ULONG acc = 0;
    if (err) *err = FALSE;
    if (!a || !a[0]) *err = TRUE;

    while (a[0] && a[0] != L' ') {
        if (a[0] >= L'0' && a[0] <= L'9')
            acc = acc * 16 + a[0] - L'0';
        else if (a[0] >= L'a' && a[0] <= L'f')
            acc = acc * 16 + a[0] - L'a' + 10;
        else if (a[0] >= L'A' && a[0] <= L'F')
            acc = acc * 16 + a[0] - L'A' + 10;
        else
        {
            if (err) *err = TRUE;
            break;
        }
        
        a++;
    }
    return acc;
}

/* Read a DWORD out of xenevtchn's parameters key. */
static NTSTATUS
ReadRegistryParameterDword(const WCHAR *value, ULONG *out)
{
    NTSTATUS stat;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValueInfo;

    pKeyValueInfo = NULL;

    stat = XenReadRegistryValue(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\xenevtchn\\Parameters",
                                value,
                                &pKeyValueInfo);
    if (NT_SUCCESS(stat))
    {
        if (pKeyValueInfo->Type == REG_DWORD)
        {
            *out = *((ULONG*)pKeyValueInfo->Data);
        }
        else
        {
            TraceError(("Expected %ws to be a DWORD, actually type %x\n",
                        value, pKeyValueInfo->Type));
            stat = STATUS_INVALID_PARAMETER;
        }
        ExFreePool(pKeyValueInfo);
    }
    return stat;
}

/* Read a flags field out of the registry.  Returns 0 if there is any
   error reading the key.  @value says which value to read; it is
   pulled out of the xenevtchn service's parameters key. */
static ULONG
ReadRegistryFlagsOrZero(PWCHAR value)
{
    ULONG res;
    NTSTATUS stat;

    stat = ReadRegistryParameterDword(value, &res);
    if (NT_SUCCESS(stat))
        return res;
    else
        return 0;
}

#define XUTIL_TAG 'LUTX'

static VOID
XenParseBootParams(void)
{
    NTSTATUS        Status;
    PKEY_VALUE_PARTIAL_INFORMATION pKeyValueInfo = NULL;

    PWCHAR          wzReturnValue = NULL;
    PWCHAR          wzPVstring;
    ULONG           features;
    BOOLEAN         bError;
    BOOLEAN         bDefault;

    //
    // XXX: HACK: Should never be called at anything other then
    // passive level but we cant import KeGetCurrentIRql in the xenvbd
    // and we try to call this again in dump_xenvbd at high irql which
    // is bad so the check is here to keep the compiler/linker happy.
    //
    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {
        return;
    }

    Status = ReadRegistryParameterDword(L"FeatureFlags", &features);
    if (NT_SUCCESS(Status))
        _g_PVFeatureFlags = features;
    _g_PVFeatureFlags |= ReadRegistryFlagsOrZero(L"SetFlags");
    _g_PVFeatureFlags &= ~ReadRegistryFlagsOrZero(L"ClearFlags");

    if (*InitSafeBootMode > 0) {
        _g_PVFeatureFlags |= DEBUG_NO_PARAVIRT;
    }   

    //
    // If the "NOPVBoot" value us in our service's key then do *not*
    // go into pv mode.
    //
    Status = XenReadRegistryValue(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Services\\xenevtchn",
                                  L"NOPVBoot",
                                  &pKeyValueInfo);
    if (NT_SUCCESS(Status)) 
    {
        _g_PVFeatureFlags |= DEBUG_NO_PARAVIRT;
        ExFreePool(pKeyValueInfo);
        pKeyValueInfo = NULL;
    }

    //
    // Normally we want to assert paged code here but whatever. see above.
    //
    Status = XenReadRegistryValue(L"\\Registry\\Machine\\SYSTEM\\CurrentControlSet\\Control",
                                  L"SystemStartOptions",
                                  &pKeyValueInfo);
    if (! NT_SUCCESS(Status))
    {
        TraceError(("%s: Failed with Status = %d\n", __FUNCTION__, Status));
        goto _Cleanup;
    }

    //
    // A hack to null terminate the string
    //

    wzReturnValue = ExAllocatePoolWithTag ( PagedPool, 
                                            pKeyValueInfo->DataLength 
                                             + sizeof(WCHAR),
                                            XUTIL_TAG);
    if (wzReturnValue == NULL) 
    {
        ASSERT (FALSE);
        goto _Cleanup;
    }
    
    RtlCopyMemory (wzReturnValue, pKeyValueInfo->Data,
                   pKeyValueInfo->DataLength);
    wzReturnValue[pKeyValueInfo->DataLength/sizeof(WCHAR)] = L'\0';

    bError = FALSE;
    bDefault = FALSE;
    features = 0;

    //
    // find the /PV=X string where X is a hex string of features
    //
        
    wzPVstring = wcsstr (wzReturnValue, L" PV");

    if (wzPVstring != NULL)
    {

        switch (wzPVstring[3])
        {
        case L'=':
            features = wcstoix(&wzPVstring[4], &bError);
            break;

        case L'^':
                if (wzPVstring[4] == L'=')
                {
                    features = wcstoix(&wzPVstring[5], &bError);
                    features = _g_PVFeatureFlags ^ features;
                }
                else
                {
                    bError = TRUE;
                }
                break;

            case L'~':
                if (wzPVstring[4] == L'=')
                {
                    features = wcstoix(&wzPVstring[5], &bError);
                    features = _g_PVFeatureFlags & ~features;
                }
                else
                {
                    bError = TRUE;
                }
                break;

            case L'|':
                if (wzPVstring[4] == L'=')
                {
                    features = wcstoix(&wzPVstring[5], &bError);
                    features = _g_PVFeatureFlags | features;
                }
                else
                {
                    bError = TRUE;
                }
                break;

            case L'\0':
            case L'\t':
            case L' ':
                bDefault = TRUE;
                break;

            default:
                bError = TRUE;
                break;
        }

        if (!bError)
        {
            if (!bDefault)
            {
                TraceNotice (("%s: Booting PV features = %I64x\n", __FUNCTION__, features));
                _g_PVFeatureFlags = features;
            }
        }
        else
        {
            TraceWarning(("%s: error parsing /PV argument.\n", __FUNCTION__));
        }
    }

    if (_g_PVFeatureFlags & DEBUG_HA_SAFEMODE)
    {
        if (_g_PVFeatureFlags & DEBUG_NO_PARAVIRT)
        {
            //
            // If both the DEBUG_HA_SAFEMODE and DEBUG_NO_PARAVIRT flags
            // are set the clear the DEBUG_NO_PARAVIRT so we will start
            // up xenevtchn.
            //
            _g_PVFeatureFlags &= ~DEBUG_NO_PARAVIRT;
        } 
        else 
        {
            //
            // This means the DEBUG_HA_SAFEMODE flag was set but the
            // DEBUG_NO_PARAVIRT is not set, so we aren't in safe mode.
            // This is not a valid config so just clear the DEBUG_HA_SAFEMODE
            // flag
            //
            _g_PVFeatureFlags &= ~DEBUG_HA_SAFEMODE;
        }
    }

    if (!AustereMode) {
        if (_g_PVFeatureFlags & DEBUG_NO_PARAVIRT)
            TraceNotice(("Booting into NON-PV mode\n"));
        else if (_g_PVFeatureFlags & DEBUG_HA_SAFEMODE)
            TraceNotice(("Booting into NON-PV mode (HA PV mode)\n"));
        else
            TraceNotice(("Booting into PV mode\n"));
    }

_Cleanup:
    if (wzReturnValue != NULL)
        ExFreePool (wzReturnValue);
        
    if (pKeyValueInfo != NULL)
        ExFreePool (pKeyValueInfo);

    if (!CheckXenHypervisor())
        _g_PVFeatureFlags |= DEBUG_NO_PARAVIRT;

    if (!AustereMode)
        DumpFeatureFlags();

    return;
}

BOOLEAN
XenPVFeatureEnabled(
    IN ULONG    FeatureFlag
    )
{
    return (BOOLEAN)((_g_PVFeatureFlags & FeatureFlag) != 0);
}

BOOLEAN
_XmCheckXenutilVersionString(
    IN  const CHAR  *Module,
    IN  BOOLEAN     Critical,
    IN  const CHAR  *ExpectedVersion
    )
{
    if (strcmp(ExpectedVersion, XENUTIL_CURRENT_VERSION) == 0)
        return TRUE;

    if (Critical) {
        TraceCritical(("%s expected XENUTIL version %s, but got version %s!\n",
                       Module,
                       ExpectedVersion,
                       XENUTIL_CURRENT_VERSION));

        if (done_unplug)
            TraceBugCheck(("Can't start PV drivers, but have already disconnected emulated devices!\n"));

        // Prevent any further PV activity
        _g_PVFeatureFlags |= DEBUG_NO_PARAVIRT;
    } else {
        TraceWarning(("%s expected XENUTIL version %s, but got version %s!\n",
                      Module,
                      ExpectedVersion,
                      XENUTIL_CURRENT_VERSION));
    }

    return FALSE;
}

BOOLEAN
XenPVEnabled()
{
    if (_g_PVFeatureFlags & DEBUG_NO_PARAVIRT)
        return FALSE;
    else
        return TRUE;
}

/* Device unplugging and connecting to the log-to-dom0 port.  This is
   slightly icky, mostly because there are two different protocols.

   In the old protocol, we have two io ports on the PCI scsi
   controller, one for unplugging and one for logging.  Writing
   anything to the unplug port causes both network and IDE disks to
   get unplugged.  Log-to-dom0 is done by writing bytes to the other
   port.

   In the new protocol, we still have two ports, but they're
   hard-coded to 0x10 and 0x12, which are reserved to the motherboard
   in the ACPI tables.  You can tell the new protocol is available if
   reading 0x10 gives you the signature 0x49d2.  If the new protocol
   is available, there will be a version number which can be obtained
   by reading a byte from 0x12.  There are two versions:

   -- 0, the base protocol.  You unplug devices by writing a USHORT
      bitmask to 0x10 (0x01 -> IDE disks, 0x02 -> rtl8139 NICs, all
      other bits -> reserved).  Logging is done by writing bytes to
      0x12.

   -- 1, which is like 0 but adds a mechanism for telling qemu what
      version of the drivers we are, and for qemu to block versions
      which are known to be bad.  The drivers are expected to write a
      product id to port 0x12 as a short and their build number to
      0x10 as a long, and then check the magic on port 0x10 again.  If
      the drivers are blacklisted, it will have changed to something
      other than the magic.  The only defined product ID is 1, which
      is the Citrix Windows PV drivers.

   The old protocol still works on new toolstacks, but the new one is
   better because it means you can unplug the PCI devices before the
   PCI driver comes up.
*/

static USHORT unplug_protocol; /* 0 -> old, 1 -> new */

/* Old protocol */
static PVOID device_unplug_port_old; /* NULL -> unknown or new
                                      * protocol */

/* New protocol */
#define NEW_UNPLUG_PORT ((PVOID)(ULONG_PTR)0x10)
#define NEW_DOM0LOG_PORT ((PVOID)(ULONG_PTR)0x12)
#define UNPLUG_VERSION_PORT ((PVOID)(ULONG_PTR)0x12)
#define UNPLUG_DRIVER_VERSION_PORT ((PVOID)(ULONG_PTR)0x10)
#define UNPLUG_DRIVER_PRODUCT_PORT ((PVOID)(ULONG_PTR)0x12)
#define DEVICE_UNPLUG_PROTO_MAGIC 0x49d2
#define UNPLUG_DRIVER_PRODUCT_NUMBER 1

#define UNPLUG_ALL_IDE 1
#define UNPLUG_ALL_NICS 2
#define UNPLUG_AUX_IDE 4

/* Shared */
PVOID dom0_debug_port;

static VOID
ParseRegistryPath(
    IN  PUNICODE_STRING Path
    )
{
    PWSTR Name;

    // Assume 'Path' is NUL terminated
    Name = wcsrchr(Path->Buffer, L'\\');
    Name++;

    if (wcscmp(Name, L"dump_XENUTIL") == 0) {
        TraceNotice(("Loading PV drivers in DUMP mode.\n"));
        SetOperatingMode(DUMP_MODE);
    } else if (wcscmp(Name, L"hiber_XENUTIL") == 0) {
        TraceNotice(("Loading PV drivers in HIBER mode.\n"));
        SetOperatingMode(HIBER_MODE);
    } else {
        TraceNotice(("Loading PV drivers in NORMAL mode.\n"));
        SetOperatingMode(NORMAL_MODE);
    }
}

typedef NTSTATUS (*PDBG_SET_DEBUG_FILTER_STATE)(
    IN ULONG,
    IN ULONG,
    IN BOOLEAN
    );

static PDBG_SET_DEBUG_FILTER_STATE __XenDbgSetDebugFilterState;

//
// XC-4394
//
// The following static and function are only used by xenvbd
// when entering hibernate (and probably crash dump file generation).
// We must detect Win7 SP1 so we can avoid a problem in SCSIPORT
// that was introduced with SP1.
//
static RTL_OSVERSIONINFOEXW Info;

VOID XenutilGetOsVersionDuringAustere(PXEN_WINDOWS_VERSION WinVer)
{
	WinVer->dwMajorVersion = Info.dwMajorVersion;
	WinVer->dwMinorVersion = Info.dwMinorVersion;
	WinVer->dwBuildNumber = Info.dwBuildNumber;
	WinVer->dwPlatformId = Info.dwPlatformId;
	WinVer->wServicePackMajor = Info.wServicePackMajor;
	WinVer->wServicePackMinor = Info.wServicePackMinor;
}

NTSTATUS
DllInitialize(PUNICODE_STRING RegistryPath)
{
    UNICODE_STRING fn;
    NTSTATUS ntStatus;

    TraceInfo(("%s __XEN_INTERFACE_VERSION__=%x\n", __FUNCTION__, __XEN_INTERFACE_VERSION__));

    ParseRegistryPath(RegistryPath);

    if (!AustereMode)
        XenWorkItemInit();

	if (AustereMode)
		XenutilGetVersionInfo(&Info);

    ntStatus = AuxKlibInitialize();
    if (!NT_SUCCESS(ntStatus)) {
        TraceError(("Cannot initialize auxiliary library: %x\n",
                    ntStatus));
        return ntStatus;
    }

    /*
     * Attempt to replace the default DbgPrint call with a
     * call to DbgPrintEx, to allow better debug filtering
     */
    RtlInitUnicodeString(&fn, L"vDbgPrintEx");
    __XenvDbgPrintEx = (VDBG_PRINT_EX)(ULONG_PTR)MmGetSystemRoutineAddress(&fn);

    if (__XenvDbgPrintEx != NULL) {
        XenDbgPrint = __XenDbgPrint;

#if DBG
        /*
         * Attempt to enable the appropriate debug filter to avoid having to set it
         * in the registry.
         */
        RtlInitUnicodeString(&fn, L"DbgSetDebugFilterState");
        __XenDbgSetDebugFilterState = (PDBG_SET_DEBUG_FILTER_STATE)(ULONG_PTR)MmGetSystemRoutineAddress(&fn);

        if (__XenDbgSetDebugFilterState != NULL)
            __XenDbgSetDebugFilterState(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, TRUE);
#endif  //DBG
    }

    XenParseBootParams();

    if (XenPVFeatureEnabled(DEBUG_MTC_PROTECTED_VM))
        XenTraceSetMtcLevels();

    if (XenPVFeatureEnabled(DEBUG_NO_PARAVIRT))
        return STATUS_SUCCESS;

    if (XenPVFeatureEnabled(DEBUG_VERY_LOUD))
    {
        int dispositions[XenTraceLevels] = {-1,-1,-1,-1,-1,-1,-1};
        XenTraceSetLevels(dispositions);
    }

    if (XenPVFeatureEnabled(DEBUG_VERY_QUIET))
    {
        int dispositions[XenTraceLevels] = {0};
        TraceNotice(("Disable all logging...\n"));
        XenTraceSetLevels(dispositions);
    }

    if (XenPVFeatureEnabled(DEBUG_HA_SAFEMODE))
        return STATUS_SUCCESS;

    return STATUS_SUCCESS;
}

static BOOLEAN
isInS3;
static SUSPEND_TOKEN
s3SuspendToken;

void
XmRecoverFromS3(void)
{
    if (!isInS3)
        return;
    isInS3 = FALSE;
    HvmResume(NULL, s3SuspendToken);
    ConnectDebugVirq();
    xenbus_recover_from_s3();
    EvtchnReleaseSuspendToken(s3SuspendToken);
}

void
XmPrepForS3(void)
{
    if (isInS3)
        return;
    isInS3 = TRUE;
    s3SuspendToken = EvtchnAllocateSuspendToken("S3");
    DisconnectDebugVirq();
}

static ULONG XenCPUIDBaseLeaf = 0x40000000;

VOID
XenCpuid(ULONG leaf, ULONG *peax, ULONG *pebx, ULONG *pecx, ULONG *pedx)
{
    _cpuid(leaf + XenCPUIDBaseLeaf, peax, pebx, pecx, pedx);
}

BOOLEAN
CheckXenHypervisor(void)
{
    ULONG eax, ebx ='fool', ecx = 'beef', edx = 'dead';
    char signature[13];

    //
    // Check that we're running on Xen and that CPUID supports leaves up to
    // at least 0x40000002 which we need to get the hypercall page info.
    //
    // Note: The Xen CPUID leaves may have been shifted upwards by a
    // multiple of 0x100.
    //

    for (; XenCPUIDBaseLeaf <= 0x40000100; XenCPUIDBaseLeaf += 0x100)
    {
        _cpuid(XenCPUIDBaseLeaf, &eax, &ebx, &ecx, &edx);

        *(ULONG*)(signature + 0) = ebx;
        *(ULONG*)(signature + 4) = ecx;
        *(ULONG*)(signature + 8) = edx;
        signature[12] = 0;
        if ( ((strcmp("XenVMMXenVMM", signature) == 0)||
              (strcmp("XciVMMXciVMM", signature) == 0))&&
               (eax >= (XenCPUIDBaseLeaf + 2)))
        {
            return TRUE;
        }
    }
    return FALSE;
}

static void
xm_thread_func(void *data)
{
    struct xm_thread *me = data;

    PsTerminateSystemThread(me->cb(me, me->data));
}

struct xm_thread *
XmSpawnThread(NTSTATUS (*cb)(struct xm_thread *xt, void *d), void *d)
{
    struct xm_thread *work;
    NTSTATUS stat;
    HANDLE tmpHandle;

    work = XmAllocateMemory(sizeof(*work));
    if (!work) return NULL;
    work->exit = FALSE;
    KeInitializeEvent(&work->event, NotificationEvent, FALSE);
    work->cb = cb;
    work->data = d;
    stat = PsCreateSystemThread(&tmpHandle, THREAD_ALL_ACCESS, NULL,
                                INVALID_HANDLE_VALUE, NULL, xm_thread_func,
                                work);
    if (!NT_SUCCESS(stat)) {
        XmFreeMemory(work);
        return NULL;
    }
    stat = ObReferenceObjectByHandle(tmpHandle, SYNCHRONIZE, NULL,
                                     KernelMode, &work->thread,
                                     NULL);
    ZwClose(tmpHandle);
    if (!NT_SUCCESS(stat)) {
        /* We can't reliably kill the thread in this case, and
           therefore can't release memory.  Instruct it to exit soon
           and hope for the best. */
        work->exit = TRUE;
        KeSetEvent(&work->event, IO_NO_INCREMENT, FALSE);
        return NULL;
    }

    return work;
}

void
XmKillThread(struct xm_thread *t)
{
    if (!t)
        return;
    TraceDebug (("Killing thread %p.\n", t));
    XM_ASSERT(KeGetCurrentThread() != t->thread);
    t->exit = TRUE;
    KeSetEvent(&t->event, IO_NO_INCREMENT, FALSE);
    KeWaitForSingleObject(t->thread, Executive, KernelMode, FALSE,
                          NULL);
    ObDereferenceObject(t->thread);
    XmFreeMemory(t);
}

int
XmThreadWait(struct xm_thread *t)
{
    if (t->exit)
        return -1;
    KeWaitForSingleObject(&t->event,
                          Executive,
                          KernelMode,
                          FALSE,
                          NULL);
    KeClearEvent(&t->event);
    if (t->exit)
        return -1;
    else
        return 0;
}

typedef struct _XEN_WORKITEM    XEN_WORKITEM;
struct _XEN_WORKITEM {
    ULONG                   Magic;
    const CHAR              *Name;
    XEN_WORK_CALLBACK       Work;
    VOID                    *Context;
    LIST_ENTRY              List;
    LARGE_INTEGER           Start;
};
#define WORKITEM_MAGIC  0x02121996

static struct xm_thread *WorkItemThread;

static struct irqsafe_lock WorkItemDispatchLock;
static LIST_ENTRY  PendingWorkItems;
static XEN_WORKITEM *CurrentItem;

static VOID
XenWorkItemDump(
    IN  VOID    *Context
    )
{
    KIRQL       Irql;
    NTSTATUS    status;

    UNREFERENCED_PARAMETER(Context);

    status = try_acquire_irqsafe_lock(&WorkItemDispatchLock, &Irql);
    if (!NT_SUCCESS(status)) {
        TraceInternal(("Could not acquire WorkItemDispatchLock\n"));
        return;
    }

    if (CurrentItem != NULL) {
        LARGE_INTEGER   Now;
        ULONGLONG       Milliseconds;

        KeQuerySystemTime(&Now);
        Milliseconds = (Now.QuadPart - CurrentItem->Start.QuadPart) / 10000ull;

        TraceInternal(("Processing work item '%s' for %llums\n", CurrentItem->Name, Milliseconds));
    } else {
        TraceInternal(("No current work item\n"));
    }

    if (!IsListEmpty(&PendingWorkItems)) {
        PLIST_ENTRY Head;

        TraceInternal(("Pending work items:\n"));
        Head = PendingWorkItems.Flink;
        XM_ASSERT(Head != &PendingWorkItems);

        do {
            XEN_WORKITEM *Item;

            Item = CONTAINING_RECORD(Head, XEN_WORKITEM, List);
            TraceInternal(("%s\n", Item->Name));

            Head = Head->Flink;
        } while (Head != &PendingWorkItems);
    } else {
        TraceInternal(("No pending work items\n"));
    }

    release_irqsafe_lock(&WorkItemDispatchLock, Irql);
}

static NTSTATUS
XenWorkItemDispatch(
    IN  struct xm_thread    *pSelf,
    IN  VOID                *Argument
    )
{
    KIRQL                   Irql;

    UNREFERENCED_PARAMETER(Argument);

    while (XmThreadWait(pSelf) >= 0) {
        Irql = acquire_irqsafe_lock(&WorkItemDispatchLock);
        while (!IsListEmpty(&PendingWorkItems)) {
            PLIST_ENTRY Head;
            XEN_WORKITEM *Item;

            Head = RemoveHeadList(&PendingWorkItems);
            Item = CurrentItem = CONTAINING_RECORD(Head, XEN_WORKITEM, List);
            release_irqsafe_lock(&WorkItemDispatchLock, Irql);

            XM_ASSERT(CurrentItem->Magic == WORKITEM_MAGIC);

            KeQuerySystemTime(&Item->Start);

            TraceVerbose(("%s: invoking '%s'\n", __FUNCTION__, CurrentItem->Name));
            CurrentItem->Work(CurrentItem->Context);

            Irql = acquire_irqsafe_lock(&WorkItemDispatchLock);
            CurrentItem = NULL;
            release_irqsafe_lock(&WorkItemDispatchLock, Irql);

            XmFreeMemory(Item);

            Irql = acquire_irqsafe_lock(&WorkItemDispatchLock);
        }
        release_irqsafe_lock(&WorkItemDispatchLock, Irql);
    }

    TraceWarning(("%s: terminating.\n", __FUNCTION__));
    return STATUS_SUCCESS;
}

NTSTATUS
_XenQueueWork(
    IN  const CHAR          *Caller,
    IN  const CHAR          *Name,
    IN  XEN_WORK_CALLBACK   Work,
    IN  VOID                *Context
    )
{
    XEN_WORKITEM            *Item;
    KIRQL                   Irql;

    Item = XmAllocateZeroedMemory(sizeof(XEN_WORKITEM));
    if (!Item) {
        TraceError(("%s: %s() failed to queue %s\n", __FUNCTION__, Caller, Name));
        return STATUS_NO_MEMORY;
    }
    TraceVerbose(("%s: %s() queueing '%s'\n", __FUNCTION__, Caller, Name));

    Item->Magic = WORKITEM_MAGIC;
    Item->Name = Name;
    Item->Work = Work;
    Item->Context = Context;

    Irql = acquire_irqsafe_lock(&WorkItemDispatchLock);
    InsertTailList(&PendingWorkItems, &Item->List);
    release_irqsafe_lock(&WorkItemDispatchLock, Irql);

    KeSetEvent(&WorkItemThread->event, IO_NO_INCREMENT, FALSE);

    return STATUS_SUCCESS;
}

NTSTATUS
XenWorkItemInit(VOID)
{
    InitializeListHead(&PendingWorkItems);

    (VOID) EvtchnSetupDebugCallback(XenWorkItemDump, NULL);

    WorkItemThread = XmSpawnThread(XenWorkItemDispatch, NULL);
    if (!WorkItemThread) {
        TraceError(("Failed to spawn work item thread\n"));
        return STATUS_UNSUCCESSFUL;
    } else {
        return STATUS_SUCCESS;
    }
}

NTSTATUS
DriverEntry(PDRIVER_OBJECT dev, PUNICODE_STRING reg_path)
{
    UNREFERENCED_PARAMETER(dev);
    UNREFERENCED_PARAMETER(reg_path);

    TraceInfo(("%s: IRQL = %d\n", __FUNCTION__, KeGetCurrentIrql()));

    return STATUS_SUCCESS;
}
