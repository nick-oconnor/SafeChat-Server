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
#include <stdexcept>
#include <pthread.h>
#include <signal.h>
#include <netdb.h>
#include "block.h"

#define __version           2.0
#define __timeout           60

#define __keepalive         0
#define __name              1
#define __available         2
#define __hosts             3
#define __try               4
#define __accept            5
#define __data              6
#define __disconnect        7

#define __max_block_size    1024 * 1024

class Socket {
public:

    typedef std::map<int, Socket *> socket_t;
    typedef std::map<int, std::string *> host_t;

    bool _delete;
    time_t _time;
    pthread_t _listener;

    Socket(int id, int socket, bool full, socket_t *sockets, host_t *hosts);
    ~Socket();

    void start();
    void clean();

    void send_block(const Block &block);
    void log(const std::string &string);

    static void *listener(void *socket) {
        return ((Socket *) socket)->listener();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    }

private:

    bool _full;
    int _id, _socket;
    std::string _name;
    Socket *_pending, *_peer;
    socket_t *_sockets;
    host_t *_hosts;

    void *listener();

};

#endif
