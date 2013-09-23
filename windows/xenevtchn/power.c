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

#include "xenevtchn.h"
#include "base.h"
#include "xsapi.h"

#include "ntstrsafe.h"

#include "../xenutil/hvm.h"
#include "../xenutil/xenutl.h"

static const CHAR *
PowerSystemStateName(
    IN  SYSTEM_POWER_STATE  State
    )
{
#define _POWER_SYSTEM_NAME(_State)                               \
        case PowerSystem ## _State:                              \
            return #_State;

    switch (State) {
    _POWER_SYSTEM_NAME(Unspecified);
    _POWER_SYSTEM_NAME(Working);
    _POWER_SYSTEM_NAME(Sleeping1);
    _POWER_SYSTEM_NAME(Sleeping2);
    _POWER_SYSTEM_NAME(Sleeping3);
    _POWER_SYSTEM_NAME(Hibernate);
    _POWER_SYSTEM_NAME(Shutdown);
    _POWER_SYSTEM_NAME(Maximum);
    default:
        break;
    }

    return ("UNKNOWN");
#undef  _POWER_SYSTEM_NAME
}

static const CHAR *
PowerDeviceStateName(
    IN  DEVICE_POWER_STATE  State
    )
{
#define _POWER_DEVICE_NAME(_State)                               \
        case PowerDevice ## _State:                              \
            return #_State;

    switch (State) {
    _POWER_DEVICE_NAME(Unspecified);
    _POWER_DEVICE_NAME(D0);
    _POWER_DEVICE_NAME(D1);
    _POWER_DEVICE_NAME(D2);
    _POWER_DEVICE_NAME(D3);
    _POWER_DEVICE_NAME(Maximum);
    default:
        break;
    }

    return ("UNKNOWN");
#undef  _POWER_DEVICE_NAME
}

static const CHAR *
PowerShutdownTypeName(
    IN  POWER_ACTION    Type
    )
{
#define _POWER_ACTION_NAME(_Type)                               \
        case PowerAction ## _Type:                              \
            return #_Type;

    switch (Type) {
    _POWER_ACTION_NAME(None);
    _POWER_ACTION_NAME(Reserved);
    _POWER_ACTION_NAME(Sleep);
    _POWER_ACTION_NAME(Hibernate);
    _POWER_ACTION_NAME(Shutdown);
    _POWER_ACTION_NAME(ShutdownReset);
    _POWER_ACTION_NAME(ShutdownOff);
    _POWER_ACTION_NAME(WarmEject);
    default:
        break;
    }

    return ("UNKNOWN");
#undef  _POWER_ACTION_NAME
}

static NTSTATUS
XenevtchnFdoSystemPowerIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    UNREFERENCED_PARAMETER(DeviceObject);
    UNREFERENCED_PARAMETER(Context);

    TraceInfo(("%s: completing power IRP\n", __FUNCTION__));
    PoStartNextPowerIrp(Irp);

    return STATUS_SUCCESS;
}

static NTSTATUS
XenevtchnHandleFdoSystemPowerIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    PXENEVTCHN_DEVICE_EXTENSION pXevtdx = 
        (PXENEVTCHN_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS status;
    static BOOLEAN hibernated;
    static BOOLEAN suspended;

    if (XenPVFeatureEnabled(DEBUG_NO_PARAVIRT))
        goto done;

    switch (stack->MinorFunction) {
    case IRP_MN_SET_POWER:
        TraceNotice(("%s: Setting power: %d:%s (%d:%s)\n", __FUNCTION__,
                        stack->Parameters.Power.State.SystemState,
                        PowerSystemStateName(stack->Parameters.Power.State.SystemState),
                        stack->Parameters.Power.ShutdownType,
                        PowerShutdownTypeName(stack->Parameters.Power.ShutdownType)));

        if (stack->Parameters.Power.State.SystemState == PowerSystemHibernate) {
            XenSetSystemPowerState(PowerSystemHibernate);
            KillSuspendThread();
            hibernated = TRUE;
        } else if (stack->Parameters.Power.State.SystemState == PowerSystemSleeping3) {
            XenSetSystemPowerState(PowerSystemSleeping3);
            XmPrepForS3();
            suspended = TRUE;
        } else if (stack->Parameters.Power.State.SystemState == PowerSystemWorking) {
            if (hibernated) {
                PnpRecoverFromHibernate(pXevtdx);
                hibernated = FALSE;
            } else if (suspended) {
                XmRecoverFromS3();
                suspended = FALSE;
            }
            XenSetSystemPowerState(PowerSystemWorking);
        }
        break;

    case IRP_MN_QUERY_POWER:
        TraceNotice(("%s: Query power: %d:%s (%d:%s)\n", __FUNCTION__, 
                        stack->Parameters.Power.State.SystemState,
                        PowerSystemStateName(stack->Parameters.Power.State.SystemState),
                        stack->Parameters.Power.ShutdownType,
                        PowerShutdownTypeName(stack->Parameters.Power.ShutdownType)));

        if (stack->Parameters.Power.State.SystemState == PowerSystemHibernate) {
            RTL_OSVERSIONINFOEXW verInfo;
            XenutilGetVersionInfo(&verInfo);
            status = STATUS_SUCCESS;
            if (verInfo.dwMajorVersion == 5 &&
                verInfo.dwMinorVersion == 0) {

                TraceWarning(("Hibernation disabled on Windows 2000.\n"));
                status = STATUS_INSUFFICIENT_RESOURCES;
                Irp->IoStatus.Status = status;
                IoCompleteRequest(Irp, IO_NO_INCREMENT);
                return status;
            }

        }
        break;

    default:
        break;
    }

done:
    IoMarkIrpPending(Irp);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, XenevtchnFdoSystemPowerIrpCompletion, NULL, TRUE, TRUE, TRUE);

    PoCallDriver(pXevtdx->LowerDeviceObject, Irp);

    return STATUS_PENDING;
}

static NTSTATUS
XenevtchnFdoDevicePowerIrpCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp,
    IN PVOID Context
    )
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    PXENEVTCHN_DEVICE_EXTENSION pXevtdx = 
        (PXENEVTCHN_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    UNREFERENCED_PARAMETER(Context);

    if (pXevtdx->PowerState >= stack->Parameters.Power.State.DeviceState) {
        TraceNotice(("%s: Powering up from %d:%s to: %d:%s (%d:%s)\n", __FUNCTION__, 
                     pXevtdx->PowerState,
                     PowerDeviceStateName(pXevtdx->PowerState),
                     stack->Parameters.Power.State.DeviceState,
                     PowerDeviceStateName(stack->Parameters.Power.State.DeviceState),
                     stack->Parameters.Power.ShutdownType,
                     PowerShutdownTypeName(stack->Parameters.Power.ShutdownType)));

        PoSetPowerState(DeviceObject,
                        DevicePowerState,
                        stack->Parameters.Power.State);
        pXevtdx->PowerState = stack->Parameters.Power.State.DeviceState;
    }

    TraceInfo(("%s: completing power IRP\n", __FUNCTION__));
    PoStartNextPowerIrp(Irp);

    return STATUS_SUCCESS;
}

static NTSTATUS
XenevtchnHandleFdoDevicePowerIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    PXENEVTCHN_DEVICE_EXTENSION pXevtdx = 
        (PXENEVTCHN_DEVICE_EXTENSION)DeviceObject->DeviceExtension;

    if (pXevtdx->PowerState < stack->Parameters.Power.State.DeviceState) {
        TraceNotice(("%s: Powering down from %d:%s to: %d:%s (%d:%s)\n", __FUNCTION__, 
                     pXevtdx->PowerState,
                     PowerDeviceStateName(pXevtdx->PowerState),
                     stack->Parameters.Power.State.DeviceState,
                     PowerDeviceStateName(stack->Parameters.Power.State.DeviceState),
                     stack->Parameters.Power.ShutdownType,
                     PowerShutdownTypeName(stack->Parameters.Power.ShutdownType)));

        PoSetPowerState(DeviceObject,
                        DevicePowerState,
                        stack->Parameters.Power.State);
        pXevtdx->PowerState = stack->Parameters.Power.State.DeviceState;
    }

    IoMarkIrpPending(Irp);
    IoCopyCurrentIrpStackLocationToNext(Irp);
    IoSetCompletionRoutine(Irp, XenevtchnFdoDevicePowerIrpCompletion, NULL, TRUE, TRUE, TRUE);

    PoCallDriver(pXevtdx->LowerDeviceObject, Irp);

    return STATUS_PENDING;
}

static NTSTATUS
XenevtchnHandlePdoPowerIrp(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS status;

    switch (stack->MinorFunction) {
    case IRP_MN_SET_POWER:
        if (stack->Parameters.Power.Type == DevicePowerState) {
            TraceNotice(("%s: Setting power: %d:%s (%d:%s)\n", __FUNCTION__, 
                          stack->Parameters.Power.State.DeviceState,
                          PowerDeviceStateName(stack->Parameters.Power.State.DeviceState),
                          stack->Parameters.Power.ShutdownType,
                          PowerShutdownTypeName(stack->Parameters.Power.ShutdownType)));

            PoSetPowerState(DeviceObject,
                            DevicePowerState,
                            stack->Parameters.Power.State);
        }

        status = STATUS_SUCCESS;
        break;

    case IRP_MN_QUERY_POWER:
        status = STATUS_SUCCESS;
        break;

    default:
        status = STATUS_NOT_SUPPORTED;
        break;
    }

    PoStartNextPowerIrp(Irp);

    Irp->IoStatus.Status = status;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return status;
}

NTSTATUS
XenevtchnDispatchPower(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
{
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    PXENEVTCHN_DEVICE_EXTENSION pXevtdx = 
        (PXENEVTCHN_DEVICE_EXTENSION)DeviceObject->DeviceExtension;
    NTSTATUS status;

    if (pXevtdx->Header.Signature == XENEVTCHN_FDO_SIGNATURE) {
        if (stack->Parameters.Power.Type == SystemPowerState) {
            status = XenevtchnHandleFdoSystemPowerIrp(DeviceObject, Irp);
        } else {
            XM_ASSERT3U(stack->Parameters.Power.Type == SystemPowerState, ==, DevicePowerState);
            status = XenevtchnHandleFdoDevicePowerIrp(DeviceObject, Irp);
        }
    } else {
        status = XenevtchnHandlePdoPowerIrp(DeviceObject, Irp);
    }

    return status;
}
