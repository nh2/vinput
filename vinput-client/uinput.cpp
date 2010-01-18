/* 
   VInput server - UInput handling unit
*/

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

#include <cstring>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>

#include "uinput.h"

UInputDeviceBase::UInputDeviceBase(const std::string &name, int bustype,
                                   initIoctls_ptr initIoctls,
                                   extendDevStruct_ptr extendDevStruct,
                                   void *extendDevStruct_params)
{
	// Open the input device
	fd = open("/dev/input/uinput", O_WRONLY | O_NDELAY);
	if (fd == -1)
		throw UInputError("Could not access UInput subsystem");
	if (initIoctls)
		(*initIoctls)(fd);
	// Initialize the UInput device structure to NULL
	memset(&dev_struct,0,sizeof(dev_struct));
	// Set device name
	name.copy(dev_struct.name, UINPUT_MAX_NAME_SIZE);
	// Set version if not set by merge struct
	dev_struct.id.version = 4;
	// Set bus type
	dev_struct.id.bustype = bustype;
	// Extend device struct
	if (extendDevStruct)
		(*extendDevStruct)(dev_struct, extendDevStruct_params);
	// Create input device
	write(fd, &dev_struct, sizeof(dev_struct));
	if (ioctl(fd, UI_DEV_CREATE))
	{
		throw UInputError("Unable to create UINPUT device.");
	}
}

UInputDeviceBase::~UInputDeviceBase() throw()
{
	ioctl(fd, UI_DEV_DESTROY);
	close(fd);
	fd = 0;
}

void UInputDeviceBase::sendEvent(int type, int code, int value)
{
	struct input_event event; 
	memset(&event, 0, sizeof(event));
	gettimeofday(&event.time, NULL);
	event.type = type;
	event.code = code;
	event.value = value;
	write(fd, &event, sizeof(event));
}

void UInputAbsPointer::initIoctls(int fd)
{
	// Setup uinput device to emulate an absolute pointer
	ioctl(fd, UI_SET_EVBIT, EV_ABS);
	ioctl(fd, UI_SET_EVBIT, EV_KEY);
	ioctl(fd, UI_SET_ABSBIT, ABS_X);
	ioctl(fd, UI_SET_ABSBIT, ABS_Y);
	ioctl(fd, UI_SET_ABSBIT, ABS_PRESSURE);
	ioctl(fd, UI_SET_KEYBIT, BTN_TOUCH);
	ioctl(fd, UI_SET_KEYBIT, BTN_LEFT);
	// Need to set BTN_TOOL_PEN to make the X-Server
	// handle the absolute coordinates correctly
	ioctl(fd, UI_SET_KEYBIT, BTN_TOOL_PEN);
}
struct UInputAbsPointerIEDSParams
{
	unsigned int width, height;
};

UInputAbsPointerIEDSParams *newUInputAbsPointerIEDSParams(uint width, uint height)
{
	// So wie verwendet führt diese Funktion vorerst zu einem Speicherleck.
	UInputAbsPointerIEDSParams *x = new UInputAbsPointerIEDSParams();
	x->width = width;
	x->height = height;
	return x;
}

void UInputAbsPointer::initExtendDevStruct(uinput_user_dev &dev_struct, void *params)
{
	UInputAbsPointerIEDSParams *size = (UInputAbsPointerIEDSParams*)params;
	// Configure absoulte area
	dev_struct.absmin[ABS_X]=0;
	dev_struct.absmax[ABS_X]=size->width;
	dev_struct.absmin[ABS_Y]=0;
	dev_struct.absmax[ABS_Y]=size->height;

	dev_struct.absmin[ABS_PRESSURE]=0;
	dev_struct.absmax[ABS_PRESSURE]=1;
}

UInputAbsPointer::UInputAbsPointer(const std::string &name, uint width, uint height,  int bustype):
	UInputDeviceBase(name, bustype, &initIoctls, &initExtendDevStruct, newUInputAbsPointerIEDSParams(width,height))
{}

UInputAbsPointer::~UInputAbsPointer() throw()
{}

void UInputAbsPointer::moveTo(int x, int y)
{
	sendEvent(EV_ABS, ABS_X, x);
	sendEvent(EV_ABS, ABS_Y, y);
	sendEvent(EV_SYN, SYN_REPORT);
}

void UInputAbsPointer::press()
{
	sendEvent(EV_KEY, BTN_LEFT, 1);
	sendEvent(EV_ABS, ABS_PRESSURE, 1);
	sendEvent(EV_SYN, SYN_REPORT);
}

void UInputAbsPointer::release()
{
	sendEvent(EV_KEY, BTN_LEFT, 0);
	sendEvent(EV_ABS, ABS_PRESSURE, 0);
	sendEvent(EV_SYN, SYN_REPORT);
}

void UInputAbsPointer::click()
{
	press();
	release();
}
