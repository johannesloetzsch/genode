/*
 * \brief  Configuration file for LwIP, adapt it to your needs.
 * \author Stefan Kalkowski
 * \date   2009-11-10
 *
 * See lwip/src/include/lwip/opt.h for all options
 */

/*
 * Copyright (C) 2009-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef __LWIP__LWIPOPTS_H__
#define __LWIP__LWIPOPTS_H__

/* Genode includes */
#include <base/fixed_stdint.h>

#include <stdlib.h>
#include <string.h>

/*
   -----------------------------------------------
   ---------- Platform specific locking ----------
   -----------------------------------------------
*/

#define SYS_LIGHTWEIGHT_PROT        1  /* do we provide lightweight protection */

void genode_memcpy(void * dst, const void *src, unsigned long size);
#define MEMCPY(dst,src,len)             genode_memcpy(dst,src,len)


/*
   ------------------------------------
   ---------- Memory options ----------
   ------------------------------------
*/

#define MEM_LIBC_MALLOC             1
#define MEMP_MEM_MALLOC             1
/* MEM_ALIGNMENT > 4 e.g. for x86_64 are not supported, see Genode issue #817 */
#define MEM_ALIGNMENT               4


/*
   ------------------------------------------------
   ---------- Internal Memory Pool Sizes ----------
   ------------------------------------------------
*/

#define MEMP_NUM_TCP_PCB           128

/*
#define MEMP_NUM_SYS_TIMEOUT        16
#define MEMP_NUM_NETCONN (MEMP_NUM_TCP_PCB + MEMP_NUM_UDP_PCB + MEMP_NUM_RAW_PCB + MEMP_NUM_TCP_PCB_LISTEN - 1)

*/

#define PBUF_POOL_SIZE                  96


/*
   ---------------------------------
   ---------- ARP options ----------
   ---------------------------------
*/

#define LWIP_ARP                        1


/*
   ----------------------------------
   ---------- DHCP options ----------
   ----------------------------------
*/

#define LWIP_DHCP                       1
#define LWIP_DHCP_CHECK_LINK_UP         1


/*
   ----------------------------------
   ---------- DNS options -----------
   ----------------------------------
*/

#define LWIP_DNS                        1


/*
   ---------------------------------
   ---------- TCP options ----------
   ---------------------------------
*/

#define TCP_MSS                         1460
#define TCP_WND                         (96 * TCP_MSS)
#define LWIP_WND_SCALE                  1
#define TCP_RCV_SCALE                   2
#define LWIP_TCP_TIMESTAMPS             1

/*
 * The window scale option (http://tools.ietf.org/html/rfc1323) patch of lwIP
 * definitely works solely for the receive window, not for the send window.
 * Setting the send window size to the maximum of an 16bit value, 65535,
 * or multiple of it (x * 65536 - 1) results in the same performance.
 * Everything else decrease performance.
 */
#define TCP_SND_BUF                 (65535)

#define TCP_SND_QUEUELEN            ((32 * (TCP_SND_BUF) + (TCP_MSS - 1))/(TCP_MSS))


/*
   ------------------------------------------------
   ---------- Network Interfaces options ----------
   ------------------------------------------------
*/

#define LWIP_NETIF_API                  1
#define LWIP_NETIF_STATUS_CALLBACK      1
#define LWIP_NETIF_LINK_CALLBACK        1
#define LWIP_NETIF_LOOPBACK             1


/*
   ------------------------------------
   ---------- Thread options ----------
   ------------------------------------
*/

#define TCPIP_MBOX_SIZE                 128
#define DEFAULT_ACCEPTMBOX_SIZE         128


/*
   ------------------------------------
   ---------- Socket options ----------
   ------------------------------------
*/

/*
 * We use lwIP sockets with the lwip_* functions,
 * do not macro over things like read and write.
 */
#define LWIP_COMPAT_SOCKETS             0
#define LWIP_POSIX_SOCKETS_IO_NAMES     0

#define RECV_BUFSIZE_DEFAULT            128 * 1024
#define LWIP_SO_RCVBUF                  1
#define SO_REUSE                        1
#define LWIP_SO_SNDTIMEO                1
#define LWIP_SO_RCVTIMEO                1


/*
   ----------------------------------------
   ---------- Statistics options ----------
   ----------------------------------------
*/

#define LWIP_STATS                      0


/*
   --------------------------------------
   ---------- Checksum options ----------
   --------------------------------------
*/

/* checksum calculation for outgoing packets can be disabled if the hardware supports it */
#define LWIP_CHECKSUM_ON_COPY           1


/*
   ---------------------------------------
   ---------- IPv6 options ---------------
   ---------------------------------------
*/

#define LWIP_IPV6                       1
#define IPV6_FRAG_COPYHEADER            1


/**********************************
 ** Options not defined in opt.h **
 **********************************/

#define LWIP_COMPAT_MUTEX 1  /* use binary semaphore instead of mutex */

#ifndef LWIP_RAND
genode_uint32_t genode_rand();
#define LWIP_RAND() genode_rand()
#endif

#endif /* __LWIP__LWIPOPTS_H__ */
