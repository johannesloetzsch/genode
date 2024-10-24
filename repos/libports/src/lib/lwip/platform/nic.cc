/*
 * \brief  LwIP ethernet interface
 * \author Stefan Kalkowski
 * \date   2009-11-05
 */

/*
 * Copyright (C) 2009-2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* LwIP includes */
extern "C" {
#include <lwip/api.h>
#include <lwip/def.h>
#include <lwip/dhcp.h>
#include <lwip/mem.h>
#include <lwip/opt.h>
#include <lwip/pbuf.h>
#include <lwip/snmp.h>
#include <lwip/sockets.h>
#include <lwip/stats.h>
#include <lwip/sys.h>
#include <lwip/ip6_addr.h>
#include <netif/etharp.h>
#include <nic.h>
#include <verbose.h>
}

/* Genode includes */
#include <base/thread.h>
#include <base/printf.h>
#include <nic/packet_allocator.h>
#include <nic_session/connection.h>
#include <nic/xml_node.h>
#include <base/log.h>

extern "C" {

	static void  genode_netif_input(struct netif *netif);

}


/*
 * Thread, that receives packets by the nic-session interface.
 */
class Nic_receiver_thread : public Genode::Thread_deprecated<8192>
{
	private:

		typedef Nic::Packet_descriptor Packet_descriptor;

		Nic::Connection  &_nic;       /* nic-session */
		Packet_descriptor _rx_packet; /* actual packet received */
		struct netif     *_netif;     /* LwIP network interface structure */

		Genode::Signal_receiver  _sig_rec;

		Genode::Signal_dispatcher<Nic_receiver_thread> _state_update_dispatcher;
		Genode::Signal_dispatcher<Nic_receiver_thread> _rx_packet_avail_dispatcher;
		Genode::Signal_dispatcher<Nic_receiver_thread> _rx_ready_to_ack_dispatcher;

		void _load_nic_state()
		{
			Genode::Xml_node const nic_node = _nic.xml();

			_netif->mtu =
				nic_node.attribute_value("mtu", (unsigned)_netif->mtu);

			bool const link_state =
				nic_node.attribute_value("link_state", false);

			if (netif_is_link_up(_netif) != link_state) {
				if (link_state)
					netif_set_link_up(_netif);
				else
					netif_set_link_down(_netif);
			}

			try { /* set the IP address if configured */
				typedef Genode::String<IP4ADDR_STRLEN_MAX> Ipv4_string;
				Ipv4_string addr, netmask, gateway;

				Genode::Xml_node const ip_node = nic_node.sub_node("ipv4");

				Ipv4_string    addr_str = ip_node.attribute_value(   "addr", Ipv4_string());
				Ipv4_string netmask_str = ip_node.attribute_value("netmask", Ipv4_string());
				Ipv4_string gateway_str = ip_node.attribute_value("gateway", Ipv4_string());

				if (addr_str != "") {
					ip4_addr_t addr, netmask, gateway;
					ip4_addr_set_zero(&addr);
					ip4_addr_set_zero(&netmask);
					ip4_addr_set_zero(&gateway);

					/* XXX: ip4addr_aton returns an error */

					ip4addr_aton(   addr_str.string(), &addr);
					ip4addr_aton(netmask_str.string(), &netmask);
					ip4addr_aton(gateway_str.string(), &gateway);

					if (!ip4_addr_cmp(&addr, netif_ip4_addr(_netif))) {
						/* bring the interface down to change IP config */
						if (netif_is_up(_netif)) {
							netif_set_down(_netif);
							dhcp_stop(_netif);
						}

						netif_set_addr(_netif, &addr, &netmask, &gateway);
						netif_set_up(_netif);
					}
				}
			}
			catch (Genode::Xml_node::Nonexistent_attribute) { }
			catch (Genode::Xml_node::Nonexistent_sub_node ) { }

			/*
			try {
				typedef Genode::String<IP6ADDR_STRLEN_MAX> Ipv6_string;
				Ipv6_string addr, netmask, gateway;

				Genode::Xml_node const ip_node = nic_node.sub_node("ipv6");

				Ipv6_string    addr_str = ip_node.attribute_value(   "addr", Ipv6_string());
				Ipv6_string netmask_str = ip_node.attribute_value("netmask", Ipv6_string());
				Ipv6_string gateway_str = ip_node.attribute_value("gateway", Ipv6_string());

				if (addr_str != "") {
					ip6_addr_t addr, netmask, gateway;
					ip6_addr_set_zero(&addr);
					ip6_addr_set_zero(&netmask);
					ip6_addr_set_zero(&gateway);

					ip6addr_aton(   addr_str.string(), &addr);
					ip6addr_aton(netmask_str.string(), &netmask);
					ip6addr_aton(gateway_str.string(), &gateway);

					if (!netif_get_ip6_addr_match(&_netif, &addr)) {
						if (netif_is_up(_netif)) {
							netif_set_down(_netif);
							dhcp_stop(_netif);
						}

						netif_set_addr(_netif, &addr, &netmask, &gateway);
						netif_set_up(_netif);
					}
				}
			}
			catch (Genode::Xml_node::Nonexistent_attribute) { }
			catch (Genode::Xml_node::Nonexistent_sub_node ) { }
			 */
		}

		void _handle_state_update(unsigned)
		{
			_nic.rom().update();
			_load_nic_state();
		}

		void _handle_rx_packet_avail(unsigned)
		{
			while (_nic.rx()->packet_avail() && _nic.rx()->ready_to_ack()) {
				_rx_packet = _nic.rx()->get_packet();
				genode_netif_input(_netif);
				_nic.rx()->acknowledge_packet(_rx_packet);
			}
		}

		void _handle_rx_read_to_ack(unsigned) { _handle_rx_packet_avail(0); }

		void _tx_ack(bool block = false)
		{
			/* check for acknowledgements */
			while (_nic.tx()->ack_avail() || block) {
				Packet_descriptor acked_packet = _nic.tx()->get_acked_packet();
				_nic.tx()->release_packet(acked_packet);
				block = false;
			}
		}

	public:

		Nic_receiver_thread(Nic::Connection &nic, struct netif *netif)
		:
			Genode::Thread_deprecated<8192>("nic-recv"), _nic(nic), _netif(netif),
			_state_update_dispatcher(_sig_rec, *this, &Nic_receiver_thread::_handle_state_update),
			_rx_packet_avail_dispatcher(_sig_rec, *this, &Nic_receiver_thread::_handle_rx_packet_avail),
			_rx_ready_to_ack_dispatcher(_sig_rec, *this, &Nic_receiver_thread::_handle_rx_read_to_ack)
		{
			_nic.rom().sigh(_state_update_dispatcher);
			_nic.rx_channel()->sigh_packet_avail(_rx_packet_avail_dispatcher);
			_nic.rx_channel()->sigh_ready_to_ack(_rx_ready_to_ack_dispatcher);

			/* set link status and maybe IP addressing */
			_load_nic_state();
		}

		void entry();
		Nic::Connection  &nic() { return _nic; };
		Packet_descriptor rx_packet() { return _rx_packet; };

		Packet_descriptor alloc_tx_packet(Genode::size_t size)
		{
			while (true) {
				try {
					Packet_descriptor packet = _nic.tx()->alloc_packet(size);
					return packet;
				} catch(Nic::Session::Tx::Source::Packet_alloc_failed) {
					/* packet allocator exhausted, wait for acknowledgements */
					_tx_ack(true);
				}
			}
		}

		void submit_tx_packet(Packet_descriptor packet)
		{
			_nic.tx()->submit_packet(packet);
			/* check for acknowledgements */
			_tx_ack();
		}

		char *content(Packet_descriptor packet) {
			return _nic.tx()->packet_content(packet); }
};


/*
 * C-interface
 */
extern "C" {

	/**
	 * This function should do the actual transmission of the packet. The packet is
	 * contained in the pbuf that is passed to the function. This pbuf
	 * might be chained.
	 *
	 * @param netif the lwip network interface structure for this genode_netif
	 * @param p the MAC packet to send (e.g. IP packet including MAC addresses and type)
	 * @return ERR_OK if the packet could be sent
	 *         an err_t value if the packet couldn't be sent
	 *
	 * @note Returning ERR_MEM here if a DMA queue of your MAC is full can lead to
	 *       strange results. You might consider waiting for space in the DMA queue
	 *       to become availale since the stack doesn't retry to send a packet
	 *       dropped because of memory failure (except for the TCP timers).
	 */
	static err_t
	low_level_output(struct netif *netif, struct pbuf *p)
	{
		Nic_receiver_thread *th = reinterpret_cast<Nic_receiver_thread*>(netif->state);

#if ETH_PAD_SIZE
		pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif
		Nic::Packet_descriptor tx_packet = th->alloc_tx_packet(p->tot_len);
		char *tx_content                 = th->content(tx_packet);

		/*
		 * Iterate through all pbufs and
		 * copy payload into packet's payload
		 */
		for(struct pbuf *q = p; q != NULL; q = q->next) {
			char *src = (char*) q->payload;
			Genode::memcpy(tx_content, src, q->len);
			tx_content += q->len;
		}

		/* Submit packet */
		th->submit_tx_packet(tx_packet);

#if ETH_PAD_SIZE
		pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
		LINK_STATS_INC(link.xmit);
		return ERR_OK;
	}


	/**
	 * Should allocate a pbuf and transfer the bytes of the incoming
	 * packet from the interface into the pbuf.
	 *
	 * @param netif the lwip network interface structure for this genode_netif
	 * @return a pbuf filled with the received packet (including MAC header)
	 *         NULL on memory error
	 */
	static struct pbuf *
	low_level_input(struct netif *netif)
	{
		Nic_receiver_thread   *th         = reinterpret_cast<Nic_receiver_thread*>(netif->state);
		Nic::Connection       &nic        = th->nic();
		Nic::Packet_descriptor rx_packet  = th->rx_packet();
		char                  *rx_content = nic.rx()->packet_content(rx_packet);
		u16_t                  len        = rx_packet.size();

#if ETH_PAD_SIZE
		len += ETH_PAD_SIZE; /* allow room for Ethernet padding */
#endif

		/* We allocate a pbuf chain of pbufs from the pool. */
		struct pbuf *p = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
		if (p) {
#if ETH_PAD_SIZE
			pbuf_header(p, -ETH_PAD_SIZE); /* drop the padding word */
#endif

			/*
			 * We iterate over the pbuf chain until we have read the entire
			 * packet into the pbuf.
			 */
			for(struct pbuf *q = p; q != 0; q = q->next) {
				char *dst = (char*)q->payload;
				Genode::memcpy(dst, rx_content, q->len);
				rx_content += q->len;
			}

#if ETH_PAD_SIZE
			pbuf_header(p, ETH_PAD_SIZE); /* reclaim the padding word */
#endif
			LINK_STATS_INC(link.recv);
		} else {
			LINK_STATS_INC(link.memerr);
			LINK_STATS_INC(link.drop);
		}

		return p;
	}


	/**
	 * This function should be called when a packet is ready to be read
	 * from the interface. It uses the function low_level_input() that
	 * should handle the actual reception of bytes from the network
	 * interface. Then the type of the received packet is determined and
	 * the appropriate input function is called.
	 *
	 * @param netif the lwip network interface structure for this genode_netif
	 */
	static void
	genode_netif_input(struct netif *netif)
	{
		/*
		 * Move received packet into a new pbuf,
		 * if something went wrong, return silently
		 */
		struct pbuf *p = low_level_input(netif);

		/* No packet could be read, silently ignore this */
		if (p == NULL) return;

		if (netif->input(p, netif) != ERR_OK) {
			if (verbose)
				PERR("genode_netif_input: input error");
			pbuf_free(p);
			p = 0;
		}
	}


	/**
	 * Should be called at the beginning of the program to set up the
	 * network interface. It calls the function low_level_init() to do the
	 * actual setup of the hardware.
	 *
	 * This function should be passed as a parameter to netif_add().
	 *
	 * @param netif the lwip network interface structure for this genode_netif
	 * @return ERR_OK if the loopif is initialized
	 *         ERR_MEM if private data couldn't be allocated
	 *         any other err_t on error
	 */
	err_t genode_netif_init(struct netif *netif)
	{
		using namespace Genode;
		LWIP_ASSERT("netif != NULL", (netif != NULL));

		/* Initialize nic-session */
		Nic::Packet_allocator *tx_block_alloc = new (env()->heap())
		                                        Nic::Packet_allocator(env()->heap());

		struct netif_buf_sizes *nbs = (struct netif_buf_sizes *) netif->state;
		Nic::Connection *nic = 0;
		try {
			nic = new (env()->heap()) Nic::Connection(tx_block_alloc,
			                                          nbs->tx_buf_size,
			                                          nbs->rx_buf_size);
		} catch (Parent::Service_denied) {
			destroy(env()->heap(), tx_block_alloc);
			return ERR_IF;
		}

		/* Setup receiver thread */
		Nic_receiver_thread *th = new (env()->heap())
			Nic_receiver_thread(*nic, netif);

		/* Store receiver thread address in user-defined netif struct part */
		netif->state      = (void*) th;
#if LWIP_NETIF_HOSTNAME
		netif->hostname   = "lwip";
#endif /* LWIP_NETIF_HOSTNAME */
		netif->name[0]    = 'e';
		netif->name[1]    = 'n';
		netif->output     = etharp_output;
		netif->linkoutput = low_level_output;
		netif->mtu        = nic->xml().attribute_value("mtu", 1500U);
		netif->hwaddr_len = ETHARP_HWADDR_LEN;
		netif->flags      = NETIF_FLAG_BROADCAST |
		                    NETIF_FLAG_ETHARP    |
		                    NETIF_FLAG_LINK_UP;

		/*
		 * Get MAC address from nic-session and set it accordingly.
		 *
		 * Once the MAC is set, it cannot be changed, so the Nic
		 * session is expected to block until an address is ready.
		 */
		Nic::Mac_address mac = nic->mac_address();
		for(int i=0; i<6; ++i)
			netif->hwaddr[i] = mac.addr[i];

		th->start();

		return ERR_OK;
	}
} /* extern "C" */


void Nic_receiver_thread::entry()
{
	while(true)
	{
		Genode::Signal sig = _sig_rec.wait_for_signal();
		int num    = sig.num();

		Genode::Signal_dispatcher_base *dispatcher;
		dispatcher = dynamic_cast<Genode::Signal_dispatcher_base *>(sig.context());
		dispatcher->dispatch(num);
	}
}
