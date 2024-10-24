/*
 * \brief  EMACPS NIC driver for Xilix Zynq-7000
 * \author Timo Wischer
 * \date   2015-03-10
 */

/*
 * Copyright (C) 2015 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

/* Genode includes */
#include <drivers/board_base.h>
#include <nic/xml_node.h>
#include <nic/root.h>
#include <base/component.h>

#include <os/config.h>

#include "cadence_gem.h"

class Gem_session_component
:
	public Cadence_gem
{
	public:
		Gem_session_component(Genode::size_t const tx_buf_size,
		                      Genode::size_t const rx_buf_size,
		                      Genode::Allocator   &rx_block_md_alloc,
		                      Genode::Ram_session &ram,
		                      Genode::Region_map  &rm,
		                      Genode::Entrypoint  &ep)
		:
			Cadence_gem(tx_buf_size, rx_buf_size, rx_block_md_alloc, ram, rm, ep,
			            Board_base::EMAC_0_MMIO_BASE,
			            Board_base::EMAC_0_MMIO_SIZE,
			            Board_base::EMAC_0_IRQ)
		{
			Nic::Mac_address mac_addr;

			/* try using configured MAC address */
			try {
				Genode::Xml_node nic_config = Genode::config()->xml_node().sub_node("nic");
				nic_config.attribute("mac").value(&mac_addr);
				Genode::log("Using configured MAC address ", mac_addr);
			} catch (...) {
				/* fall back to fake MAC address (unicast, locally managed) */
				mac_addr.addr[0] = 0x02;
				mac_addr.addr[1] = 0x00;
				mac_addr.addr[2] = 0x00;
				mac_addr.addr[3] = 0x00;
				mac_addr.addr[4] = 0x00;
				mac_addr.addr[5] = 0x01;
			}

			/* set mac address */
			mac_address(mac_addr);
		}
};

namespace Component { struct Main; }

struct Component::Main
{
	Genode::Heap heap;
	Nic::Root<Gem_session_component> nic_root;

	Main(Genode::Env &env)
	: heap(env.ram(), env.rm()), nic_root(env, heap, heap)
	{
		env.parent().announce(env.ep().manage(nic_root));
	}
};


Genode::size_t Component::stack_size()      { return 2*1024*sizeof(long); }
void Component::construct(Genode::Env &env) { static Main main(env);      }
