/****************************************************************************
 *
 *      $Id: network.c,v 1.1 2003/09/10 11:44:38 benjl Exp $
 *
 *      Description: Initialise the network stack and NFS library.
 *
 *      Author:      Ben Leslie
 *
 ****************************************************************************/

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include <lwip/mem.h>
#include <lwip/memp.h>
#include <lwip/udp.h>
#include <lwip/pbuf.h>
#include <lwip/netif/etharp.h>
#include <lwip/netif/sosif.h>

#include "constants.h"
#include "libsos.h"
#include "network.h"

struct cookie mnt_point = {{0}};
static struct serial *serial = NULL;

#define verbose 1

/* 
 * This is the directory on the NFS server that will be mounted
 * for your file system. Change it to match your NFS server config.
 *
 * Watch out for possible swap file collisions with your partner! :)
 */

// Internal APIs, just direct publish from ixp_osal
extern uint32_t ixOsalOemInit(void);
extern void ixOsalOSServicesFinaliseInit(void);
extern int ixOsalOSServicesServiceInterrupt(L4_ThreadId_t *tP, int *sendP);

int network_irq(L4_ThreadId_t *tP, int *sendP)
{
	return ixOsalOSServicesServiceInterrupt(tP, sendP);
}


void network_init(void) {
	dprintf(1, "\nStarting %s\n", __FUNCTION__);

	// Initialise the nslu2 hardware
	ixOsalOemInit(); 

	/* Initialise lwIP */
	mem_init();
	memp_init();
	pbuf_init();
	netif_init();
	udp_init();
	etharp_init();

	/* Setup the network interface */
	struct ip_addr netmask, ipaddr, gw;
	IP4_ADDR(&netmask, 255, 255, 255, 0);	// Standard net mask
#if defined(CUST_ETHZ)
	IP4_ADDR(&gw,      192, 168, 0, 1);		// Your host system
	IP4_ADDR(&ipaddr,  192, 168, 0, 2);		// The Slug's IP address
#else
	/* UNSW */
	IP4_ADDR(&gw,      192, 168, 168, 1);		// Your host system
	IP4_ADDR(&ipaddr,  192, 168, 168, 2);		// The Slug's IP address
#endif

	struct netif *netif = netif_add(&ipaddr,&netmask,&gw, sosIfInit, ip_input);
	netif_set_default(netif);

	// Generate an arp entry for our gateway
	// We should only need to do this once, but Linux seems to love ignoring
	// ARP queries (why??!), so we keep trying until we get a response
	struct pbuf *p = etharp_query(netif, &netif->gw, NULL);
	do {
		(*netif_default->linkoutput)(netif, p);	// Direct output
		sos_usleep(100000);	// Wait a while for a reply
	} while (!etharp_entry_present(&netif->gw));
	pbuf_free(p);

	// Finish the initialisation of the nslu2 hardware
	ixOsalOSServicesFinaliseInit();

	/* Initialise NFS */
	int r = nfs_init(gw); assert(!r);

	mnt_get_export_list();	// Print out the exports on this server

	const char *msg;
	if (mnt_mount(NFS_DIR, &mnt_point))		// Mount aos_nfs
		msg = "%s: Error mounting path '%s'!\n";
	else
		msg = "%s: Successfully mounted '%s'\n";
	dprintf(2, msg, __FUNCTION__, NFS_DIR);

	// Initialise the serial driver
	serial = serial_init();

	dprintf(2, "Finished %s\n\n", __FUNCTION__);
}

int network_sendstring(char *s, int len) {
	if (len <= 0) {
		return 0;
	}
	return serial_send(serial, s, len);
}

int network_register_serialhandler(
		void (*handler)(struct serial *serial, char c)) {
	dprintf(2, "*** network_register_serialhandler\n");

	if (serial == NULL) {
		dprintf(2, "*** serial not started yet ***\n");
		return -1;
	}

	return serial_register_handler(serial, handler);
}

