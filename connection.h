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
#include <iostream>
#include <stdexcept>
#include <netdb.h>
#include <signal.h>
#include <pthread.h>
#include "block.h"

#define __version       4.2
#define __timeout       30

class Connection {
public:

    typedef std::map<int, Connection *> connections_t;
    typedef std::map<int, std::string *> peers_t;

    Connection(int socket, bool full, connections_t *connections, peers_t *peers);
    ~Connection();

    void start();
    void log(const std::string &string);

    static void *network_listener(void *connection) {
        return ((Connection *) connection)->network_listener();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    }

    const time_t &get_time() const {
        return _time;
    }

    bool is_terminated() const {
        return _terminate;
    }

private:

    bool _full, _terminate;
    int _socket;
    std::string _name;
    pthread_t _network_listener;
    connections_t *_connections;
    peers_t *_peers;
    time_t _time;
    Connection *_peer;

    void *network_listener();
    void send_block(const block_t &block);

};

#endif
