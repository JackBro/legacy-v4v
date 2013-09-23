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

#include "ntifs.h"
#include "xenevtchn.h"
#include "pnp.h"
#include "base.h"

// These should not be here. Code requiring them will eventually
// move into xenutil
#include "../xenutil/hvm.h"
#include "../xenutil/hypercall.h"
#include "../xenutil/xenutl.h"

#pragma warning(push)
#pragma warning(disable: 4200)
#include <xen/hvm/params.h>
#include <xen/hvm/hvm_op.h>
#pragma warning(pop)

#define WPP_CONTROL_GUIDS \
  WPP_DEFINE_CONTROL_GUID( \
            XenevtchnWppGuid, \
			(01793C2A, 3CD0, 4C29, 95D3, DBA5FFD48332), \
			WPP_DEFINE_BIT(FLAG_DEBUG) \
			WPP_DEFINE_BIT(FLAG_VERBOSE) \
			WPP_DEFINE_BIT(FLAG_INFO) \
			WPP_DEFINE_BIT(FLAG_NOTICE) \
			WPP_DEFINE_BIT(FLAG_WARNING) \
			WPP_DEFINE_BIT(FLAG_ERROR) \
			WPP_DEFINE_BIT(FLAG_CRITICAL) \
			WPP_DEFINE_BIT(FLAG_PROFILE) \
            )

#include "xenevtchn.tmh"

#define MIN(a,b) ((a)<(b)?(a):(b))

//
// Dont care about unreferenced formal parameters here
//
#pragma warning( disable : 4100 )

#define XEVC_TAG 'CVEX'

PDRIVER_OBJECT XenevtchnDriver;

//
// Function prototypes
//
NTSTATUS
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
);

DRIVER_ADD_DEVICE XenevtchnAddDevice;
DRIVER_DISPATCH XenevtchnDispatchPnp;
DRIVER_DISPATCH XenevtchnDispatchWmi;
DRIVER_DISPATCH XenevtchnDispatchPower;

extern PULONG InitSafeBootMode;

UNICODE_STRING XenevtchnRegistryPath;

static VOID
XenevtchnUnload(
    IN PDRIVER_OBJECT DriverObject
    )
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    PVOID PathBuffer;
    USHORT PathLength;

    TraceInfo(("%s: IRQL = %d\n", __FUNCTION__, KeGetCurrentIrql()));

    XenevtchnDriver = DriverObject;

    if (!XmCheckXenutilVersionString(TRUE, XENUTIL_CURRENT_VERSION))
        return STATUS_REVISION_MISMATCH;

    if ((*InitSafeBootMode > 0) &&
        (!XenPVFeatureEnabled(DEBUG_HA_SAFEMODE)))
    {
        return STATUS_SUCCESS;
    }

    TraceDebug (("==>\n"));

    WPP_SYSTEMCONTROL(DriverObject); // Needed for win2k

    PathLength = RegistryPath->Length;
    if ((PathBuffer = ExAllocatePoolWithTag(NonPagedPool,
                                            PathLength,
                                            XEVC_TAG)) == NULL) {
        return STATUS_NO_MEMORY;
    }

    XenevtchnRegistryPath.Buffer = PathBuffer;
    XenevtchnRegistryPath.Length = PathLength;
    XenevtchnRegistryPath.MaximumLength = PathLength;

    RtlCopyUnicodeString(&XenevtchnRegistryPath, RegistryPath);

    SuspendPreInit();

    //
    // Cant unload becuase the binary patching engine uses 
    // PsSetLoadImageNotifyRoutine and it doesnt allow for unloading.
    //

    // DriverObject->DriverUnload = XenevtchnUnload;

    //
    // Define the driver's standard entry routines. 
    //

    DriverObject->DriverExtension->AddDevice = XenevtchnAddDevice;
    DriverObject->MajorFunction[IRP_MJ_PNP] = XenevtchnDispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_POWER] = XenevtchnDispatchPower;
    DriverObject->MajorFunction[IRP_MJ_SYSTEM_CONTROL] = 
        XenevtchnDispatchWmi;
    DriverObject->DriverUnload = XenevtchnUnload;

    TraceDebug (("<==\n"));

    return( STATUS_SUCCESS );
}

static VOID
XenEvtchnWppTrace(
    IN  XEN_TRACE_LEVEL Level,
    IN  CHAR            *Message
    )
{
    // Unfortunately DoTraceMessage is a macro and we don't necessarily know
    // what magic is being applied to the FLAG_ argument, so safest to use
    // a switch statement to map from Level to FLAG_ even though it is
    // somewhat inelegant.

    switch (Level) {
    case XenTraceLevelDebug:
        DoTraceMessage(FLAG_DEBUG, "%s", Message);
        break;

    case XenTraceLevelVerbose:
        DoTraceMessage(FLAG_VERBOSE, "%s", Message);
        break;

    case XenTraceLevelInfo:
        DoTraceMessage(FLAG_INFO, "%s", Message);
        break;

    case XenTraceLevelNotice:
        DoTraceMessage(FLAG_NOTICE, "%s", Message);
        break;

    case XenTraceLevelWarning:
        DoTraceMessage(FLAG_WARNING, "%s", Message);
        break;

    case XenTraceLevelError:
        DoTraceMessage(FLAG_ERROR, "%s", Message);
        break;

    case XenTraceLevelCritical:
        DoTraceMessage(FLAG_CRITICAL, "%s", Message);
        break;

    case XenTraceLevelBugCheck:
        DoTraceMessage(FLAG_CRITICAL, "%s", Message);
        break;

    case XenTraceLevelProfile:
        DoTraceMessage(FLAG_PROFILE, "%s", Message);
        break;
    }
}

static DEVICE_OBJECT    *XenevtchnFdo;

static const CHAR *
DeviceUsageName(
    IN  DEVICE_USAGE_NOTIFICATION_TYPE  Type
    )
{
#define _TYPE_NAME(_Type)               \
        case DeviceUsageType ## _Type:  \
            return #_Type;

    switch (Type) {
    _TYPE_NAME(Undefined);
    _TYPE_NAME(Paging);
    _TYPE_NAME(Hibernation);
    _TYPE_NAME(DumpFile);
    default:
        break;
    }

    return "UNKNOWN";

#undef  _TYPE_NAME
}

VOID
XenevtchnSetDeviceUsage(
    IN  DEVICE_USAGE_NOTIFICATION_TYPE  Type,
    IN  BOOLEAN                         On
    )
{
    IO_STACK_LOCATION   *StackLocation;
    IRP                 *Irp;
    KEVENT              Event;
    IO_STATUS_BLOCK     IoStatus;
    NTSTATUS            status;

    XM_ASSERT3U(KeGetCurrentIrql(), <, DISPATCH_LEVEL);

    if (XenevtchnFdo == NULL)
        return;

    KeInitializeEvent(&Event, NotificationEvent, FALSE);

    Irp = IoBuildSynchronousFsdRequest(IRP_MJ_PNP, XenevtchnFdo, NULL, 0, NULL, &Event, &IoStatus);

    StackLocation = IoGetNextIrpStackLocation(Irp);
    StackLocation->MinorFunction = IRP_MN_DEVICE_USAGE_NOTIFICATION;
    StackLocation->Parameters.UsageNotification.Type = Type;
    StackLocation->Parameters.UsageNotification.InPath = On;

    Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;

    TraceInfo(("%s: issuing IRP (%s,%s)\n", __FUNCTION__, 
               DeviceUsageName(Type),
               (On) ? "ON" : "OFF"));

    status = IoCallDriver(XenevtchnFdo, Irp);
    if (status == STATUS_PENDING)
        KeWaitForSingleObject(&Event,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);

    TraceInfo(("%s: completed IRP (%08x)\n", __FUNCTION__,
               IoStatus.Status));
}

NTSTATUS
XenevtchnAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT pdo
)
{
    PDEVICE_OBJECT fdo;
    NTSTATUS status;
    UNICODE_STRING name, linkName;
    PXENEVTCHN_DEVICE_EXTENSION pXendx;

    RtlInitUnicodeString(&name, XENEVTCHN_DEVICE_NAME);

    status = IoCreateDevice(DriverObject,
                sizeof(XENEVTCHN_DEVICE_EXTENSION),
                &name, FILE_DEVICE_UNKNOWN,
                FILE_DEVICE_SECURE_OPEN,
                FALSE,
                &fdo);
    if (status != STATUS_SUCCESS) {
        TraceError (("IoCreateDevice failed %d\n", status));
        return status;
    }

    ObReferenceObject(fdo); // We don't want this to go away
    XenevtchnFdo = fdo;

    TraceNotice(("%s: FDO = 0x%p\n", __FUNCTION__, XenevtchnFdo));

    RtlInitUnicodeString( &linkName, XENEVTCHN_FILE_NAME );
    status = IoCreateSymbolicLink( &linkName, &name );
    if ( !NT_SUCCESS( status )) {
        TraceError (("IoCreateSymbolicLink failed %d\n", status));
        IoDeleteDevice( fdo );
        return status;
    }

    pXendx = (PXENEVTCHN_DEVICE_EXTENSION)fdo->DeviceExtension;
    memset(pXendx, 0, sizeof(*pXendx));
    pXendx->DriverObject = DriverObject;
    pXendx->DeviceObject = fdo;
    pXendx->PhysicalDeviceObject = pdo;
    pXendx->LowerDeviceObject = IoAttachDeviceToDeviceStack(fdo, pdo);
    pXendx->Header.Signature = XENEVTCHN_FDO_SIGNATURE;
    InitializeListHead(&pXendx->xenbus_device_classes);
    InitializeListHead(&pXendx->devices);
    InitializeListHead(&pXendx->ActiveHandles);
    ExInitializeFastMutex(&pXendx->xenbus_lock);
    KeInitializeTimer(&pXendx->xenbus_timer);
    KeInitializeDpc(&pXendx->xenbus_dpc, XsRequestInvalidateBus,
                    pXendx);
    KeInitializeSpinLock(&pXendx->ActiveHandleLock);

    // Must start out disabled
    pXendx->UninstEnabled = FALSE;

    WPP_INIT_TRACING(fdo, &XenevtchnRegistryPath); // fdo ignored in XP or above
    SetWppTrace(XenEvtchnWppTrace);
    TraceNotice(("Initialized tracing provider\n"));

    fdo->Flags &= ~DO_DEVICE_INITIALIZING;

    return ( status );
}

//
// General dispatch point for Windows Plug and Play
// Irps.
//
NTSTATUS 
XenevtchnDispatchPnp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    PXENEVTCHN_DEVICE_EXTENSION pXevtdx = 
        (PXENEVTCHN_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    PEVTCHN_PNP_HANDLER handler = NULL;
    PCHAR name = NULL;
    NTSTATUS status;

    TraceDebug (("==> enter minor %d\n", stack->MinorFunction));
    if (pXevtdx->Header.Signature == XENEVTCHN_PDO_SIGNATURE) {
        TraceDebug (("(%d) - PDO\n", stack->MinorFunction));
        if (stack->MinorFunction >
            IRP_MN_QUERY_LEGACY_BUS_INFORMATION) {
            name = "???";
            handler = IgnoreRequest;
        } else {
            name = pnpInfo[stack->MinorFunction].name;
            handler = pnpInfo[stack->MinorFunction].pdoHandler;
        }
    } else if (pXevtdx->Header.Signature == XENEVTCHN_FDO_SIGNATURE) {
        TraceDebug (("(%d) - FDO\n", stack->MinorFunction));
        if (stack->MinorFunction >
            IRP_MN_QUERY_LEGACY_BUS_INFORMATION) {
            name = "???";
            handler = DefaultPnpHandler;
        } else {
            name = pnpInfo[stack->MinorFunction].name;
            handler = pnpInfo[stack->MinorFunction].fdoHandler;
        }
    } else {
        TraceBugCheck(("Bad signature %x\n", pXevtdx->Header.Signature));
    }

    status = handler(DeviceObject, Irp);
    TraceDebug (("<== (%s) = %d\n", name, status));

    return status;

}

NTSTATUS
XenevtchnDispatchWmi(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
)
{
    /* We don't support WMI, so just pass it on down the stack. */
    PXENEVTCHN_DEVICE_EXTENSION pXevtdx =
        (PXENEVTCHN_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    TraceDebug (("DispatchWmi.\n"));
    if (pXevtdx->Header.Signature == XENEVTCHN_PDO_SIGNATURE) {
        TraceDebug (("DispatchWmi PDO.\n"));
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return STATUS_NOT_SUPPORTED;
    } else {
        IoSkipCurrentIrpStackLocation(Irp);
        return IoCallDriver(pXevtdx->LowerDeviceObject, Irp);
    }
}

NTSTATUS
DllInitialize(PUNICODE_STRING RegistryPath)
{
    TraceInfo(("%s\n", __FUNCTION__));

    return STATUS_SUCCESS;
}
