/*
 IMMS: Intelligent Multimedia Management System
 Copyright (C) 2001-2009 Michael Grigoriev

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation; either version 2 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
*/
#include "socketserver.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>  // for TCP server
#include <string.h>
#include <errno.h>

#include <cstdlib>
#include <iostream>

using std::endl;
using std::cerr;

// initialize here since we're not allowed to do so in the header
SocketType SocketListenerBase::sockettype = UNIX_SOCKET;

SocketListenerBase::SocketListenerBase(const string &sockpath)
{
    // set up fd as file socket
    struct sockaddr_un sock_addr;

    sockettype = UNIX_SOCKET;

    int fd = socket(PF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        cerr << "Could not create a file socket: " << strerror(errno) << endl;
        exit(3);
    }

    sock_addr.sun_family = AF_UNIX;
    strncpy(sock_addr.sun_path, sockpath.c_str(), sizeof(sock_addr.sun_path));
    unlink(sockpath.c_str());

    if (bind(fd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) != 0)
    {
        cerr << "Could not bind socket: " << strerror(errno) << endl;
        exit(4);
    }

    Listen(fd);

    listener = g_io_channel_unix_new(fd);
    g_io_add_watch(listener, (GIOCondition)(G_IO_IN | G_IO_PRI),
            incoming_connection_helper, this);
}

SocketListenerBase::SocketListenerBase(int portno)
{
    // set up fd as TCP socket
    struct sockaddr_in sock_addr;

    sockettype = TCP_SOCKET;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        cerr << "Could not create a TCP socket: " << strerror(errno) << endl;
        exit(3);
    }

    bzero((char *) &sock_addr, sizeof(sock_addr));
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_addr.s_addr = INADDR_ANY;
    sock_addr.sin_port = htons(portno);

    if (bind(fd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) != 0)
    {
        cerr << "Could not bind socket: " << strerror(errno) << endl;
        exit(4);
    }

    Listen(fd);

    //gsocket = gnet_tcp_socket_server_new_with_port(portno);
    //listener = gnet_tcp_socket_get_io_channel(gsocket);
    //TODO: add watch or listen async

    std::cout << "listening on TCP port " << portno << std::endl;
}

void SocketListenerBase::Listen(int fd)
{
    if (listen(fd, 5))
    {
        cerr << "Could not listen on socket: " <<
            strerror(errno) << endl;
        exit(5);
    }
}

SocketListenerBase::~SocketListenerBase()
{
    if (listener)
    {
        g_io_channel_close(listener);
        g_io_channel_unref(listener);
        listener = 0;
    }
}

// if we receive a message on the port, accept and create a connection
gboolean SocketListenerBase::incoming_connection_helper(
        GIOChannel *source, GIOCondition condition, gpointer data)
{
    SocketListenerBase *ss = (SocketListenerBase*)data;

    int fd;

    if (sockettype == UNIX_SOCKET)
    {
        socklen_t size = 0;
        struct sockaddr_un sock_addr;

        memset(&sock_addr, 0, sizeof(sock_addr));
        fd = accept(g_io_channel_unix_get_fd(ss->listener),
                (struct sockaddr*)&sock_addr, &size);

    #ifdef DEBUG
        cerr << "Incoming file connection!" << endl;
    #endif
    }
    else if (sockettype == TCP_SOCKET)
    {
        //fd = gnet_tcp_socket_get_port(gsocket);
    }
    else
    {
        cerr << "Unknown socket type" << endl;
        exit(6);
    }

    if (fd != -1)
        ss->incoming_connection(fd);

    return true;
}

