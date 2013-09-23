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

#ifndef _PNP_H_
#define _PNP_H_

//
// PnP handler function prototypes.
//
NTSTATUS
DefaultPnpHandler(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
);

NTSTATUS
IgnoreRequest(
    PDEVICE_OBJECT pdo,
    PIRP Irp
);

typedef
NTSTATUS
(*PEVTCHN_PNP_HANDLER)(
    PDEVICE_OBJECT DeviceObject,
    PIRP Irp
);

//
// PnP function vector table
//
#define XENEVT_MAX_PNP_FN   24
typedef struct  {
    PEVTCHN_PNP_HANDLER fdoHandler;
    PEVTCHN_PNP_HANDLER pdoHandler;
    PCHAR name;
} PNP_INFO, *PPNP_INFO;

extern PNP_INFO pnpInfo[];

DEFINE_GUID(GUID_XENBUS_BUS_TYPE, 0x8a1b2f56, 0x5023, 0x473e, 0x8e,
        0x06, 0x56, 0x74, 0x2f, 0xb0, 0x20, 0x28);

#endif // _PNP_H_
