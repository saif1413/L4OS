/****************************************************************************
 *
 *      Description: Example startup file for the SOS project.
 *
 *      Author:		    Godfrey van der Linden(gvdl)
 *      Original Author:    Ben Leslie
 *
 ****************************************************************************/

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

#include <clock/clock.h>
#include <clock/nslu2.h>
#include <sos/sos.h>

#include "frames.h"
#include "irq.h"
#include "l4.h"
#include "libsos.h"
#include "network.h"
#include "pager.h"
#include "syscall.h"
#include "vfs.h"

#define verbose 2

#define ONE_MEG (1 * 1024 * 1024)
#define HEAP_SIZE ONE_MEG /* 1 MB heap */
#define INFINITY (1 << 8)
#define RUN_FRAME_TEST 0

#define IRQ_MASK (1 << SOS_IRQ_NOTIFY_BIT)

// Set aside some memory for a stack for the
// first user task 
#define STACK_SIZE 0x1000
static L4_Word_t init_stack_s[STACK_SIZE];
static L4_Word_t user_stack_s[STACK_SIZE];

// Init thread - This function starts up our device drivers and runs the first
// user program.
static void
init_thread(void)
{
	L4_KDB_SetThreadName(sos_my_tid(), "init_thread");

	// Initialise the network for libsos_logf_init
	network_init();
	vfs_init();
	pager_init();

	// start executables listed in the the BootInfo
	// hack: crt0 expects to be able to pop 3 words off the stack
	dprintf(0, "user_stack_s start at %lx, end at %lx\n", user_stack_s, &user_stack_s[STACK_SIZE]);
	sos_start_binfo_executables(&user_stack_s[STACK_SIZE-3]);

	// Thread finished - block forever
	for (;;)
		sos_usleep(30 * 1000 * 1000);
}

/*
  Syscall loop.

  This implements a very simple, single threaded functions for 
  recieving any IPCs and dispatching to the correct subsystem.

  It currently handles pagefaults, interrupts and special sigma0
  requests.
*/

static __inline__ void 
syscall_loop(void)
{
	dprintf(3, "Entering %s\n", __FUNCTION__);

	// Set up which messages we will receive
	L4_Accept(L4_AddAcceptor(L4_UntypedWordsAcceptor,L4_NotifyMsgAcceptor));

	int send = 0;
	L4_Msg_t msg;
	L4_ThreadId_t tid = L4_nilthread;

	for (;;) {
		L4_MsgTag_t tag;

		// Wait for a message, sometimes sending a reply
		if (!send)
			tag = L4_Wait(&tid); // Nothing to send, so we just wait
		else
			tag = L4_ReplyWait(tid, &tid); // Reply to caller and wait for IPC

		if (L4_IpcFailed(tag)) {
			L4_Word_t ec = L4_ErrorCode();
			dprintf(0, "%s: IPC error\n", __FUNCTION__);
			sos_print_error(ec);
			assert( !(ec & 1) );	// Check for recieve error and bail
			send = 0;
			continue;
		}

		// At this point we have, probably, recieved an IPC
		L4_MsgStore(tag, &msg); /* Get the tag */

		if (L4_IsNilThread(tid)) {
			L4_Word_t notify_bits = L4_MsgWord(&msg, 0);
			dprintf(2, "*** syscall_loop: async notify %lx\n", notify_bits);

			if (notify_bits & IRQ_MASK) {
				int irq = __L4_TCR_PlatformReserved(0);
				int dummy = 0; // never want to reply

				if (irq_find(irq)->irq_request(&tid, &dummy)) {
					L4_LoadMR(0, irq);
					L4_AcknowledgeInterrupt(0, 0);
				}

				msgClear();
			}

			send = 0;
			continue;
		}

		dprintf(2, "%s: got msg, reply cap %lx (%d), (%d %p)\n", __FUNCTION__,
				tid.raw, L4_ThreadNo(tid), (int) TAG_SYSLAB(tag),
				(void *) L4_MsgWord(&msg, 0));

		//
		// Dispatch IPC according to protocol.
		//
		send = 1; /* In most cases we will want to send a reply */
		switch (TAG_SYSLAB(tag)) {
			case L4_PAGEFAULT:
				sos_pager_handler(L4_MsgWord(&msg, 0), L4_MsgWord(&msg, 1));
				L4_Set_MsgTag(L4_Niltag);
				break;

				/*
				pager(tid, &msg);
				L4_Set_MsgTag(L4_Niltag);
				break;
				*/

			case L4_INTERRUPT:
				/* actually an IRQ lock/unlock message */
				dprintf(3, "got interrupt\n");
				network_irq(&tid, &send);
				break;

			case L4_EXCEPTION:
				dprintf(0, "exception: ip=%lx, sp=%lx\ncpsr=%lx\nexception=%lx, cause=%lx\n\n",
						L4_MsgWord(&msg, 0), L4_MsgWord(&msg, 1), L4_MsgWord(&msg, 2),
						L4_MsgWord(&msg, 3), L4_MsgWord(&msg, 4));
				send = 0;
				break;

			default:
				// Turn the tid cap in to an actual tid
				send = syscall_handle(tag, sos_sid2tid(L4_SenderSpace()), &msg);
		}
	}
}

//
// Test for M1
//
static void
frame_test(void) {
	static L4_Word_t *page, *page1, *page2;
	static int i;

	dprintf(0, "entered frame_test\n");

	/* Allocate 10 pages and make sure you can touch them all */
	for (i = 0; i < 10; i++) {
		/* Allocate a page */
		page = (L4_Word_t*) frame_alloc();
		assert(page);

		/* Test you can touch the page */
		*page = 0x37;
		assert(*page == 0x37);

		dprintf(0, "(1) Page #%d allocated at %p\n", i, page);
	}

	/* A couple of pages to free later. */
	page1 = (L4_Word_t*) frame_alloc();
	page2 = (L4_Word_t*) frame_alloc();

	/* Test that you eventually run out of memory gracefully. */
	for (;;) {
		page = (L4_Word_t*) frame_alloc();

		if (!page) {
			dprintf(0, "(2) Out of memory!\n");
			break;
		} else {
			*page = 0x37;
			assert(*page == 0x37);
		}
	}

	frame_free((L4_Word_t) page1);
	frame_free((L4_Word_t) page2);

	/* Test that you never run out of memory if you always free frames. */
	for (i = 0; i < INFINITY; i++) {
		page = (L4_Word_t*) frame_alloc();
		assert(page != NULL);

		// Test you can touch the page.
		*page = 0x37;
		assert(*page == 0x37);

		dprintf(0, "(3) Page #%d allocated at %p\n",  i, page);
		frame_free((L4_Word_t) page);
	}
}

//
// Main entry point - called by crt.
//

int
main (void)
{
	// store our thread id
	L4_Set_UserDefinedHandle(L4_rootserver.raw);

	// Initialise initial sos environment
	libsos_init();

	// Find information about available memory
	L4_Word_t low, high;
	sos_find_memory(&low, &high);
	dprintf(0, "Available memory from 0x%08lx to 0x%08lx - %luMB\n", 
			low, high, (high - low) / ONE_MEG);

	// Initialise the various monolithic things
	frame_init((low + HEAP_SIZE), high);

	// Add irq information
	irq_init();
	irq_add(NSLU2_NPEB_IRQ, network_irq);
	irq_add(NSLU2_NPEC_IRQ, network_irq);
	irq_add(NSLU2_QM1_IRQ, network_irq);
	irq_add(NSLU2_TIMESTAMP_IRQ, timestamp_irq);
	irq_add(NSLU2_TIMER0_IRQ, timer_irq);

	start_timer();

	// Spawn the setup thread which completes the rest of the initialisation,
	// leaving this thread free to act as a pager and interrupt handler.
	(void) sos_thread_new(&init_thread, &init_stack_s[STACK_SIZE]);

	/* Test M1 */
	if (RUN_FRAME_TEST) frame_test();

	dprintf(1, "*** main: about to start syscall loop\n");
	syscall_loop(); // Enter the syscall loop
	/* Not reached */

	return 0;
}

