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
#ifndef __SOCKETSERVER_H
#define __SOCKETSERVER_H

#include <string>

#include "immsconf.h"
#include "giosocket.h"

//TODO: figure out correct scope working
#include "immsd.h"

using std::string;

class FileSocketListenerBase
{
public:
    FileSocketListenerBase(const string &sockpath);
    virtual ~FileSocketListenerBase();
    static gboolean incoming_connection_helper(GIOChannel *source,
            GIOCondition condition, gpointer data);

    virtual void incoming_connection(int fd) = 0;
protected:
    GIOChannel *listener;
};

// Use unix-domain file sockets for connections
template <typename Connection>
class FileSocketListener : public FileSocketListenerBase
{
public:
    FileSocketListener(const string &sockpath) : 
      FileSocketListenerBase(sockpath) {};
    virtual void incoming_connection(int fd) { new Connection(fd); }
};

// Use TCP sockets for connections
class TCPSocketListener
{
public:
    TCPSocketListener(int portno);  // technically an optional parameter
    ~TCPSocketListener();

    void incoming_connection(int sockfd);
    static gboolean incoming_connection_helper(GIOChannel *source,
            GIOCondition condition, gpointer data);

    static const int default_port = 7778;

protected:
    GIOChannel *listener;
};

#endif
