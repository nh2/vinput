#!/usr/bin/env python
# -*- coding: utf-8 -*-

"""
 This program implements a server sending libavg touch events over a socket
 using a simple binary protocol.
 
 It is written for the vinput Linux virtual input project.
 Copyright (C) 2009 Christoph Grenz, Niklas Hambüchen

 License:
 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 This program uses
 libavg - Media Playback Engine (C) 2003-2008 Ulrich von Zadow
"""

# For compatibility with Python 2.5
from __future__ import with_statement as _with_statement

########## Defaults ##########

DEBUG = False
DEF_CURSORCOUNT = 4
DEF_LISTEN_ADDR = '::'
DEF_PORT = 1243
DEF_RESOLUTION = (1280, 720)

##############################

import sys, threading
from socket import socket as _socket, AF_INET6 as _AF_INET6, AF_INET as \
                   _AF_INET, AF_UNIX as _AF_UNIX, SOCK_STREAM as _SOCK_STREAM,\
                   error as _sockerror

from libavg import avg, AVGApp

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
	'''
	ServerApp.start(cursorcount, resolution, server, ...) -> a libAVG
	application object sending touches over a ServerSocketThread object
	to all interested clients.
	
	 cursorcount: number of touches that should be registered and processed
	              simultaneously.
	 resolution: resolution of the application window and the touch coordinates
	 server: Object used to send the events to clients (e.g. a running
	         ServerSocketThread instance)
	'''
	
	multitouch = True
	
	def __init__(self, cursorcount, resolution, server, *args, **kwargs):
		'''constructor - initializes x'''
		super(ServerApp, self).__init__(*args, **kwargs)
		self.cursorcount = cursorcount
		self.server = server
		# Initialize id mapping set
		self.mappable_ids = set(xrange(self.cursorcount)) if self.cursorcount \
		                    is not None else None
		self.mappings = {}
	
	def __del__(self):
		self.server.close_clients()
		del self.server
	
	def init(self):
		'''used internally - registers the touch event handlers'''
		# Register event handlers
		self._parentNode.setEventHandler(avg.CURSORDOWN, avg.TOUCH, self._thandler)
		self._parentNode.setEventHandler(avg.CURSORUP, avg.TOUCH, self._thandler)
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
	"""
	ServerSocketThread(announce_resolution, sock_addr, sock_family,
	                   [sock_type, [sock_protocol]]) -> a Thread object
	implementing the simple vinput protocol on a socket.
	
	 announce_resolution: resolution/range of the used coordinates
	 sock_addr: address to listen on; format varies with sock_family
	            (for example ('::', 1243)) to listen  on TCP port 1243 on
	            all ip addresses when using an IPv6 socket)
	 sock_family: address family as defined in the socket module
	              (for example socket.AF_INET6)
	 sock_type: socket type as defined in the socket module
	            (default: SOCK_STREAM)
	 sock_protocol: socket protocol as defined in the socket module
	                (if omitted, uses system default protocol for the
	                given socket type, so TCP by default)
	"""
	
	def __init__(self, announce_resolution, sock_addr, sock_family,
	             sock_type=_SOCK_STREAM, sock_protocol=0):
		"""
		constructor - initializes x
		"""
		super(ServerSocketThread, self).__init__(name="Socket Loop")
		# Flag as daemon thread
		self.setDaemon(True)
		# Init attributes
		self._clients = []
		self._clients_lock = threading.RLock()
		self.address = sock_addr
		self.family = sock_family
		self.sock_type = sock_type
		self.sock_prot = sock_protocol
		self.resolution = announce_resolution
		self._sock = None
	
	def __del__(self):
		"""destructor - cleans up all open connections and closes the server socket"""
		if self._sock:
			try:
				self.close()
			finally:
				del self._sock
	
	def run(self):
		"""
		Contains the thread main loop - opens and binds the server socket and
		waits for client connections
		NOTE: use x.start() to run the thread. x.run() is used internally.
		"""
		debug_output('opening socket on %r' % (self.address,))
		self._sock = sock = _socket(self.family, self.sock_type, self.sock_prot)
		sock.bind(self.address)
		sock.listen(1)
		debug_output('starting socket loop')
		while True:
			conn, addr = sock.accept()
			conn.send("##"+i2s(self.resolution[0], 2)+i2s(self.resolution[1], 2))
			with self._clients_lock:
				self._clients.append((conn, addr))
	
	def send(self, opcode, pointer, xcoord=0, ycoord=0):
		"""
		x.send(opcode, pointer, [xcoord, ycoord])
		Send a command to all connected clients.
		
		Usually needed opcodes are '.' for movements, 'd' and 'u' for touch
		and release, respective the uppercase variants for simultaneous
		movements and touches/releases.
		"""
		debug_output(" sending %s %i %i %i" %(opcode, pointer, xcoord, ycoord))
		if isinstance(opcode, (int, long)):
			opcode = chr(opcode)
		if isinstance(pointer, (int, long)):
			pointer = chr(pointer % 256)
		data = opcode+pointer+i2s(xcoord, 2)+i2s(ycoord, 2)
		with self._clients_lock:
			failed = []
			for conn, addr in self._clients:
				try:
					conn.send(data)
				except _sockerror:
					failed.append((conn, addr))
			for client in failed:
				try:
					client[0].close()
				except _sockerror:
					pass
				self._clients.remove(client)
	def close_clients(self):
		'''
		x.close_clients()
		Closes all client connections.
		'''
		with self._clients_lock:
			for conn, addr in self._clients:
				try:
					conn.close()
				except _sockerror:
					pass
			del self._clients[:]
	
	def close(self):
		with self._clients_lock:
			self.close_clients()
			try:
				self._sock.close()
			finally:
				self._sock = None
		

def _parse_addr(string):
	'''parses a listen address string into all needed socket parameters'''
	result = None
	if string.isdigit():
		result = (_AF_INET6, _SOCK_STREAM, 0, ('::', int(string)))
	elif string.startswith('unix:'):
		if string.startswith('unix::'):
			result = (_AF_UNIX, _SOCK_STREAM, 0, '\0'+string[6:])
		else:
			result = (_AF_UNIX, _SOCK_STREAM, 0, string[5:])
	elif '.' in string:
		addr, port = string.rsplit(':', 1)
		port = int(port)
		result = (_AF_INET, _SOCK_STREAM, 0, (addr, port))
	elif string.startswith('['):
		addr, port = string.rsplit(']:', 1)
		port = int(port)
		addr = addr[1:]
		if '%' in addr:
			addr, scopeid = addr.rsplit('%', 1)
			result = (_AF_INET6, _SOCK_STREAM, 0, (addr, port, 0, scopeid))
		else:
			result = (_AF_INET6, _SOCK_STREAM, 0, (addr, port))
	else:
		# By default assume IPv6 addresses
		addr, port = string.rsplit(':', 1)
		port = int(port)
		if '%' in addr:
			addr, scopeid = addr.rsplit('%', 1)
			result = (_AF_INET6, _SOCK_STREAM, 0, (addr, port, 0, scopeid))
		else:
			result = (_AF_INET6, _SOCK_STREAM, 0, (addr, port))
	return result

def main():
	'''main program - creates a server application and runs it'''
	# Try using psyco to boost performance
	try:
		debug_output('using psyco')
		import psyco
		psyco.full()
	except ImportError:
		pass
	# Process parameters
	cursorcount, resolution = DEF_CURSORCOUNT, DEF_RESOLUTION
	addr = DEF_LISTEN_ADDR, DEF_PORT
	sock_family, sock_type, sock_prot = _AF_INET6, _SOCK_STREAM, 0
	args = sys.argv[1:]
	try:
		while len(args) > 0:
			if args[0] in ('-d', '--debug'):
				global DEBUG
				DEBUG = True
				args.pop(0)
			elif args[0] == '-l':
				sock_family, sock_type, sock_prot, addr = _parse_addr(args[1])
				args.pop(0)
				args.pop(0)
			elif args[0].startswith('--listen='):
				sock_family, sock_type, sock_prot, addr = _parse_addr(args[0][9:])
				args.pop(0)
			elif args[0] == '-c':
				cursorcount = int(args[1])
				args.pop(0)
				args.pop(0)
			elif args[0].startswith('--cursorcount='):
				cursorcount = int(args[0][14:])
				args.pop(0)
			elif args[0] == '-r':
				resolution = [ int(x) for x in args[1].split('x', 1) ]
				args.pop(0)
				args.pop(0)
			elif args[0].startswith('--resolution='):
				resolution = [ int(x) for x in args[0][13:].split('x', 1) ]
				if 0 in resolution or len(resolution) != 2:
					raise ValueError, 'invalid resolution'
				args.pop(0)
			else:
				print >> sys.stderr, 'Unknown parameter "%s". Aborted.' \
				                     % args[0]
				sys.exit(1)
	except IndexError:
		print >> sys.stderr, 'Parameter "%s" needs an argument. Aborted.' \
		                     % args[0]
		sys.exit(1)
	except ValueError:
		print >> sys.stderr, 'Invalid syntax for parameter "%s". Aborted.' \
		                     % args[0]
		sys.exit(1)
	# Run server socket thread
	server = ServerSocketThread(resolution, addr, sock_family, sock_type,
	                            sock_prot)
	server.start()
	# Run main application
	debug_output('running app')
	ServerApp.start(cursorcount=cursorcount, resolution=resolution,
	                server=server)
	debug_output('terminating')
	server.close()
if __name__ == '__main__':
	main()
