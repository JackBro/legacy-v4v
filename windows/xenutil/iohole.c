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

/* Various bits related to managing the IO hole we get from the PCI
 * device. */
#include <ntddk.h>
#include <stdarg.h>
#include <ntstrsafe.h>
#include "xsapi.h"
#include "base.h"
#include "verinfo.h"

#define MAX(a, b) ((a)>(b)?(a):(b))

/* IO hole is currently 45 pages. */
#define IO_HOLE_MAX_PAGES 45

static char io_hole_owner[16];
static BOOLEAN io_hole_initialized;

static PHYSICAL_ADDRESS io_hole_start;
static PVOID io_hole_va_start;
static ULONG io_hole_nr_pages;

static ULONG io_hole_bitmap[(IO_HOLE_MAX_PAGES + 31)/32];
static RTL_BITMAP io_hole_in_use;
static struct irqsafe_lock io_hole_lock;

VOID
__XenevtchnShutdownIoHole(const char *module)
{
    memset(io_hole_bitmap, 0, sizeof(io_hole_bitmap));
    io_hole_initialized = FALSE;

    TraceNotice(("IO hole cleared by %s\n", module));
    memset(io_hole_owner, '\0', sizeof(io_hole_owner));
}

VOID
__XenevtchnInitIoHole(const char *module, PHYSICAL_ADDRESS base, PVOID base_va, ULONG nbytes)
{
    if (!AustereMode && io_hole_initialized) {
        TraceWarning(("IO hole already initialized by %s\n", io_hole_owner));
        return;
    }

    io_hole_start = base;
    io_hole_va_start = base_va;
    io_hole_nr_pages = MAX(IO_HOLE_MAX_PAGES, (nbytes / PAGE_SIZE));

    /* For some reason, RtlInitializeBitmap() isn't allowed to be
       called above APC level, although all the other bitmap functions
       work at any irql.  Duplicate the entire thing here. */
    io_hole_in_use.SizeOfBitMap = io_hole_nr_pages;
    io_hole_in_use.Buffer = io_hole_bitmap;

    strncpy(io_hole_owner, module, sizeof(io_hole_owner));
    io_hole_owner[sizeof(io_hole_owner) - 1] = '\0';

    io_hole_initialized = TRUE;

    TraceNotice(("%s: IO hole: [%016llx,%016llx) mapped at %p\n",
                 io_hole_owner,
                 io_hole_start.QuadPart,
                 io_hole_start.QuadPart + (io_hole_nr_pages * PAGE_SIZE),
                 io_hole_va_start));
}

BOOLEAN
__XenevtchnIsMyIoHole(const char *module)
{
    return (strncmp(module, io_hole_owner, sizeof (io_hole_owner)) == 0) ? TRUE : FALSE;
}

PFN_NUMBER
XenevtchnAllocIoPFN(void)
{
    KIRQL old_irql;
    ULONG page_nr;

    old_irql = acquire_irqsafe_lock(&io_hole_lock);
    page_nr = RtlFindClearBitsAndSet(&io_hole_in_use, 1, 0);
    release_irqsafe_lock(&io_hole_lock, old_irql);

    if (page_nr == 0xffffffff)
        return 0;
    else
        return (PFN_NUMBER)(page_nr + (io_hole_start.QuadPart >> PAGE_SHIFT));
}

PVOID
XenevtchnAllocIoMemory(ULONG nr_bytes, PHYSICAL_ADDRESS *pa)
{
    KIRQL old_irql;
    ULONG page_nr;

    nr_bytes = (nr_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

    old_irql = acquire_irqsafe_lock(&io_hole_lock);
    page_nr = RtlFindClearBitsAndSet(&io_hole_in_use,
                                     nr_bytes / PAGE_SIZE,
                                     0);
    release_irqsafe_lock(&io_hole_lock, old_irql);
    if (page_nr == 0xffffffff) {
        TraceWarning (("Filled the io hole!\n"));
        return NULL;
    } else {
        pa->QuadPart = io_hole_start.QuadPart + page_nr * PAGE_SIZE;
        return (PVOID)((ULONG_PTR)io_hole_va_start + page_nr * PAGE_SIZE);
    }
}

VOID
XenevtchnReleaseIoMemory(PVOID va, ULONG nr_bytes)
{
    KIRQL old_irql;
    ULONG page_nr;

    XM_ASSERT(((ULONG_PTR)va & (PAGE_SIZE - 1)) == 0);

    nr_bytes = (nr_bytes + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    page_nr =
        (ULONG)(((ULONG_PTR)va - (ULONG_PTR)io_hole_va_start) / PAGE_SIZE);

    old_irql = acquire_irqsafe_lock(&io_hole_lock);
    XM_ASSERT(RtlAreBitsSet(&io_hole_in_use,
                            page_nr,
                            nr_bytes / PAGE_SIZE));
    RtlClearBits(&io_hole_in_use, page_nr, nr_bytes / PAGE_SIZE);
    release_irqsafe_lock(&io_hole_lock, old_irql);
}

void
XenevtchnReleaseIoPFN(PFN_NUMBER pfn)
{
    KIRQL old_irql;

    old_irql = acquire_irqsafe_lock(&io_hole_lock);
    RtlClearBits(&io_hole_in_use,
                 (ULONG)(pfn - (io_hole_start.QuadPart >> PAGE_SHIFT)),
                 1);
    release_irqsafe_lock(&io_hole_lock, old_irql);
}
