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

/* The NDIS HCTs require that all network cards register as being
   capable of 64 bit scatter-gather DMA.  This requires that the
   underlying PDO be able to construct a DMA_ADAPTER object via the
   standard bus interface.  This is our implementation of such a that.
   It's not actually terribly useful, because we don't really have a
   DMA adapter to control, but it keeps driver verifier happy. */
#include "ntddk.h"
#include "wdm.h"
#include "xenevtchn.h"
#include "base.h"

/* Dont care about unreferenced formal parameters here */
#pragma warning( disable : 4100 )

/* Dont care about missing return statements here */
#pragma warning( disable : 4716 )

#pragma warning( disable : 4201 )

struct _OBJECT_HEADER {
    LONG            PointerCount;
    union {
        LONG        HandleCount;
        PVOID       NextToFree;
    };
    POBJECT_TYPE    Type;
    UCHAR           NameInfoOffset;
    UCHAR           HandleInfoOffset;
    UCHAR           QuotaInfoOffset;
    UCHAR           Flags;
    union {
        PVOID       ObjectCreateInfo;
        PVOID       QuotaBlockCharged;
    };
    PVOID           SecurityDescriptor;
};

struct _ADAPTER_OBJECT {
    DMA_ADAPTER             DmaHeader;
    struct _ADAPTER_OBJECT  *MasterAdapter;
    ULONG                   MapRegistersPerChannel;
    PVOID                   AdapterBaseVa;
    PVOID                   MapRegisterBase;
    ULONG                   NumberOfMapRegisters;
    ULONG                   CommittedMapRegisters;
    PVOID                   CurrentWcb;
    KDEVICE_QUEUE           ChannelWaitQueue;
    PKDEVICE_QUEUE          RegisterWaitQueue;
    LIST_ENTRY              AdapterQueue;
    KSPIN_LOCK              SpinLock;
    PRTL_BITMAP             MapRegisters;
    PUCHAR                  PagePort;
    UCHAR                   ChannelNumber;
    UCHAR                   AdapterNumber;
    USHORT                  DmaPortAddress;
    UCHAR                   AdapterMode;
    BOOLEAN                 NeedsMapRegisters;
    BOOLEAN                 MasterDevice;
    UCHAR                   Width16Bits;
    BOOLEAN                 ScatterGather;
    BOOLEAN                 IgnoreCount;
    BOOLEAN                 Dma32BitAddresses;
    BOOLEAN                 Dma64BitAddresses;
    LIST_ENTRY              AdapterList;
};

struct xenbus_dma_adapter {
    struct _OBJECT_HEADER       Header;
    struct _ADAPTER_OBJECT      Object;
    struct xenbus_dma_adapter   *Next;
};

static struct xenbus_dma_adapter *
HeadPendingAdapterRelease;

static VOID
FreeAdapter(
    IN  struct _ADAPTER_OBJECT *Object
    )
{
    struct xenbus_dma_adapter   *xda =
        CONTAINING_RECORD(Object, struct xenbus_dma_adapter, Object);

    if (InterlockedDecrement(&xda->Header.PointerCount) == 1) {
        TraceNotice(("%s: release xda %p\n", __FUNCTION__, xda));
        XmFreeMemory(xda);
    } else {
        TraceNotice(("%s: queue xda %p for later release, refcount %d\n",
                     xda, xda->Header.PointerCount));
        do {
            xda->Next = HeadPendingAdapterRelease;
        } while (InterlockedCompareExchangePointer(&HeadPendingAdapterRelease,
                                                   xda,
                                                   xda->Next) != xda->Next);
    }
}

static void
xenbus_put_dma_adapter(
    IN  PDMA_ADAPTER            DmaHeader)
{
    struct _ADAPTER_OBJECT      *Object = 
        CONTAINING_RECORD(DmaHeader, struct _ADAPTER_OBJECT, DmaHeader);
    RTL_OSVERSIONINFOEXW verInfo;

    TraceNotice(("%s(%p)\n", __FUNCTION__, DmaHeader));

    XenutilGetVersionInfo(&verInfo);

    if (verInfo.dwMajorVersion == 5 &&
        verInfo.dwMinorVersion == 1) { // XP
        ObDereferenceObject(Object);
    } else {
        FreeAdapter(Object);
    }
}

static PVOID
xenbus_allocate_common_buffer(PDMA_ADAPTER pda, ULONG length,
                              PPHYSICAL_ADDRESS addr,
                              BOOLEAN cacheable)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static void
xenbus_free_common_buffer(PDMA_ADAPTER pda, ULONG length,
                          PHYSICAL_ADDRESS addr, PVOID vaddr,
                          BOOLEAN cacheable)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static NTSTATUS
xenbus_allocate_adapter_channel(PDMA_ADAPTER pda,
                                PDEVICE_OBJECT pdo,
                                ULONG nr,
                                PDRIVER_CONTROL pdc,
                                PVOID ctxt)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static BOOLEAN
xenbus_flush_adapter_buffers(PDMA_ADAPTER pda, PMDL mdl,
                             PVOID map_reg_base,
                             PVOID va,
                             ULONG length,
                             BOOLEAN write)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static VOID
xenbus_free_adapter_channel(PDMA_ADAPTER pda)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static VOID
xenbus_free_map_registers(PDMA_ADAPTER pda,
                          PVOID base,
                          ULONG nr)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static PHYSICAL_ADDRESS
xenbus_map_transfer(PDMA_ADAPTER pda,
                    PMDL mdl,
                    PVOID map_base,
                    PVOID va,
                    PULONG length,
                    BOOLEAN write)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static ULONG
xenbus_get_dma_alignment(PDMA_ADAPTER pda)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static ULONG
xenbus_read_dma_counter(PDMA_ADAPTER pda)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static VOID
xenbus_put_scatter_gather_list(PDMA_ADAPTER pda,
                               PSCATTER_GATHER_LIST psgl,
                               BOOLEAN write)
{
    return;
}

static NTSTATUS
xenbus_calculate_scatter_gather_list(PDMA_ADAPTER pda,
                                     PMDL mdl,
                                     PVOID va,
                                     ULONG length,
                                     PULONG size,
                                     PULONG pnr_regs)
{
    unsigned nr_sges = 0;

    if (mdl == NULL) {
        nr_sges = ADDRESS_AND_SIZE_TO_SPAN_PAGES(va, length);
    } else {
        while (mdl->ByteCount == 0)
            mdl = mdl->Next;

        while (mdl != NULL) {
            nr_sges += ADDRESS_AND_SIZE_TO_SPAN_PAGES(mdl->ByteOffset,
                                                      mdl->ByteCount);
            mdl = mdl->Next;
        }
    }
    *size =
        sizeof(SCATTER_GATHER_LIST) +
        (nr_sges - 1) * sizeof(SCATTER_GATHER_ELEMENT);
    if (pnr_regs)
        *pnr_regs = 1;
    return STATUS_SUCCESS;
}

static NTSTATUS
add_range(PSCATTER_GATHER_LIST psgl,
          PHYSICAL_ADDRESS start,
          ULONG length,
          PVOID buf,
          ULONG buf_len)
{
    PSCATTER_GATHER_ELEMENT psge;
    PHYSICAL_ADDRESS old_end;

    if ( (ULONG_PTR)(&psgl->NumberOfElements + 1) >
         (ULONG_PTR)buf + buf_len )
        return STATUS_BUFFER_TOO_SMALL;

    psge = &psgl->Elements[psgl->NumberOfElements - 1];
    if ( (ULONG_PTR)(psge + 1) > (ULONG_PTR)buf + buf_len )
        return STATUS_BUFFER_TOO_SMALL;
    old_end = psge->Address;
    old_end.QuadPart += psge->Length;
    if (old_end.QuadPart == start.QuadPart) {
        psge->Length += length;
    } else {
        /* Create another SG segment */
        psgl->NumberOfElements++;
        psge = &psgl->Elements[psgl->NumberOfElements - 1];
        if ( (ULONG_PTR)(psge + 1) > (ULONG_PTR)buf + buf_len )
            return STATUS_BUFFER_TOO_SMALL;
        psge->Address = start;
        psge->Length = length;
    }
    return STATUS_SUCCESS;
}

#define safe_assign(a, b) do {                              \
        if ( (ULONG_PTR)(&(a) + 1) >                        \
             (ULONG_PTR)buf + buf_len ) {                   \
            res = STATUS_BUFFER_TOO_SMALL;                  \
            goto err;                                       \
        } else {                                            \
            (a) = (b);                                      \
        }                                                   \
    } while (0);

static NTSTATUS
_xenbus_build_scatter_gather_list(PDMA_ADAPTER pda,
                                  PDEVICE_OBJECT pdo,
                                  PMDL mdl,
                                  PVOID va,
                                  ULONG length,
                                  PDRIVER_LIST_CONTROL pdlc,
                                  PVOID ctxt,
                                  BOOLEAN write,
                                  PVOID buf,
                                  ULONG buf_len,
                                  BOOLEAN release)
{
    PSCATTER_GATHER_LIST psgl = buf;
    ULONG cur_mdl_index;
    ULONG pfns_in_mdl;
    PPFN_NUMBER pfns;
    PHYSICAL_ADDRESS start;
    ULONG i;
    NTSTATUS res;

    res = STATUS_SUCCESS;

    safe_assign(psgl->Reserved, release);

    while (mdl->ByteCount == 0)
        mdl = mdl->Next;

    safe_assign(psgl->NumberOfElements, 1);
    cur_mdl_index = 0;

    pfns = (PPFN_NUMBER)(mdl + 1);
    safe_assign(psgl->Elements[0].Address.QuadPart,
                ((ULONGLONG)pfns[0] << PAGE_SHIFT) + mdl->ByteOffset);
    safe_assign(psgl->Elements[0].Length, 0);

    while (mdl) {
        pfns = (PPFN_NUMBER)(mdl + 1);
        pfns_in_mdl =
            (mdl->ByteCount + mdl->ByteOffset + PAGE_SIZE - 1) / PAGE_SIZE;
        start.QuadPart =
            ((ULONGLONG)pfns[0] << PAGE_SHIFT) + mdl->ByteOffset;
        if (pfns_in_mdl > 1) {
            res = add_range(psgl, start,
                            (ULONG)(PAGE_SIZE -
                                    (start.QuadPart & (PAGE_SIZE - 1))),
                            buf,
                            buf_len);
            if (res != STATUS_SUCCESS)
                goto err;
            for (i = 1; i < pfns_in_mdl - 1; i++) {
                start.QuadPart = (ULONGLONG)pfns[i] << PAGE_SHIFT;
                res = add_range(psgl, start, PAGE_SIZE, buf, buf_len);
                if (res != STATUS_SUCCESS)
                    goto err;
            }

            start.QuadPart = (ULONGLONG)pfns[i] << PAGE_SHIFT;
            res = add_range(psgl, start,
                            (mdl->ByteCount + mdl->ByteOffset) &
                             (PAGE_SIZE - 1),
                            buf,
                            buf_len);
        } else if (pfns_in_mdl == 1) {
            res = add_range(psgl, start, mdl->ByteCount, buf, buf_len);
        }
        if (res != STATUS_SUCCESS)
            goto err;
        mdl = mdl->Next;
    }

    pdlc(pdo, pdo->CurrentIrp, psgl, ctxt);
    return STATUS_SUCCESS;

err:
    return res;
}

#undef safe_assign

static NTSTATUS
xenbus_build_scatter_gather_list(PDMA_ADAPTER pda,
                                 PDEVICE_OBJECT pdo,
                                 PMDL mdl,
                                 PVOID va,
                                 ULONG length,
                                 PDRIVER_LIST_CONTROL pdlc,
                                 PVOID ctxt,
                                 BOOLEAN write,
                                 PVOID buf,
                                 ULONG buf_len)
{
    return _xenbus_build_scatter_gather_list(pda, pdo, mdl, va, length,
                                             pdlc, ctxt, write, buf,
                                             buf_len, FALSE);
}

static NTSTATUS
xenbus_get_scatter_gather_list(PDMA_ADAPTER pda,
                               PDEVICE_OBJECT pdo,
                               PMDL mdl,
                               PVOID va,
                               ULONG length,
                               PDRIVER_LIST_CONTROL pdlc,
                               PVOID ctxt,
                               BOOLEAN write)
{
    ULONG size;
    void *buf;
    NTSTATUS status;

    xenbus_calculate_scatter_gather_list(pda, mdl, va, length,
                                         &size, NULL);
    buf = ExAllocatePoolWithTag(NonPagedPool, size, 'xbsg');
    if (!buf)
        return STATUS_INSUFFICIENT_RESOURCES;
    status = _xenbus_build_scatter_gather_list(pda, pdo, mdl,
                                               va, length, pdlc,
                                               ctxt, write,
                                               buf, size,
                                               FALSE);
    return status;
}

static NTSTATUS
xenbus_build_mdl_from_scatter_gather_list(PDMA_ADAPTER pda,
                                          PSCATTER_GATHER_LIST psgl,
                                          PMDL mdl,
                                          PMDL *tmdl)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static DMA_OPERATIONS
xenbus_dma_operations = {
    sizeof(DMA_OPERATIONS), /* Size */
    xenbus_put_dma_adapter,
    xenbus_allocate_common_buffer,
    xenbus_free_common_buffer,
    xenbus_allocate_adapter_channel,
    xenbus_flush_adapter_buffers,
    xenbus_free_adapter_channel,
    xenbus_free_map_registers,
    xenbus_map_transfer,
    xenbus_get_dma_alignment,
    xenbus_read_dma_counter,
    xenbus_get_scatter_gather_list,
    xenbus_put_scatter_gather_list,
    xenbus_calculate_scatter_gather_list,
    xenbus_build_scatter_gather_list,
    xenbus_build_mdl_from_scatter_gather_list
};

static VOID busInterfaceReference(PVOID context)
{
    TraceNotice(("%s(%p)\n", __FUNCTION__, context));
    xenbusBusInterfaceRefcount++;
}

static VOID busInterfaceDereference(PVOID context)
{
    TraceNotice(("%s(%p)\n", __FUNCTION__, context));
    XM_ASSERT(xenbusBusInterfaceRefcount != 0);
    xenbusBusInterfaceRefcount--;
}

static BOOLEAN busInterfaceTranslate(PVOID context,
                                     PHYSICAL_ADDRESS BusAddress,
                                     ULONG Length,
                                     PULONG AddressSpace,
                                     PPHYSICAL_ADDRESS translatedAddress)
{
    TraceBugCheck(("%s\n", __FUNCTION__));
}

static VOID
DumpDeviceDescription(
    IN  PDEVICE_DESCRIPTION Description
    )
{
    TraceVerbose(("Version = %d\n", Description->Version));

    TraceVerbose(("Master = %s\n", (Description->Master) ? "ON" : "OFF"));
    TraceVerbose(("ScatterGather = %s\n", (Description->ScatterGather) ? "ON" : "OFF"));
    TraceVerbose(("DemandMode = %s\n", (Description->DemandMode) ? "ON" : "OFF"));
    TraceVerbose(("AutoInitialize = %s\n", (Description->AutoInitialize) ? "ON" : "OFF"));
    TraceVerbose(("Dma32BitAddresses = %s\n", (Description->Dma32BitAddresses) ? "ON" : "OFF"));
    TraceVerbose(("IgnoreCount = %s\n", (Description->IgnoreCount) ? "ON" : "OFF"));
    TraceVerbose(("Dma64BitAddresses = %s\n", (Description->Dma64BitAddresses) ? "ON" : "OFF"));

    TraceVerbose(("BusNumber = %d\n", Description->BusNumber));
    TraceVerbose(("DmaChannel = %d\n", Description->DmaChannel));
    TraceVerbose(("InterfaceType = %d\n", Description->InterfaceType));
    TraceVerbose(("DmaWidth = %d\n", Description->DmaWidth));
    TraceVerbose(("DmaSpeed = %d\n", Description->DmaSpeed));
    TraceVerbose(("MaximumLength = %d\n", Description->MaximumLength));
    TraceVerbose(("DmaPort = %d\n", Description->DmaPort));
}

static NTSTATUS
AllocateAdapter(
    OUT struct _ADAPTER_OBJECT  **Object
    )
{
    struct xenbus_dma_adapter *xda, *next;

    for (xda = InterlockedExchangePointer(&HeadPendingAdapterRelease, NULL);
         xda != NULL;
         xda = next) {
        next = xda->Next;
        if (xda->Header.PointerCount == 1) {
            TraceNotice(("%s: release xda %p\n", __FUNCTION__, xda));
            XmFreeMemory(xda);
        }
    }
    xda = XmAllocateZeroedMemory(sizeof(*xda));
    if (xda == NULL)
        return STATUS_UNSUCCESSFUL;

    /* We offset the count by 1 so that ObDereferenceObject doesn't
       release the reference before we're ready. */
    xda->Header.PointerCount = 2;

    *Object = &xda->Object;

    return STATUS_SUCCESS;
}

extern NTKERNELAPI NTSTATUS ObCreateObject(
    IN  KPROCESSOR_MODE     ObjectAttributesAccessMode OPTIONAL,
    IN  POBJECT_TYPE        ObjectType,
    IN  POBJECT_ATTRIBUTES  ObjectAttributes OPTIONAL,
    IN  KPROCESSOR_MODE     AccessMode,
    IN  PVOID               Reserved,
    IN  ULONG               ObjectSizeToAllocate,
    IN  ULONG               PagedPoolCharge OPTIONAL,
    IN  ULONG               NonPagedPoolCharge OPTIONAL,
    OUT PVOID               *Object); 

extern  POBJECT_TYPE    IoAdapterObjectType;

static PDMA_ADAPTER busInterfaceGetAdapter(PVOID context,
                                           PDEVICE_DESCRIPTION descr,
                                           PULONG nrRegisters)
{
    RTL_OSVERSIONINFOEXW verInfo;
    struct _ADAPTER_OBJECT *Object;
    NTSTATUS status;

    TraceNotice(("%s(%p)\n", __FUNCTION__, context));

    DumpDeviceDescription(descr);

    XenutilGetVersionInfo(&verInfo);

    if (verInfo.dwMajorVersion == 5 &&
        verInfo.dwMinorVersion == 1) {  // XP
        status = ObCreateObject(KernelMode,
                                IoAdapterObjectType,
                                NULL,
                                KernelMode,
                                NULL,
                                sizeof (struct _ADAPTER_OBJECT),
                                0,
                                0,
                                &Object);
    } else {
        status = AllocateAdapter(&Object);
    }

    if (!NT_SUCCESS(status)) {
        TraceError(("%s: failed to create object (%08x)\n", __FUNCTION__, status));
        return NULL;
    }

    RtlZeroMemory(Object, sizeof (struct _ADAPTER_OBJECT));

    Object->DmaHeader.Version = 1;
    Object->DmaHeader.Size = sizeof(struct _ADAPTER_OBJECT);
    Object->DmaHeader.DmaOperations = &xenbus_dma_operations;

    KeInitializeSpinLock(&Object->SpinLock);
    KeInitializeDeviceQueue(&Object->ChannelWaitQueue);
    InitializeListHead(&Object->AdapterQueue);
    InitializeListHead(&Object->AdapterList);

    Object->MapRegistersPerChannel = 99;
    Object->ScatterGather = descr->ScatterGather;
    Object->IgnoreCount = descr->IgnoreCount;
    Object->Dma32BitAddresses = descr->Dma32BitAddresses;
    Object->Dma64BitAddresses = descr->Dma64BitAddresses;

    *nrRegisters = Object->MapRegistersPerChannel;
    return &Object->DmaHeader;
}

static ULONG busInterfaceGetSetBusData(PVOID context,
                                       ULONG type,
                                       PVOID buffer,
                                       ULONG offset,
                                       ULONG length)
{
    TraceInfo(("%s(%p)\n", __FUNCTION__, context));
    //
    // Nothing to read\write on XENBUS.
    //
    return 0;
}

int
xenbusBusInterfaceRefcount;

const BUS_INTERFACE_STANDARD
xenbusBusInterface = {
    sizeof(BUS_INTERFACE_STANDARD),           /* Size */
    1,                                        /* Version */
    NULL,                                     /* Context */
    busInterfaceReference,
    busInterfaceDereference,
    busInterfaceTranslate,
    busInterfaceGetAdapter,
    busInterfaceGetSetBusData,
    busInterfaceGetSetBusData
};

