#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# This program is written for the vinput Linux virtual input project.
# Copyright (C) 2009 Christoph Grenz, Niklas Hambüchen
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# This program uses
# libavg - Media Playback Engine (C) 2003-2008 Ulrich von Zadow

from __future__ import with_statement

DEBUG = False
CURSORCOUNT = 4
PORT = 1243
RESOLUTION = (1280, 720)

import sys, threading
from socket import socket, AF_INET6, SOCK_STREAM, error as sockerror
from libavg import avg, AVGApp

try:
	import psyco
	psyco.full()
except ImportError:
	pass


def i2s(integer, bytelength=None):
	'''Converts an integer into a big-endian byte string'''
	result = []
	while integer > 0:
		result.append(chr(integer%256))
		integer = integer//256
	if bytelength is not None:
		if len(result) > bytelength:
			raise OverflowError, 'Given number is too big for %i bytes.' % bytelength
		for _ in range(len(result), bytelength):
			result.append(chr(0))
	result.reverse()
	return ''.join(result)

def debug_output(*args):
	'''Prints debugging output to stderr if DEBUG is True'''
	if DEBUG and len(args) > 0:
		for arg in args[:-1]:
			print >> sys.stderr, arg,
		print >> sys.stderr, args[-1]

class ServerApp(AVGApp):
	multitouch = True
	
	def __init__(self, cursorcount, resolution=(1280, 720), addr=('::', 1243),
	             addr_family=AF_INET6, *args, **kwargs):
		super(ServerApp, self).__init__(*args, **kwargs)
		self.cursorcount = cursorcount
		self.server = ServerSocketThread(addr_family, addr, resolution)
		# Initialize id mapping set
		self.mappable_ids = set(xrange(self.cursorcount)) if self.cursorcount \
		                    is not None else None
		self.mappings = {}
	
	def __del__(self):
		self.server.close_clients()
		del self.server
	
	def init(self):
		# Register event handlers
		self._parentNode.setEventHandler(avg.CURSORDOWN, avg.TOUCH, self._thandler)
		self._parentNode.setEventHandler(avg.CURSORUP, avg.TOUCH, self._thandler)
		self._parentNode.setEventHandler(avg.CURSOROUT, avg.TOUCH, self._thandler)
		self._parentNode.setEventHandler(avg.CURSORMOTION, avg.TOUCH, self._thandler)
	
	def _thandler(self, event):
		'''Callback function -- Handles touch events'''
		mappable_ids, mappings = self.mappable_ids, self.mappings
		debug_output('Event:', event.cursorid, event.x, event.y, 'TYPE:', event.type)
		try:
			# Get cursor id to id mapping if possible
			myid = mappings[event.cursorid]
		except KeyError:
			myid = None
		touchid = event.cursorid
		
		# Handle beginning touches
		if event.type == avg.CURSORDOWN:
			if myid is None:
				if len(mappable_ids) > 0:
					# Assign smallest free id to current cursor id
					myid = self.cursorcount
					for element in mappable_ids:
						if element < myid:
							myid = element
					mappable_ids.remove(myid)
					mappings[event.cursorid] = myid
				# If there is no free id, show a warning
				debug_output(('Ignoring pointer at %ix%i; maximum of %i '
				             +'cursors reached.')
				             % (event.x,event.y, self.cursorcount))
			# If an id could be assigned, send events:
			if myid is not None:
				self.server.send('.', myid, event.x, event.y)
				self.server.send('d', myid, event.x, event.y)
		# Handle ending touches
		elif event.type == avg.CURSORUP and myid is not None:
			mappable_ids.add(myid)
			del mappings[touchid]
			self.server.send('u', myid, event.x, event.y)
		# Handle touch movements
		elif event.type == avg.CURSORMOTION and myid is not None:
			self.server.send('.', myid, event.x, event.y)

class ServerSocketThread(threading.Thread):
	def __init__(self, family, addr,
	             announce_resolution=(1280, 720)):
		super(ServerSocketThread, self).__init__(name="Socket Loop")
		self.clients = []
		self.clients_lock = threading.Lock()
		self.address = addr
		self.family = family
		self.resolution = announce_resolution
		self.sock = None
	
	def __del__(self):
		if self.sock:
			try:
				self.close_clients()
			finally:
				self.sock.close()
				del self.sock
	
	def run(self):
		debug_output('opening socket on %r' % self.address)
		self.sock = sock = socket(self.family, SOCK_STREAM)
		sock.bind(self.address)
		sock.listen(1)
		debug_output('starting socket loop')
		while True:
			conn, addr = sock.accept()
			conn.send("##"+i2s(self.resolution[0], 2)+i2s(self.resolution[1], 2))
			with self.clients_lock:
				self.clients.append((conn, addr))
	
	def send(self, opcode, pointer, xcoord=0, ycoord=0):
		debug_output(" sending %s %i %i %i" %(opcode, pointer, xcoord, ycoord))
		if isinstance(opcode, (int, long)):
			opcode = chr(opcode)
		if isinstance(pointer, (int, long)):
			pointer = chr(pointer % 256)
		data = opcode+pointer+i2s(xcoord, 2)+i2s(ycoord, 2)
		with self.clients_lock:
			failed = []
			for conn, addr in self.clients:
				try:
					conn.send(data)
				except sockerror:
					failed.append((conn, addr))
			for client in failed:
				try:
					client[0].close()
				except sockerror:
					pass
				self.clients.remove(client)
	def close_clients(self):
		with self.clients_lock:
			for conn, addr in self.clients:
				try:
					conn.close()
				except sockerror:
					pass
			del self.clients[:]

def main():
	print 'running app'
	ServerApp.start(cursorcount=CURSORCOUNT, resolution=RESOLUTION,
	                addr=('::', PORT))
	print 'terminating'
if __name__ == '__main__':
	main()
