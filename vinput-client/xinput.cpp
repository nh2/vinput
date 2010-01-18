/* 
   VInput server - XInput2 / MPX handling unit
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

#include <X11/Xlib.h>
#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xutil.h>
#include <stdlib.h>
#include <stdio.h>

#include "xinput.h"

XIDeviceInfoX* xi2_find_device_info(Display *display, const char *name)
{
    XIDeviceInfo *info;
    int ndevices;
    Bool is_id = True;
    int i, id = -1;
	
	unsigned int j;
    for(j = 0; j < strlen(name); j++) {
	if (!isdigit(name[j])) {
	    is_id = False;
	    break;
	}
    }

    if (is_id) {
	  id = atoi(name);
    }

    info = XIQueryDevice(display, XIAllDevices, &ndevices);
    for(i = 0; i < ndevices; i++)
    {
        if ((is_id && info[i].deviceid == id) ||
                (!is_id && strcmp(info[i].name, name) == 0))
        {
			XIDeviceInfoX* ret = new XIDeviceInfoX(info[i]);
			XIFreeDeviceInfo(info);
			return ret;
        }
    }

    XIFreeDeviceInfo(info);
    return NULL;
}

int xi2_create_master(Display* dpy, const std::string &name, bool sendCore, bool enable)
{
    XIAddMasterInfo c;
	c.type = XIAddMaster;
	c.name = new char[name.length()+1];
	name.copy(c.name, name.length());
	c.name[name.length()] = 0;
	c.send_core = sendCore;
	c.enable = enable;
	int ret = XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&c, 1);
	delete[] c.name;
	XSync(dpy, False);
	return ret;
}

int xi2_remove_master(Display* dpy, const std::string &name)
{
	XIRemoveMasterInfo r;
	XIDeviceInfoX *info;
	int ret;
	
	const char *mastername = name.c_str();
	info = xi2_find_device_info(dpy, mastername);

	if (!info) {
		std::string newname = name+" pointer";
		info = xi2_find_device_info(dpy, newname.c_str());
	}

	if (!info) {
		printf("unable to find device %s\n", mastername);
		return EXIT_FAILURE;
	}

	r.type = XIRemoveMaster;
	r.deviceid = info->deviceid;
	r.return_mode = XIFloating;
	
	ret = XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&r, 1);
	delete info;
	XSync(dpy, False);
	return ret;
}

void xi2_change_attachment(Display* dpy, const std::string &slave, const std::string &new_master)
{
    XIDeviceInfoX *sd_info, *md_info;
    XIAttachSlaveInfo c;
    int ret;

	std::string slavename = slave;
	std::string mastername = new_master;
    sd_info = xi2_find_device_info(dpy, slavename.c_str());
    if (!sd_info) {
		fprintf(stderr, "unable to find device %s\n", slavename.c_str());
		return;
    }

	md_info= xi2_find_device_info(dpy, mastername.c_str());
	if (!md_info) {
		if (sd_info->use == XISlavePointer)
			mastername += " pointer";
		if (sd_info->use == XISlaveKeyboard)
			mastername += " keyboard";
		md_info= xi2_find_device_info(dpy, mastername.c_str());
	}
	if (!md_info) {
		delete sd_info;
		fprintf(stderr, "unable to find device %s\n", mastername.c_str());
		return;
    }

    c.type = XIAttachSlave;
    c.deviceid = sd_info->deviceid;
    c.new_master = md_info->deviceid;

    ret = XIChangeHierarchy(dpy, (XIAnyHierarchyChangeInfo*)&c, 1);
	delete md_info;
	delete sd_info;
	XSync(dpy, False);
    return;
}

typedef XIAnyClassInfo* XIAnyClassInfo_ptr;

XIDeviceInfoX::XIDeviceInfoX(const XIDeviceInfo &x)
{
	deviceid = x.deviceid;
	use = x.use;
	attachment = x.attachment;
	enabled = x.enabled;
	num_classes = x.num_classes;
	name = new char[strlen(x.name)+1];
	strncpy(name, x.name, strlen(x.name));
	name[strlen(x.name)] = '\0';
	classes = new XIAnyClassInfo_ptr[num_classes];
	int i;
	for (i=0;i<num_classes;i++)
	{
		classes[i] = new XIAnyClassInfo();
		memcpy(classes[i], x.classes[i], sizeof(XIAnyClassInfo));
	}
}

XIDeviceInfoX::~XIDeviceInfoX() throw()
{
	int i;
	delete[] name;
	for (i=0;i<num_classes;i++)
		delete classes[i];
	delete[] classes;
}

bool xi2_wait_for_device(Display* display, const std::string &name, unsigned int time_msecs)
{
	unsigned int i = 0;
	while (i <= time_msecs)
	{
		XIDeviceInfoX *x = xi2_find_device_info(display, name.c_str());
		if (x)
		{
			delete x;
			return true;
		}
		i++;
		usleep(1000);
	}
	return false;
}
