/*
 * Copyright (C) 2009 Christoph Grenz, Niklas Hambüchen
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __XINPUT_H__
#define __XINPUT_H__

#include <string>
#include <exception>
#include <cstring>
#include <X11/extensions/XInput2.h>

class XInputError: public std::exception {
	public:
		XInputError(const std::string &text) throw(): std::exception(), _what(text) {}
		const char* what() const throw() { return _what.c_str(); }
		~XInputError() throw() {}
	private:
		std::string _what;
};

struct XIDeviceInfoX: public XIDeviceInfo {
	XIDeviceInfoX(const XIDeviceInfo &x);
	~XIDeviceInfoX() throw();
};

/**
 * Returns a input device information object for the given name or NULL if
 * there is no such device. The returned object must be destroyed by the
 * caller.
 */
XIDeviceInfoX* xi2_find_device_info(Display *display, const char *name);

/**
 * Create a new master device. Name must be supplied, other values are
 * optional.
 */
int xi2_create_master(Display* dpy, const std::string &name, bool sendCore=1, bool enable=1);

/**
 * Remove a master device.
 * All attached devices are set to Floating
 */
int xi2_remove_master(Display* dpy, const std::string &name);

/**
 * Swap a device from one master to another.
 */
void xi2_change_attachment(Display* dpy, const std::string &slave, const std::string &new_master);

/**
 * Waits for a input device to appear by polling every millisecond.
 */
bool xi2_wait_for_device(Display* display, const std::string &name, unsigned int time_msecs);

#endif