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

#ifndef __UINPUT_H__
#define __UINPUT_H__

#include <string>
#include <exception>
#include <linux/uinput.h>

class UInputError: public std::exception {
	public:
		inline UInputError(const std::string &text) throw(): std::exception(), _what(text) {}
		inline const char* what() const throw() { return _what.c_str(); }
		inline ~UInputError() throw() {}
	private:
		std::string _what;
};

typedef void (*initIoctls_ptr) (int);
typedef void (*extendDevStruct_ptr) (uinput_user_dev&, void*);

class UInputDeviceBase {
	public:
		UInputDeviceBase(const std::string &name, int bustype = BUS_USB, initIoctls_ptr = NULL, extendDevStruct_ptr = NULL, void* = NULL);
		virtual ~UInputDeviceBase() throw();
	protected:
		void sendEvent(int type, int code, int value=0);
		struct uinput_user_dev dev_struct;
	private:
		int fd;
};

class UInputAbsPointer: public UInputDeviceBase {
	public:
		UInputAbsPointer(const std::string &name, uint width=1280, uint height=720, int bustype = BUS_USB);
		~UInputAbsPointer() throw();
		void moveTo(int x, int y);
		void press();
		void release();
		void click();
	private:
		static void initIoctls(int fd);
		static void initExtendDevStruct(uinput_user_dev&, void*);
};

#endif