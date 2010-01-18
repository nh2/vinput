/* 
   VInput server
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

//#define MPX 1 // See make.sh

#include <cmath>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "uinput.h"
#ifdef MPX
#include "xinput.h"
#endif

int Debug=0, Mpx=1;

volatile static sig_atomic_t killed = 0;
void schedule_terminate (int param)
{
	std::cerr << "Terminating..." << std::endl;
	killed = 1;
	alarm(1);
}

UInputAbsPointer *pointer = 0;
Display* display = 0;

inline std::string i2str(int i)
{
	char *buf = new char[static_cast<int>(log10(abs(i)+1))+3];
	sprintf(buf,"%d",i);
	std::string res = buf;
	delete[] buf;
	return res;
}

inline int myrecv(int sock, unsigned char *buffer, size_t bufsize, int flags = 0)
{
	if (!killed)
	{
		return recv(sock, buffer, bufsize, flags);
	}
	else
		return 0;
}

#define POINTER_COUNT 4

void mainLoop(int sock)
{
	uint16_t width, height;
	uint8_t devcount = POINTER_COUNT;
	UInputAbsPointer *uinputs[POINTER_COUNT];
	Display *display = 0;
	UInputAbsPointer *pointer = 0;
	
	unsigned char buffer[7];
	int bytes;
	
	// Warte auf HELO
	bytes = myrecv(sock, buffer, sizeof(buffer) - 1, 0);
	if (bytes == -1)
	{
		perror("recv() fehlgeschlagen");
 		return;
	}
	else if (bytes == 0)
		return;
	else if ((bytes >= 6) and (buffer[0] == '#') and (buffer[1] == '#'))
	{
		// Got HELO
		width = ntohs(*(reinterpret_cast<uint16_t*>(&buffer[2])));
		height = ntohs(*(reinterpret_cast<uint16_t*>(&buffer[4])));
	}
	else
	{
		std::cerr << "Received invalid HELO" << std::endl;
		return;
	}
	
	uint i;
	for (i=0;i<POINTER_COUNT;i++)
	{
		std::string name = "MTC Touchpoint ";
		name += i2str(i);
		// Erzeuge UInput-Gerät
		uinputs[i] = new UInputAbsPointer(name, width, height);
	}
	
	// Verbinde zu X-Display
	#ifdef MPX
	if (Mpx)
	{
		display = XOpenDisplay(NULL);
		if (display == NULL) {
			std::cout << "Could not connect to X-Server!" << std::endl;
			exit(1);
		}
		XSync(display, False);
	}
	if (Mpx) for (i=0;i<POINTER_COUNT;i++)
	{
		std::string mname;
		std::string name = "MTC Touchpoint ";
		name += i2str(i);
		if (i>0)
		{
			mname = "MTC Touchpoint Master ";
			mname += i2str(i);
			// Erzeuge einen neuen XInput2-Master
			if (xi2_create_master(display, mname, 0, 1) != 0) {
				std::cout << "Could not create XInput2 master device " << i2str(i) << "!" << std::endl;
				exit(1);
			}
		}
		else
			mname = "Virtual core pointer";
		// Warte bis das vorhin erzeugte UInput-Gerät vom X-Server eingebunden wurde
		// aber maximal 1s.
		xi2_wait_for_device(display, name, 1000);
		// Wechsle die UInput-Geräte in die neuen Master.
		xi2_change_attachment(display, name, mname);
	}
	#endif
	
	// Befehlsschleife
	while (!killed)
	{
		bytes = myrecv(sock, buffer, sizeof(buffer) - 1, 0);
		buffer[bytes] = 0;
		if (bytes == -1)
		{
			perror("recv() fehlgeschlagen");
			break;
		}
		if (bytes == 0)
			break;

		/*while (bytes < 6)
		{
			int i = recv(sock, buffer+bytes, sizeof(buffer-bytes) - 1, 0);
			if (i <= 0) break;
			bytes += i;
			buffer[bytes] = 0;
		}*/
		
		if (bytes >= 6)
		{
			unsigned char opcode = buffer[0], pointernum = buffer[1];
			uint16_t xcoord = ntohs(*(reinterpret_cast<uint16_t*>(&buffer[2]))),
						ycoord = ntohs(*(reinterpret_cast<uint16_t*>(&buffer[4])));
			if (DEBUG) printf("ACTION %c POINTER %hhu [ %hu, %hu ]\n", opcode, pointernum, xcoord, ycoord);
			if (pointernum < POINTER_COUNT)
			{
				pointer = uinputs[pointernum];
			}
			else continue;
			
			if (opcode == '.')
				pointer->moveTo(xcoord, ycoord);
			else if (opcode == '!')
			{
				pointer->moveTo(xcoord, ycoord);
				usleep(20000);
				pointer->click();
			}
			else if (opcode == 'd')
			{
				pointer->press();
				usleep(20000);
			}
			else if (opcode == 'D')
			{
				pointer->moveTo(xcoord, ycoord);
				pointer->press();
				usleep(20000);
			}
			else if (opcode == 'u')
			{
				pointer->release();
				usleep(20000);
			}
			else if (opcode == 'U')
			{
				pointer->moveTo(xcoord, ycoord);
				pointer->release();
				usleep(20000);
			}
			else { printf("Unknown opcode '%c' received.\n", opcode); }
		}
		else
			std::cerr << "Received invalid data: " << buffer << std::endl;
	}
	
	
	for (i=POINTER_COUNT-1;i>0;i--)
	{
		std::string name = "MTC Touchpoint ";
		name += i2str(i);
		// Lösche das UInput-Gerät
		if (uinputs[i])
		{
			if (DEBUG) std::cerr << "deleting " << name << std::endl;
			delete uinputs[i];
			uinputs[i] = NULL;
		}
	}
	#ifdef MPX
	if (Mpx) for (i=POINTER_COUNT-1;i>0;i--)
	{
		usleep(50000);
		std::string name = "MTC Touchpoint ";
		name += i2str(i);
		std::string mname = "MTC Touchpoint Master ";
		mname += i2str(i);
		// Lösche den Master
		if (display)
		{
			if (DEBUG) std::cerr << "deleting master " << mname << std::endl;
			xi2_remove_master(display, mname);
		}
	}
	#endif
	std::cerr << "done" << std::endl;
}

int main(int argc, char**argv)
{
	#ifdef MPX
	Mpx=1;
	#endif
	std::signal(SIGINT, schedule_terminate);
	std::signal(SIGTERM, schedule_terminate);
	std::signal(SIGABRT, schedule_terminate);
	std::signal(SIGQUIT, schedule_terminate);
	std::signal(SIGHUP, schedule_terminate);
	
	int i;
	for (i=1;i<argc;i++)
	{
		if (strcmp(argv[i], "--debug") == 0)
		{
			DEBUG=1;
			std::cerr << "Debug" << std::endl;
		}
		#ifdef MPX
		if (strcmp(argv[i], "--nompx") == 0) 
		{
			std::cerr << "Non-MPX" << std::endl;
			Mpx=0;
		}
		#endif
	}
	
	// Erzeuge Server-Socket
	struct sockaddr_in addr;
	int s = socket(AF_INET, SOCK_STREAM, 0);
	if (s == -1)
	{
		perror("Could not open client socket!");
		return 1;
	}

	// Clear structure
	memset(&addr, 0, sizeof(struct sockaddr_in));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(1243);
	addr.sin_addr.s_addr = inet_addr("42.42.42.253");
	//addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == -1)
	{
		perror("Could not connect to server!");
		return 3;
	}
	
	mainLoop(s);
	close(s);

}
