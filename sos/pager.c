/****************************************************************************
 *
 *      $Id: pager.c,v 1.4 2003/08/06 22:52:04 benjl Exp $
 *
 *      Description: Example pager for the SOS project.
 *
 *      Author:			Godfrey van der Linden
 *      Original Author:	Ben Leslie
 *
 ****************************************************************************/


//
// Pager is called from the syscall loop whenever a page fault occurs.//
//

#include <stdio.h>
#include <stdlib.h>

#include <l4/types.h>
#include <l4/map.h>
#include <l4/misc.h>
#include <l4/space.h>
#include <l4/thread.h>

#include "frames.h"
#include "pager.h"
#include "libsos.h"

#define verbose 2

AddrSpace addrspace[MAX_ADDRSPACES];

static uintptr_t
page_align_up(uintptr_t adr)
{
	//TODO: Can be simplified, just align usually then add a page size.
	int pageoffset = adr % PAGESIZE;
	if (pageoffset > 0) {
		adr += (PAGESIZE - pageoffset);
	}

	return adr;
}

void
as_init(void) {
	for (int i = 0; i < MAX_ADDRSPACES; i++) {
		addrspace[i].pagetb = NULL;
		addrspace[i].regions = NULL;
	}
}

uintptr_t
add_stackheap(AddrSpace *as) {
	uintptr_t top = 0;

	// Find the highest address from bootinfo and put the
	// heap above it.
	for (Region *r = as->regions; r != NULL; r = r->next)
	{
		if ((r->base + r->size) > top)
			top = r->base + r->size;
	}

	// TODO: Need to actually change malloc to use this new heap area
	top = page_align_up(top);
	Region *heap = (Region *)malloc(sizeof(Region));
	heap->size = ONE_MEG;
	heap->base = top;
	heap->rights = REGION_READ | REGION_WRITE;

	// Put the stack half way up the address space - at the top
	// causes pagefaults, so halfway up seems like a nice compromise.
	top = page_align_up(((unsigned int) -1) >> 1);
	Region *stack = (Region *)malloc(sizeof(Region));
	stack->size = ONE_MEG;
	stack->base = top - stack->size;
	stack->rights = REGION_READ | REGION_WRITE;

	// Add them to the region list.
	stack->next = heap;
	heap->next = as->regions;
	as->regions = stack;

	// crt0 (whatever that is) will pop 3 words off stack,
	// hence the need to subtact 3 words from the sp.
	return stack->base + stack->size - (3 * sizeof(L4_Word_t));
}

static L4_Word_t*
findPageTableWord(PageTable1 *level1, L4_Word_t addr) {
	addr /= PAGESIZE;
	int offset1 = addr / PAGETABLE_SIZE2;
	int offset2 = addr - (offset1 * PAGETABLE_SIZE2);

	if (level1 == NULL) {
		dprintf(0, "!!! findPageTableWord: level1 is NULL!\n");
		return NULL;
	} else if (level1->pages2 == NULL) {
		dprintf(0, "!!! findPageTableWord: level1->pages2 is NULL!\n");
		return NULL;
	}

	if (level1->pages2[offset1] == NULL) {
		level1->pages2[offset1] = (PageTable2*) malloc(sizeof(PageTable2));

		for (int i = 0; i < PAGETABLE_SIZE2; i++) {
			level1->pages2[offset1]->pages[i] = 0;
		}
	}

	return &level1->pages2[offset1]->pages[offset2];
}

static Region*
findRegion(AddrSpace *as, L4_Word_t addr) {
	Region *r;

	for (r = as->regions; r != NULL; r = r->next) {
		if (addr >= r->base && addr < r->base + r->size) break;
	}

	return r;
}

static void
doPager(L4_Word_t addr, L4_Word_t ip) {
	AddrSpace *as = &addrspace[L4_SpaceNo(L4_SenderSpace())];
	L4_Word_t frame;
	int rights;
	int mapKernelToo = 0;

	dprintf(1, "*** pager: fault on ss=%d, addr=%p, ip=%p\n",
			L4_SpaceNo(L4_SenderSpace()), addr, ip);

	addr &= PAGEALIGN;

	if (L4_IsSpaceEqual(L4_SenderSpace(), L4_rootspace)) {
		// Root task will page fault before page table is actually
		// set up, so map 1:1.
		frame = addr;
		rights = L4_FullyAccessible;
	} else {
		// Find region it belongs in.
		Region *r = findRegion(as, addr);

		if (r == NULL) {
			dprintf(0, "Segmentation fault\n");
			// TODO kill the thread (L4_ThreadControl)
		}

		// Place in, or retrieve from, page table.
		L4_Word_t *entry = findPageTableWord(as->pagetb, addr);

		if (*entry != 0) {
			// Already appears in page table, just got unmapped somehow.
		} else if (r->mapDirectly) {
			// Wants to be mapped directly (code/data probably).
			// In this case the kernel doesn't know about it from the
			// frame table, so map it 1:1 in the kernel too.
			*entry = addr;
			mapKernelToo = 1;
		} else {
			*entry = frame_alloc();
		}

		frame = *entry;
		rights = r->rights;
	}

	L4_Fpage_t fpage = L4_Fpage(addr, PAGESIZE);
	L4_Set_Rights(&fpage, rights);
	L4_PhysDesc_t ppage = L4_PhysDesc(frame, L4_UncachedMemory);

	if (!L4_MapFpage(L4_SenderSpace(), fpage, ppage)) {
		sos_print_error(L4_ErrorCode());
		dprintf(0, "Can't map page at %lx to frame %lx for ip = %lx\n",
				addr, frame, ip);
	}

	if (mapKernelToo) {
		if (!L4_MapFpage(L4_rootspace, fpage, ppage)) {
			sos_print_error(L4_ErrorCode());
			dprintf(0, "Failed mapping to kernel too\n");
		}
	}
}

void
pager(L4_ThreadId_t tid, L4_Msg_t *msgP) {
	doPager(L4_MsgWord(msgP, 0), L4_MsgWord(msgP, 1));
}

void
pager_flush(L4_ThreadId_t tid, L4_Msg_t *msgP)
{
	// There is actually a magic fpage that we can use to unmap
	// the whole address space - and I assume we're meant to
	// unmap it from the sender space.
	if (!L4_UnmapFpage(L4_SenderSpace(), L4_CompleteAddressSpace)) {
		sos_print_error(L4_ErrorCode());
		printf("!!! pager_flush: failed to unmap complete address space\n");
	}
}

L4_Word_t*
sender2kernel(L4_Word_t addr) {
	dprintf(1, "*** sender2kernel: addr=%p ", addr);
	AddrSpace *as = &addrspace[L4_SpaceNo(L4_SenderSpace())];

	// Check that addr is in valid region
	Region *r = findRegion(as, addr);
	if (r == NULL) return NULL;

	// Find equivalent physical address - the address might
	// not actually be in the page table yet, so may need to
	// invoke pager manually.
	L4_Word_t physAddr;
	while ((physAddr = *findPageTableWord(as->pagetb, addr & PAGEALIGN)) == 0) {
		dprintf(1, "*** sender2kernel: %p wasn't mapped in, doing manually\n", addr);
		doPager(addr, (L4_Word_t) -1);
	}

	physAddr += (L4_Word_t) (addr & (PAGESIZE - 1));
	return (L4_Word_t*) physAddr;
}

