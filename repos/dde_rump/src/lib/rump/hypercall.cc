/**
 * \brief  Rump hypercall-interface implementation
 * \author Sebastian Sumpf
 * \author Josef Soentgen
 * \date   2013-12-06
 */

/*
 * Copyright (C) 2013-2014 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#include "sched.h"

#include <base/env.h>
#include <base/printf.h>
#include <base/sleep.h>
#include <os/config.h>
#include <os/timed_semaphore.h>
#include <util/allocator_fap.h>
#include <util/random.h>
#include <util/string.h>

extern "C" void wait_for_continue();
enum {
	SUPPORTED_RUMP_VERSION = 17,
	RESERVE_MEM = 2U * 1024 * 1024
};

static bool verbose = false;

/* upcalls to rump kernel */
struct rumpuser_hyperup _rump_upcalls;


/********************
 ** Initialization **
 ********************/

int rumpuser_init(int version, const struct rumpuser_hyperup *hyp)
{
	PLOG("init Rump hypercall interface version %d", version);
	if (version != SUPPORTED_RUMP_VERSION) {
		PERR("only version is %d supported!", SUPPORTED_RUMP_VERSION);
		return -1;
	}

	_rump_upcalls = *hyp;

	/*
	 * Start 'Timeout_thread' so it does not get constructed concurrently (which
	 * causes one thread to spin in cxa_guard_aqcuire), making emulation *really*
	 * slow
	 */
	Genode::Timeout_thread::alarm_timer();

	return 0;
}


/*************
 ** Threads **
 *************/

static Hard_context * main_thread()
{
	static Hard_context inst(0);
	return &inst;
}

static Hard_context *myself()
{
	Hard_context *h = dynamic_cast<Hard_context *>(Genode::Thread::myself());
	return h ? h : main_thread();
}


Timer::Connection *Hard_context::timer()
{
	static Timer::Connection _timer;
	return &_timer;
}


void rumpuser_curlwpop(int enum_rumplwpop, struct lwp *l)
{
	Hard_context *h = myself();
	switch (enum_rumplwpop) {
		case RUMPUSER_LWP_CREATE:
		case RUMPUSER_LWP_DESTROY:
			break;
		case RUMPUSER_LWP_SET:
			h->set_lwp(l);
			break;
		case RUMPUSER_LWP_CLEAR:
			h->set_lwp(0);
			break;
	}
}


struct lwp * rumpuser_curlwp(void)
{
	return myself()->get_lwp();
}


int rumpuser_thread_create(func f, void *arg, const char *name,
                           int mustjoin, int priority, int cpui_dx, void **cookie)
{
	static long count = 0;

	if (mustjoin)
		*cookie = (void *)++count;

	new (Genode::env()->heap()) Hard_context_thread(name, f, arg, mustjoin ? count : 0);

	return 0;
}


void rumpuser_thread_exit()
{
	Genode::sleep_forever();
}


int errno;
void rumpuser_seterrno(int e) { errno = e; }


/*************
 ** Console **
 *************/

void rumpuser_putchar(int ch)
{
	static unsigned char buf[256];
	static int count = 0;

	buf[count++] = (unsigned char)ch;

	if (ch == '\n') {
		buf[count] = 0;
		int nlocks;
		if (myself() != main_thread())
			rumpkern_unsched(&nlocks, 0);

		PLOG("rump: %s", buf);

		if (myself() != main_thread())
			rumpkern_sched(nlocks, 0);

		count = 0;
	}
}


/************
 ** Memory **
 ************/

class Rump_alloc
{
	private:

		enum { MAX_VM_SIZE = 64 * 1024 * 1024 };

		struct Allocator_policy
		{
			static int block()
			{
				int nlocks;

				if (myself() != main_thread())
					rumpkern_unsched(&nlocks, 0);
				return nlocks;
			}

			static void unblock(int nlocks)
			{
				if (myself() != main_thread())
					rumpkern_sched(nlocks, 0);
			}
		};

		Allocator::Backend_alloc<MAX_VM_SIZE, Allocator_policy> _alloc;

		size_t        _remaining;
		rumpuser_mtx *_mtx;

	public:

		Rump_alloc(Genode::Cache_attribute cached)
		: _alloc(cached)
		{
			using namespace Genode;

			try {
				Genode::Number_of_bytes ram_bytes = 0;
				Xml_node node = config()->xml_node().sub_node("rump");
				node.attribute("quota").value(&ram_bytes);
				_remaining = ram_bytes;
			} catch (...) {
				_remaining = 0;
			}
			if (!_remaining)
				_remaining = env()->ram_session()->quota() - RESERVE_MEM;
			if (_remaining > MAX_VM_SIZE)
				_remaining = MAX_VM_SIZE;
			PLOG("Rump allocator constrained to %lu KB", _remaining / 1024);

			rumpuser_mutex_init(&_mtx, 0);
		}

		~Rump_alloc() { rumpuser_mutex_destroy(_mtx); }

		void *alloc(size_t size, int align = 0)
		{
			void *addr = 0;
			rumpuser_mutex_enter(_mtx);

			if (size > _remaining) {
				PERR("Rump quota reached");
			} else {
				addr = _alloc.alloc_aligned(size, align);
				if (addr)
					_remaining -= size;
			}

			rumpuser_mutex_exit(_mtx);
			return addr;
		}

		void free(void *addr, size_t size)
		{
			rumpuser_mutex_enter(_mtx);
			_alloc.free(addr, size);
			_remaining += size;
			rumpuser_mutex_exit(_mtx);
		}

		Genode::addr_t phys_addr(void *addr) const {
			return _alloc.phys_addr((Genode::addr_t)addr); }

		Genode::size_t avail() const { return _remaining; }
};

static Rump_alloc* allocator()
{
	static Rump_alloc _alloc(Genode::CACHED);
	return &_alloc;
}

int rumpuser_malloc(size_t len, int alignment, void **memp)
{
	int align = alignment ? Genode::log2(alignment) : 0;
	*memp     = allocator()->alloc(len, align);

	if (verbose)
		PWRN("ALLOC: p: %p, s: %zx, a: %d %d", *memp, len, align, alignment);


	return *memp ? 0 : -1;
}


void rumpuser_free(void *mem, size_t len)
{
	allocator()->free(mem, len);

	if (verbose)
		PWRN("FREE: p: %p, s: %zx", mem, len);
}


/************
 ** Clocks **
 ************/

int rumpuser_clock_gettime(int enum_rumpclock, int64_t *sec, long *nsec)
{
	Hard_context *h = myself();
	unsigned long t = h->timer()->elapsed_ms();
	*sec = (int64_t)t / 1000;
	*nsec = (t % 1000) * 1000;
	return 0;
}


int rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec)
{
	int nlocks;
	unsigned int msec = 0;

	Timer::Connection *timer  = myself()->timer();

	rumpkern_unsched(&nlocks, 0);
	switch (enum_rumpclock) {
		case RUMPUSER_CLOCK_RELWALL:
			msec = sec * 1000 + nsec / (1000*1000UL);
			break;
		case RUMPUSER_CLOCK_ABSMONO:
			msec = timer->elapsed_ms();
			msec = ((sec * 1000) + (nsec / (1000 * 1000))) - msec;
			break;
	}

	timer->msleep(msec);
	rumpkern_sched(nlocks, 0);
	return 0;
}


/*****************
 ** Random pool **
 *****************/

int rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp)
{
	return rumpuser_getrandom_backend(buf, buflen, flags, retp);
}


/*************************
 ** Parameter retrieval **
 *************************/

int rumpuser_getparam(const char *name, void *buf, size_t buflen)
{
	/* support one cpu */
	if (!Genode::strcmp(name, "_RUMPUSER_NCPU")) {
		Genode::strncpy((char *)buf, "1", 2);
		return 0;
	}

	/* return out cool host name */
	if (!Genode::strcmp(name, "_RUMPUSER_HOSTNAME")) {
		Genode::strncpy((char *)buf, "rump4genode", 12);
		return 0;
	}

	if (!Genode::strcmp(name, "RUMP_MEMLIMIT")) {
		size_t rump_ram = allocator()->avail();
		/* convert to string */
		Genode::snprintf((char *)buf, buflen, "%zu", rump_ram);
		PLOG("Asserting rump kernel %zu KB of RAM", rump_ram / 1024);
		return 0;
	}

	if (!Genode::strcmp(name, "RUMP_VERBOSE")) {
		bool verbose = false;
		try {
			Genode::Xml_node rump_node =
				Genode::config()->xml_node().sub_node("rump");
			verbose = rump_node.attribute("verbose").value(&verbose);
		} catch (...) { }
		if (verbose)
			Genode::strncpy((char *)buf, "1", 2);
		else
			Genode::strncpy((char *)buf, "0", 2);
		return 0;
	}

	PWRN("unhandled rumpuser parameter %s", name);
	return -1;
}


/**********
 ** Exit **
 **********/

void genode_exit(int) __attribute__((noreturn));

void rumpuser_exit(int status)
{
	if (status == RUMPUSER_PANIC)
		PERR("Rump panic");

	genode_exit(status);
}
