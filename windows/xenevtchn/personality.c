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

#include "wdm.h"
#include "ntddk.h"
#include "xenevtchn.h"
#include "base.h"

#include "ntstrsafe.h"

#define XEMN_TAG 'NMEX'

#define CONTROLSET L"ControlSet"
#define HOSTNAME_COMPUTERNAME L"\\Registry\\Machine\\SYSTEM\\%ws\\Control\\ComputerName\\ComputerName"
#define HOSTNAME_TCPIP L"\\Registry\\Machine\\SYSTEM\\%ws\\Services\\Tcpip\\Parameters"

static NTSTATUS
UpdateMachineNameValues(
    PCWSTR ControlSetValue,
    PUNICODE_STRING HostName
    );

NTSTATUS
UpdateMachineName()
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING regbase;
    HANDLE hKey = NULL;
    WCHAR *computerName = NULL;
    char *hostnameAnsi = NULL;
    UNICODE_STRING nameUnicode;
    ANSI_STRING nameAnsi;
    ULONG index;
    ULONG cbKeyInformation = 0;
    PKEY_BASIC_INFORMATION keyInformation = NULL;

    XM_ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

    status = xenbus_read(XBT_NIL, "vm-data/hostname", &hostnameAnsi);
    if (!NT_SUCCESS(status)) {
        goto clean;
    }

    RtlInitAnsiString(&nameAnsi, hostnameAnsi);

    computerName = ExAllocatePoolWithTag(NonPagedPool, (nameAnsi.Length + 1) * sizeof(WCHAR), XEMN_TAG);
    if (computerName == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto clean;
    }

    nameUnicode.Buffer = computerName;
    nameUnicode.Length = 0;
    nameUnicode.MaximumLength = (nameAnsi.Length + 1) * sizeof(WCHAR);

    status = RtlAnsiStringToUnicodeString(&nameUnicode, &nameAnsi, FALSE);
    if (!NT_SUCCESS(status)) {
        goto clean;
    }

    computerName[nameUnicode.Length / sizeof(WCHAR)] = UNICODE_NULL;

    //
    // To play things on the safe side, instead of updating only the
    // CurrentControlSet, instead update all of the ControlSetXxx keys.
    // Note that the CurrentControlSet key simply is a link to one of the
    // ControlSetXxx keys.
    // By updating all of the ControlSetXxx keys the machine name change
    // will stick even if the user chooses LastKnownGood.
    //
    RtlInitUnicodeString(&regbase, L"\\Registry\\Machine\\SYSTEM");
    InitializeObjectAttributes(&oa, 
                               &regbase,
                               OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL, 
                               NULL);
    status = ZwOpenKey(&hKey, KEY_READ | KEY_ENUMERATE_SUB_KEYS, &oa);
    if (NT_SUCCESS(status)) {
        index = 0;
        do {
            status = ZwEnumerateKey(hKey,
                                    index,
                                    KeyBasicInformation,
                                    NULL,
                                    0,
                                    &cbKeyInformation);
            if ((status == STATUS_BUFFER_OVERFLOW) ||
                (status == STATUS_BUFFER_TOO_SMALL)) {
                /* + sizeof(WCHAR) because we want it
                 * nul-terminated. */
                keyInformation = ExAllocatePoolWithTag(NonPagedPool, cbKeyInformation + sizeof(WCHAR), XEMN_TAG);
                if (keyInformation == NULL) {
                    status = STATUS_INSUFFICIENT_RESOURCES;
                    goto clean;
                }
                memset(keyInformation, 0, cbKeyInformation + sizeof(WCHAR));

                status = ZwEnumerateKey(hKey,
                                        index,
                                        KeyBasicInformation,
                                        keyInformation,
                                        cbKeyInformation,
                                        &cbKeyInformation);
                if (NT_SUCCESS(status)) {
                    if (!wcsncmp(keyInformation->Name, CONTROLSET, wcslen(CONTROLSET))) {
                        status = UpdateMachineNameValues(keyInformation->Name, &nameUnicode);
                    }
                }

                if (keyInformation != NULL) {
                    ExFreePool(keyInformation);
                }
            }

            if (status != STATUS_NO_MORE_ENTRIES &&
                !NT_SUCCESS(status)) {
                TraceWarning(("%x enumerating HKLM\\SYSTEM", status));
                break;
            }

            index++;
        } while (status != STATUS_NO_MORE_ENTRIES);
    }

clean:
    if (hKey != NULL) {
        ZwClose(hKey);
    }
    if (hostnameAnsi != NULL) {
        XmFreeMemory(hostnameAnsi);
    }
    if (computerName != NULL) {
        ExFreePool(computerName);
    }

    return status;
}

static NTSTATUS
UpdateSingleMachineNameValue(
    PCWSTR RegPath,
    PCWSTR Value,
    PUNICODE_STRING HostName
    )
{
    NTSTATUS status;
    HANDLE hKey;
    UNICODE_STRING regbase;
    UNICODE_STRING valueName;
    OBJECT_ATTRIBUTES oa;

    RtlInitUnicodeString(&regbase, RegPath);
    InitializeObjectAttributes(&oa, 
                               &regbase,
                               OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE,
                               NULL, 
                               NULL);
    status = ZwOpenKey(&hKey, KEY_WRITE, &oa);
    if (NT_SUCCESS(status)) {
        RtlInitUnicodeString(&valueName, Value);
        status = ZwSetValueKey(hKey,
                               &valueName,
                               0,
                               REG_SZ,
                               (PVOID)HostName->Buffer,
                               HostName->Length + sizeof(WCHAR));
    }
    ZwClose(hKey);
 
    return status;
}

static NTSTATUS
UpdateMachineNameValues(
    PCWSTR ControlSetValue,
    PUNICODE_STRING HostName
    )
{
    NTSTATUS status;
    size_t cbRegPath;
    WCHAR *regPath;
    size_t cbControlSetValue;

    cbControlSetValue = wcslen(ControlSetValue) * sizeof(WCHAR);

    cbRegPath = sizeof(HOSTNAME_COMPUTERNAME) + cbControlSetValue + sizeof(WCHAR);

    regPath = ExAllocatePoolWithTag(NonPagedPool, cbRegPath, XEMN_TAG);
    if (regPath == NULL) {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto clean;
    }
    
    status = RtlStringCbPrintfW(regPath, 
                                cbRegPath, 
                                HOSTNAME_COMPUTERNAME, 
                                ControlSetValue);
    if (!NT_SUCCESS(status)) {
        goto clean;
    }
    UpdateSingleMachineNameValue(regPath, L"ComputerName", HostName);

    status = RtlStringCbPrintfW(regPath, 
                                cbRegPath, 
                                HOSTNAME_TCPIP, 
                                ControlSetValue);
    if (!NT_SUCCESS(status)) {
        goto clean;
    }
    status = UpdateSingleMachineNameValue(regPath, L"NV Hostname", HostName);

clean:
    if (regPath != NULL) {
        ExFreePool(regPath);
    }

    return status;
}
