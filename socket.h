/*
  Copyright Xphysics 2012. All Rights Reserved.

  SafeChat-Server is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation, either version 3 of the License, or (at your option) any
  later version.

  SafeChat-Server is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

  <http://www.gnu.org/licenses/>
 */

#ifndef connection_h
#define	connection_h

#include <map>
#include <pthread.h>
#include <signal.h>
#include <netdb.h>
#include "block.h"

#define __version           3
#define __time_out          60

#define __keep_alive        1
#define __set_name          2
#define __set_available     3
#define __get_hosts         4
#define __try_host          5
#define __decline_client    6
#define __accept_client     7
#define __data              8
#define __disconnect        9

#define __max_block_size    1000000

class Socket {
public:

    bool _terminated;
    time_t _time;
    pthread_t _listener;

    Socket(int socket, bool full, std::map<int, Socket *> *sockets, std::map<int, std::string> *hosts);

    void terminate();
    void print_log(const std::string &str);

    static void *listener(void *socket) {
        return ((Socket *) socket)->listener();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    }

private:

    bool _full;
    short _version;
    int _socket;
    std::string _name;
    Socket *_peer;
    std::map<int, Socket *> *_sockets;
    std::map<int, std::string> *_hosts;

    void *listener();
    void send_block(Block &block);
};

#endif
