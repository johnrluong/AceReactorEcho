/*
* Sample program demonstrating the use of Ace 7.0.4 to accept connections and echo back data from the client.
* Copyright (C) 2021 Johnny Luong
* 
* This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*/
#include <iostream>
#include <stdlib.h>
#include "ace/Event_Handler.h"
#include "ace/INET_Addr.h"
#include "ace/Hash_Map_Manager.h"
#include "ace/Reactor.h"
#include "ace/Recursive_Thread_Mutex.h"
#include "ace/Service_Object.h"
#include "ace/Sock_Acceptor.h"
#include "ace/Svc_Handler.h"

#include "ace/Timer_Wheel.h"


// Based on https://www.dre.vanderbilt.edu/~schmidt/PDF/reactor-rules.pdf
// ACE 7.0.4
// Tested on Windows 10, 64-bit Release and 32-bit Debug


class ClientSocket : public ACE_Service_Object
{
private:
	ClientSocket();
	ClientSocket(const ClientSocket&);
	const ClientSocket& operator=(const ClientSocket&);
protected:
	ACE_SOCK_STREAM stream;
public:
	ClientSocket(ACE_SOCK_STREAM _stream, ACE_Reactor * reactor) : ACE_Service_Object(reactor), stream(_stream)
	{
		ACE_INET_Addr addr;
		stream.get_remote_addr(addr);
		std::cout << "New connection from " << addr.get_host_addr() << ":" << addr.get_port_number() << std::endl;
	}

	virtual ACE_HANDLE get_handle(void) const
	{
		return stream.get_handle();
	}
	virtual int handle_timeout(const ACE_Time_Value& tv, const void* arg)
	{
		return 0;
	}
	virtual int handle_input(ACE_HANDLE)
	{
		const size_t len = 256;
		char buf[len];
		int flags = 0;
		size_t bytes_transferred = 0;
		ACE_Time_Value v = ACE_Time_Value::zero;

		ssize_t status = stream.recv_n(buf, len, flags, &v, &bytes_transferred);

		if (status == 0)
		{
			// Connection closed.
			if (bytes_transferred > 0)
			{
				std::cout << "Data last received: " << "'" << std::string(buf, bytes_transferred) << "'" << std::endl;
			}
			std::cout << "Connection closed." << std::endl;
			return -1;
		}
		if (!bytes_transferred && status == -1)
		{
			int e = errno;
			if (e == ETIME)
			{
				std::cout << "Timer expired." << std::endl;
				return 0;
			}
			else
			{
				std::cout << "Timer not expired." << std::endl;
			}
			return -1;
		}

		status = stream.send_n(buf, bytes_transferred, flags);

		if (status <= 0)
		{
			std::cout << "Connection error while sending data." << std::endl;
			return -1;
		}
		else
		{
			std::cout << "Sent to client: " << "'" << std::string(buf, bytes_transferred) << "'" << std::endl;
		}

		return 0;
	}

	virtual int handle_close(ACE_HANDLE handle, ACE_Reactor_Mask mask)
	{
		ACE_INET_Addr addr;
		stream.get_remote_addr(addr);
		std::cout << "Closing connection from " << addr.get_host_addr() << ":" << addr.get_port_number() << std::endl;

		stream.close();
		delete this;

		return 0;
	}
};

class ServerSocket : public ACE_Service_Object
{
private:
	ServerSocket();
	ServerSocket(const ServerSocket&);
	const ServerSocket& operator=(const ServerSocket&);

protected:
	ACE_SOCK_Acceptor & acceptor;

public:
	ServerSocket(ACE_SOCK_Acceptor& _acceptor, ACE_Reactor * instance) : ACE_Service_Object(instance), acceptor(_acceptor)
	{
		instance->register_handler(this, ACCEPT_MASK);
	}

	virtual ACE_HANDLE get_handle(void) const
	{
		return acceptor.get_handle();
	}

	virtual int handle_input(ACE_HANDLE fd)
	{
		ACE_SOCK_STREAM stream;

		this->acceptor.accept(stream);
		reactor()->register_handler(new ClientSocket(stream, reactor()), READ_MASK);
		return 0;
	}
	virtual int handle_close(ACE_HANDLE, ACE_Reactor_Mask)
	{
		std::cout << "Shutting down listener" << std::endl;
		acceptor.close();
		delete this;
		return 0;
	}
};

int main(int argc, char** argv)
{

	static int PORT = 50000;
	if (argc == 2)
	{
		PORT = ACE_OS::atoi(argv[1]);
		if (PORT <= 0 || PORT >= 65535)
			PORT = 50000;
	}


	// O(1) time handler
	ACE_Timer_Wheel wheel;
	ACE_Reactor::instance()->timer_queue(&wheel);

	// Listen on port PORT, reuse address as needed.
	ACE_INET_Addr port(PORT);
	ACE_SOCK_Acceptor acceptor(port, 1);

	ServerSocket* listener = new ServerSocket(acceptor, ACE_Reactor::instance());
	
	ACE_Time_Value tv;
	// Set a timer of 10 seconds for the event loop
	tv.set(10);

	std::cout << time(0) << ":Starting event loop." << std::endl;
	ACE_Reactor::instance()->run_reactor_event_loop(tv);
	std::cout << time(0) << ":End event loop." << std::endl;
	ACE_Reactor::instance()->end_reactor_event_loop();
	ACE_Reactor::instance()->close();

	return EXIT_SUCCESS;
}
