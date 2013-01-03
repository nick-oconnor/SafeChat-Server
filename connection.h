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
#include <string.h>
#include <signal.h>
#include <pthread.h>

#define __version       3.0
#define __timeout       30
#define __block_size    1024 * 1024

class Connection {
public:

    typedef std::map<int, Connection *> connections_t;
    typedef std::map<int, std::string *> hosts_t;

    bool _terminate;
    time_t _time;

    Connection(int socket, bool full, connections_t *connections, hosts_t *hosts);
    ~Connection();

    void start();
    void log(const std::string &string);

    static void *network_listener(void *connection) {
        return ((Connection *) connection)->network_listener();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    }

private:

    struct block_t {

        enum cmd_t {
            keepalive, version, full, name, host, list, request, accept, decline, unavailable, data, disconnect
        } _cmd;
        int _size;
        unsigned char *_data;

        block_t() {
            _data = new unsigned char[__block_size];
        }

        block_t(cmd_t cmd) {
            _cmd = cmd;
            _size = 0;
            _data = new unsigned char[__block_size];
        }

        block_t(cmd_t cmd, int size) {
            _cmd = cmd;
            _size = size;
            _data = new unsigned char[__block_size];
        }

        block_t(cmd_t cmd, const void *data, int size) {
            _cmd = cmd;
            _size = size;
            _data = new unsigned char[__block_size];
            memcpy(_data, data, size);
        }

        ~block_t() {
            delete[] _data;
        }

        block_t &operator=(const block_t & block) {
            _cmd = block._cmd;
            _size = block._size;
            memcpy(_data, block._data, block._size);
            return *this;
        }

    };

    bool _full;
    int _socket;
    std::string _name;
    pthread_t _network_listener;
    connections_t *_connections;
    hosts_t *_hosts;
    Connection *_pending, *_peer;

    void *network_listener();
    void send_block(const block_t &block);
};

#endif
