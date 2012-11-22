vinput
======

vinput is a software collection for creating virtual inputs on a Linux system.
It was written at the 26C3 for the multitouch table in the c-base.

vinput is useful to create inputs from arbitrary input devices that cannot be connected easyly to the computer, e.g. multitouch tables.

It offers a daemon which creates new virtual input devices on the fly by listening to a device server which sends information about pointer coordinates, up/down events and so on. Daemon and device server can run on the same system or on different machines in the network.
Written for a multitouch table, vinput optionally creates multi-pointer X (MPX) pointers.


Benefits
--------

* Easily use any kind of input device (touch tables, gesture recognition, telepathy … whatever)
* No need to write drivers, fiddle with low-level input or XInput – vinput does this for you

So what to do if you have a cool device like a multitouch table?

1. Write a small program that can get input information from your device and send it via an easy protocol.
2. Start vinput with the mpx option and connect it to that program.
3. Done. You now have a new mouse pointer on your screen for each finger that touches your table’s surface. Wasn’t that easy?


Prerequisites
-------------

* Linux kernel 2.6
* Xorg >= 7.5 / X server 1.6 (for MPX)
