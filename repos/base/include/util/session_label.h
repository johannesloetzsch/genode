/*
 * \brief  Session label utilities
 * \author Emery Hemingway
 * \date   2015-01-12
 */

/*
 * Copyright (C) 2016 Genode Labs GmbH
 *
 * This file is part of the Genode OS framework, which is distributed
 * under the terms of the GNU General Public License version 2.
 */

#ifndef _INCLUDE__UTIL__LABEL_H_
#define _INCLUDE__UTIL__LABEL_H_

#include <base/snprintf.h>
#include <util/string.h>

namespace Genode {

	enum { LABEL_MAX_LEN = 128 };

	struct Label;

	static char const *label_last(char const *label)
	{
		for (int i = strlen(label)-4; i > 0; --i)
			if (!strcmp(" -> ", label+i, 4))
				return label+i+4;
		return label;
	}
}

struct Genode::Label : String<LABEL_MAX_LEN>
{
	typedef String<LABEL_MAX_LEN> String;

	Label() { }
	Label(char const *str) : String(str) { }
	Label(char const *str, size_t len) : String(str, len) { }

	/**
	 * Constructor
	 *
	 * \param label   session label
	 * \param parent  parent label to prepend to session label
	 */
	explicit Label(char const *label, char const *parent)
	{
		char buf[String::capacity()];

		strncpy(buf, parent, String::capacity());
		if (*parent) {
			size_t n = strlen(buf);

			strncpy(&buf[n], " -> ", String::capacity() - n);
			n += 4;
			strncpy(&buf[n], label,  String::capacity() - n);
		}
		*static_cast<String *>(this) = String(buf);
	}

	char const *last_element() const { return label_last(string()); }
};

#endif
