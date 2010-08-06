#!/usr/bin/env python
# -*- coding: utf-8 -*-

class MouseWindow:
	def __init__(self, resolution, server, fullscreen):
		self.resolution = resolution
		self.fullscreen = fullscreen
		self.server = server
		
	def get_size(self):
		return 
		
	def show(self):
		import gtk
		from gtk import Window, Label

		w = Window()
		w.set_size_request(*self.resolution)
		l = gtk.Label('Coordinates')
		w.add(l)
		l.show()

		w.add_events(gtk.gdk.EXPOSURE_MASK
						| gtk.gdk.LEAVE_NOTIFY_MASK
						| gtk.gdk.BUTTON_PRESS_MASK
						| gtk.gdk.BUTTON_RELEASE_MASK
						| gtk.gdk.POINTER_MOTION_MASK
						| gtk.gdk.POINTER_MOTION_HINT_MASK)

		def configure_event(widget, event):
			pass

		def button_release_event(widget, event):
			if event.button == 1:
				print "unclick"
				self.server.send('u', 1, event.x, event.y)
				return True

		def button_press_event(widget, event):
			if event.button == 1:
				print "click"
				self.server.send('d', 1, event.x, event.y)
				return True

		def motion_notify_event(widget, event):
			if event.is_hint:
				x, y, state = event.window.get_pointer()
			else:
				print "no hint"
				x, y, state = event.x, event.y, event.state

			l.set_text("(%d,%d)" % (x,y))
			print "Event number %d, (%d,%d)" % (event.type, x, y)
			self.server.send('.', 1, event.x, event.y)

			return True

		w.connect("configure_event", configure_event)
		w.connect("motion_notify_event", motion_notify_event)
		w.connect("button_press_event", button_press_event)
		w.connect("button_release_event", button_release_event)
		w.connect("delete_event", gtk.main_quit)

		if self.fullscreen:
			w.fullscreen()
		
		w.show()
		self.server.resolution = w.get_size()

		gtk.main()




DEBUG = True
DEF_LISTEN_ADDR = '::'
DEF_PORT = 1243
DEF_RESOLUTION = (400, 400)
DEF_FULLSCREEN = False

import sys, threading
from socket import socket as _socket, AF_INET6 as _AF_INET6, AF_INET as \
                   _AF_INET, AF_UNIX as _AF_UNIX, SOCK_STREAM as _SOCK_STREAM,\
                   error as _sockerror

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
	resolution = DEF_RESOLUTION
	fullscreen = DEF_FULLSCREEN
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
			elif args[0] == '-r':
				resolution = [ int(x) for x in args[1].split('x', 1) ]
				args.pop(0)
				args.pop(0)
			elif args[0].startswith('--resolution='):
				resolution = [ int(x) for x in args[0][13:].split('x', 1) ]
				if 0 in resolution or len(resolution) != 2:
					raise ValueError, 'invalid resolution'
				args.pop(0)
			elif args[0] in ('--fullscreen'):
				fullscreen = True
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
	
	MouseWindow(resolution=resolution, server=server, fullscreen=fullscreen).show()
	
	debug_output('terminating')
	server.close()
if __name__ == '__main__':
	main()
