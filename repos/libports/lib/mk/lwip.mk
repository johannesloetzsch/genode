#
# lwIP TCP/IP library
#
# The library implementes TCP and UDP as well as DNS and DHCP.
#

LWIP_PORT_DIR := $(call select_from_ports,lwip)
LWIP_DIR      := $(LWIP_PORT_DIR)/src/lib/lwip

# Genode platform files
SRC_CC   = nic.cc printf.cc sys_arch.cc rand.cc

# Core files
SRC_C    = init.c mem.c memp.c netif.c pbuf.c stats.c udp.c raw.c sys.c \
           tcp.c tcp_in.c tcp_out.c dhcp.c dns.c timers.c def.c inet_chksum.c ip.c

# IPv4 files
SRC_C   += icmp.c  igmp.c  ip4_addr.c  ip4.c  ip_frag.c

# IPv6 files
SRC_C   += $(notdir $(wildcard $(LWIP_DIR)/src/core/ipv6/*.c))

# API files
SRC_C   += err.c api_lib.c api_msg.c netbuf.c netdb.c netifapi.c sockets.c \
           tcpip.c

# Network interface files
SRC_C   += etharp.c ethernet.c

LIBS     = alarm libc timed_semaphore

D_OPTS   = ERRNO
D_OPTS  := $(addprefix -D,$(D_OPTS))
CC_DEF  += $(D_OPTS)

INC_DIR += $(REP_DIR)/include/lwip \
           $(LWIP_PORT_DIR)/include/lwip \
           $(LWIP_DIR)/src/include \
           $(LWIP_DIR)/src/include/ipv4 \
           $(LWIP_DIR)/src/include/api \
           $(LWIP_DIR)/src/include/netif \
           $(REP_DIR)/src/lib/lwip/include

vpath %.cc $(REP_DIR)/src/lib/lwip/platform
vpath %.c  $(LWIP_DIR)/src/core
vpath %.c  $(LWIP_DIR)/src/core/ipv4
vpath %.c  $(LWIP_DIR)/src/core/ipv6
vpath %.c  $(LWIP_DIR)/src/api
vpath %.c  $(LWIP_DIR)/src/netif

SHARED_LIB = yes
