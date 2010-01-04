/* 
   Demo paint application for MPX.
   Compile with:
   gcc -o demo-paint demo-paint.c `pkg-config --cflags --libs xi`
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

#include <stdio.h>
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/extensions/XInput2.h>

/* A 2-dimensional point */
typedef struct {
	int x, y;
} Point;

/* A pointer/pencil to draw with */
typedef struct {
	char down;
	Point p;
	Point lastp;
	GC gc;
} Pointer;

int main (int argc, char** argv) {

	/* Connect to the X server */
	Display *display = XOpenDisplay(NULL);
	int screen_num;

	XGCValues values;
	Colormap colormap;

	/* Check if the XInput Extension is available */
	int opcode, event, error;
	if (!XQueryExtension(display, "XInputExtension", &opcode, &event, &error)) {
		printf("X Input extension not available.\n");
		return -1;
	}

	/* Check for XI2 support */
	int major = 2, minor = 0;
	if (XIQueryVersion(display, &major, &minor) == BadRequest) {
		printf("XI2 not available. Server supports %d.%d\n", major, minor);
		return -1;
	}

	/* Get screen size from display structure macro */
	screen_num = DefaultScreen(display);
	int display_width = DisplayWidth(display, screen_num);
	int display_height = DisplayHeight(display, screen_num);
	printf("screen_num: %d\n", screen_num);
	printf("display_width: %d\n", display_width);
	printf("display_height: %d\n", display_height);

	/* set window dimensions */
	int width = display_width/4*3;
	int height = display_height/4*3;
	int border_width = 3;
	printf("width: %d\n", width);
	printf("height: %d\n", height);

	/* create opaque window */
	Window win = XCreateSimpleWindow(display, DefaultRootWindow(display), 
			0, 0, width, height, border_width, WhitePixel(display,
			screen_num), BlackPixel(display,screen_num));

	printf("win: %d\n", (int)win);

	/* Display window */
	XMapWindow(display, win);
	XSync( display, False );

	/* Set default values for pens (GCs) to draw lines with */
	values.foreground = WhitePixel(display, screen_num);
	values.line_width = 10;
	values.line_style = LineSolid;
	values.cap_style = CapRound;

	/* Create colormap */
	colormap = DefaultColormap(display, screen_num);


	/* Set up MPX events */
	XIEventMask eventmask;
	unsigned char mask[1] = { 0 };

	eventmask.deviceid = XIAllMasterDevices;
	eventmask.mask_len = sizeof(mask);
	eventmask.mask = mask;

	/* Events we want to listen for */
	XISetMask(mask, XI_Motion);
	XISetMask(mask, XI_ButtonPress);
	XISetMask(mask, XI_ButtonRelease);
	XISetMask(mask, XI_KeyPress);
	XISetMask(mask, XI_KeyRelease);

	/* Register events on the window */
	XISelectEvents(display, win, &eventmask, 1);


	/* Maximum pointer count */
	const unsigned int numPointers = 0xFF;
	Pointer pointers[numPointers];

	/* Initialize pointers */
	int i;
	for(i=0; i<numPointers; i++) {
		pointers[i].down = 0;
		pointers[i].p.x = -1;
		pointers[i].p.y = -1;
		pointers[i].lastp.x = -1;
		pointers[i].lastp.y = -1;
	}

	/* Event loop */
	while(1) {

		XEvent ev;
		/* Get next event; blocks until an event occurs */
		XNextEvent(display, &ev);
		if (ev.xcookie.type == GenericEvent &&
		    ev.xcookie.extension == opcode &&
		    XGetEventData(display, &ev.xcookie))
		{
			XIDeviceEvent* evData = (XIDeviceEvent*)(ev.xcookie.data);
			int deviceID = evData->deviceid;
			Pointer *pointer = &pointers[deviceID];

			switch(ev.xcookie.evtype)
			{
			case XI_Motion:
				printf("motion\n");
				if(pointer->down) {
					/* Draw a line from the last point to the next one;
					   multiple lines of these create a stroke */
					XDrawLine(display, win, pointer->gc, pointer->lastp.x,
					          pointer->lastp.y, evData->event_x, evData->event_y);

					// Comment out the following for interesting structures
					pointer->lastp.x = evData->event_x;
					pointer->lastp.y = evData->event_y;
				}
				break;

			case XI_ButtonPress:
				printf("click - ");
				printf("abs X:%f Y:%f - ", evData->root_x, evData->root_y);
				printf("win X:%f Y:%f\n", evData->event_x, evData->event_y);

				/* Create random color for each stroke */
				values.foreground = rand() % 0xFFFFFF;
				pointer->gc = XCreateGC(display, win,
				                        GCForeground|GCLineWidth|GCLineStyle|GCCapStyle,
				                        &values);

				XDrawLine(display, win, pointer->gc, evData->event_x,
					      evData->event_y, evData->event_x, evData->event_y);

				pointer->down = 1;
				pointer->p.x = evData->event_x;
				pointer->p.y = evData->event_y;
				pointer->lastp.x = evData->event_x;
				pointer->lastp.y = evData->event_y;

				break;

			case XI_ButtonRelease:
				printf("unclick\n");
				pointer->down = 0;
				break;

			case XI_KeyPress:
				printf("key down\n");
				break;

			case XI_KeyRelease:
				printf("key up\n");
				break;
			}
		}
		XFreeEventData(display, &ev.xcookie);
	}

	XDestroyWindow(display, win);
	return 0;

}
