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

Connection::Connection(int socket, bool full, connections_t *connections, hosts_t *hosts) {
    _socket = socket;
    _full = full;
    _connections = connections;
    _hosts = hosts;
    _terminate = false;
    _pending = _peer = NULL;
    time(&_time);
}

Connection::~Connection() {
    pthread_kill(_network_listener, SIGTERM);
    close(_socket);
    _hosts->erase(_socket);
    if (_pending != NULL) {
        _pending->send_block(block_t(block_t::unavailable));
        _pending->log("unavailable host sent");
        _pending = NULL;
    }
    if (_peer != NULL) {
        pthread_kill(_peer->_network_listener, SIGTERM);
        _peer->send_block(block_t(block_t::disconnect));
        _peer->log("disconnect sent");
        _peer->_terminate = true;
        _peer = _peer->_peer = NULL;
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

    int hosts_size;
    connections_t::iterator connection;
    hosts_t::iterator host;
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
            if (block._cmd == block_t::name && !_pending && !_peer) {
                _name = (char *) block._data;
                log("set name to " + _name);
            } else if (block._cmd == block_t::host && !_pending && !_peer) {
                _hosts->insert(std::make_pair(_socket, &_name));
                log("added to hosts list");
            } else if (block._cmd == block_t::list && !_pending && !_peer) {
                hosts_size = _hosts->size();
                send_block(block_t(block_t::list, &hosts_size, sizeof hosts_size));
                for (host = _hosts->begin(); host != _hosts->end(); host++) {
                    send_block(block_t(block_t::list, &host->first, sizeof host->first));
                    send_block(block_t(block_t::list, host->second->c_str(), host->second->size() + 1));
                }
                log("hosts list sent");
            } else if (block._cmd == block_t::request && !_pending && !_peer) {
                connection = _connections->find(*(int *) block._data);
                host = _hosts->find(*(int *) block._data);
                if (connection != _connections->end() && host != _hosts->end()) {
                    _hosts->erase(connection->second->_socket);
                    connection->second->_pending = this;
                    connection->second->send_block(block_t(block_t::request, _name.c_str(), _name.size() + 1));
                    log("tried host " + connection->second->_name);
                } else {
                    send_block(block_t(block_t::unavailable));
                    log("tried unavailable host");
                }
            } else if (block._cmd == block_t::accept && _pending && !_peer) {
                _peer = _pending;
                _peer->_peer = this;
                _pending = NULL;
                _peer->send_block(block);
                log("accepted client " + _peer->_name);
            } else if (block._cmd == block_t::decline && _pending && !_peer) {
                _pending->send_block(block);
                log("declined client " + _pending->_name);
                _pending = NULL;
                _hosts->insert(std::make_pair(_socket, &_name));
            } else if (block._cmd == block_t::data && !_pending && _peer)
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
