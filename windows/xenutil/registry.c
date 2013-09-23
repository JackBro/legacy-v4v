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

NTSTATUS
XenOpenRegistryKey(
    IN  const WCHAR     *Path,
    IN  ACCESS_MASK     Access,
    OUT PHANDLE         pKey
    )
{
    UNICODE_STRING      String;
    OBJECT_ATTRIBUTES   Attributes;
    HANDLE              Key;
    NTSTATUS            status;

    RtlInitUnicodeString(&String, Path);

    InitializeObjectAttributes(&Attributes,
                               &String,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);
    status = ZwOpenKey(&Key,
                       Access,
                       &Attributes);
    if (!NT_SUCCESS(status))
        goto fail1;

    *pKey = Key;
    return STATUS_SUCCESS;

fail1:
    TraceError(("%s(%ws): fail1 (%08x)\n", __FUNCTION__, Path, status));

    return status;
}

NTSTATUS
XenReadRegistryValue(
    IN  const WCHAR                     *Path,
    IN  const WCHAR                     *Name,
    OUT PKEY_VALUE_PARTIAL_INFORMATION  *pInfo
    )
{
    UNICODE_STRING                      String;
    HANDLE                              Key;
    PVOID                               Buffer;
    ULONG                               Size;
    NTSTATUS                            status;
            
    status = XenOpenRegistryKey(Path, KEY_READ, &Key);    
    if (!NT_SUCCESS(status))
        goto fail1;

    RtlInitUnicodeString(&String, Name);

    Buffer = NULL;
    Size = 0;

    for (;;) {
        status = ZwQueryValueKey(Key,
                                 &String,
                                 KeyValuePartialInformation,
                                 Buffer,
                                 Size,
                                 &Size);
        if (NT_SUCCESS(status))
            break;

        if (status == STATUS_OBJECT_NAME_NOT_FOUND)
            goto not_found;

        if (status != STATUS_BUFFER_OVERFLOW &&
            status != STATUS_BUFFER_TOO_SMALL)
            goto fail2;

        if (Buffer != NULL)
            ExFreePoolWithTag(Buffer, XUTIL_TAG);

        Buffer = ExAllocatePoolWithTag(NonPagedPool, Size, XUTIL_TAG);

        status = STATUS_INSUFFICIENT_RESOURCES;
        if (Buffer == NULL)
            goto fail3;

        RtlZeroMemory(Buffer, Size);
    }
        
    ZwClose(Key);

    *pInfo = Buffer;
    return STATUS_SUCCESS;

not_found:
    return STATUS_OBJECT_NAME_NOT_FOUND;

fail3:
    TraceError(("%s: fail2\n", __FUNCTION__));

fail2:
    TraceError(("%s: fail2\n", __FUNCTION__));

fail1:

    TraceError(("%s(%ws\\%ws): fail1 (%08x)\n", __FUNCTION__, Path, Name, status));

    return status;
}

