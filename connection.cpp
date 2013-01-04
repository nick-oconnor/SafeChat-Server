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

#include "connection.h"

Connection::Connection(int socket, bool full, connections_t *connections, peers_t *peers) {
    _socket = socket;
    _full = full;
    _connections = connections;
    _peers = peers;
    _terminate = false;
    _peer = NULL;
    time(&_time);
}

Connection::~Connection() {
    pthread_kill(_network_listener, SIGTERM);
    close(_socket);
    _peers->erase(_socket);
    if (_peer) {
        pthread_kill(_peer->_network_listener, SIGTERM);
        _peer->send_block(block_t(block_t::disconnect));
        _peer->log("disconnect sent");
        _peer->_terminate = true;
        _peer->_peer = NULL;
    }
    log("terminated");
}

void Connection::start() {

    int protocol_ = __version;

    log("established");
    send_block(block_t(block_t::version, &protocol_, sizeof protocol_));
    send_block(block_t(block_t::full, &_full, sizeof _full));
    if (_full) {
        log("server is full");
        _terminate = true;
    } else
        pthread_create(&_network_listener, NULL, &Connection::network_listener, this);
}

void Connection::log(const std::string &string) {

    time_t time_t = time(NULL);
    struct tm *tm = localtime(&time_t);
    char *time = asctime(tm);

    time[strlen(time) - 1] = '\0';
    std::cout << time << " Connection " << _socket << ": " << string << std::endl;
}

void *Connection::network_listener() {

    int lists_size;
    connections_t::iterator connection;
    peers_t::iterator peer;
    block_t block;

    signal(SIGTERM, thread_handler);
    try {
        while (true) {
            if (!recv(_socket, &block._cmd, sizeof block._cmd, MSG_WAITALL))
                throw std::runtime_error("connection dropped");
            if (!recv(_socket, &block._size, sizeof block._size, MSG_WAITALL))
                throw std::runtime_error("connection dropped");
            if (block._size > __block_size)
                throw std::runtime_error("oversized block received");
            if (block._size)
                if (!recv(_socket, block._data, block._size, MSG_WAITALL))
                    throw std::runtime_error("connection dropped");
            time(&_time);
            if (block._cmd == block_t::name && !_peer) {
                _name = (char *) block._data;
                log("set name to " + _name);
            } else if (block._cmd == block_t::add && !_peer) {
                _peers->insert(std::make_pair(_socket, &_name));
                log("added to peers");
            } else if (block._cmd == block_t::list && !_peer) {
                lists_size = _peers->size();
                send_block(block_t(block_t::list, &lists_size, sizeof lists_size));
                for (peer = _peers->begin(); peer != _peers->end(); peer++) {
                    send_block(block_t(block_t::list, &peer->first, sizeof peer->first));
                    send_block(block_t(block_t::list, peer->second->c_str(), peer->second->size() + 1));
                }
                log("peers list sent");
            } else if (block._cmd == block_t::connect && !_peer) {
                connection = _connections->find(*(int *) block._data);
                peer = _peers->find(*(int *) block._data);
                if (connection != _connections->end() && peer != _peers->end()) {
                    _peer = connection->second;
                    _peer->_peer = this;
                    send_block(block_t(block_t::connect, _peer->_name.c_str(), _peer->_name.size() + 1));
                    _peer->send_block(block_t(block_t::connect, _name.c_str(), _name.size() + 1));
                    log("connected to " + connection->second->_name);
                    _peers->erase(_peer->_socket);
                } else {
                    send_block(block_t(block_t::unavailable));
                    log("unavailable sent");
                }
            } else if (block._cmd == block_t::data && _peer)
                _peer->send_block(block);
            else if (block._cmd == block_t::disconnect)
                throw std::runtime_error("disconnect received");
        }
    } catch (const std::exception &exception) {
        log(exception.what());
        _terminate = true;
    }
    return NULL;
}

void Connection::send_block(const block_t &block) {
    signal(SIGPIPE, SIG_IGN);
    send(_socket, &block._cmd, sizeof block._cmd, 0);
    send(_socket, &block._size, sizeof block._size, 0);
    if (block._size)
        send(_socket, block._data, block._size, 0);
}
