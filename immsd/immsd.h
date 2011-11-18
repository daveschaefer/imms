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
#ifndef __IMMSD_H
#define __IMMSD_H

#include <string>

#include "imms.h"
#include "socketserver.h"

using std::string;

enum SocketType {
    UNIX_SOCKET = 0,  // unix domain file sockets; the default
    TCP_SOCKET
};

class FileSocketConnection : public GIOSocket
{
public:
    FileSocketConnection(int fd) : processor(0) { init(fd); }
    ~FileSocketConnection() { delete processor; }
    virtual void process_line(const string &line);
    virtual void connection_lost() { delete this; }
protected:
    LineProcessor *processor;
};

// TODO: add security check to make sure the client IP doesn't shift around?
class TCPSocketConnection : public GIOSocket
{
public:
    TCPSocketConnection(int fd) : processor(0) { init(fd); }
    ~TCPSocketConnection() { delete processor; }
    virtual void process_line(const string &line);
    virtual void connection_lost() { delete this; }
protected:
    LineProcessor *processor;
};

class RemoteProcessor : public LineProcessor
{
public:
    RemoteProcessor(FileSocketConnection *connection);
    ~RemoteProcessor();
    void write_command(const string &command)
        { connection->write(command + "\n"); }
    void process_line(const string &line);
protected:
    FileSocketConnection *connection;
};

class ImmsProcessor : public IMMSServer, public LineProcessor
{
public:
    ImmsProcessor(FileSocketConnection *connection);
    ImmsProcessor(TCPSocketConnection *tcpconnection);
    ~ImmsProcessor();
    void write_command(const string &command)
        { connection->write(command + "\n"); }
    void check_playlist_item(int pos, const string &path);
    void process_line(const string &line);

    void playlist_updated();
protected:
    FileSocketConnection *connection;
    TCPSocketConnection *tcpconnection;
};

#endif
