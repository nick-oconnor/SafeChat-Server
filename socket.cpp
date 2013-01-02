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

#include "socket.h"

Socket::Socket(int id, int socket, bool full, socket_t *sockets, host_t *hosts) {
    _id = id;
    _socket = socket;
    _full = full;
    _sockets = sockets;
    _hosts = hosts;
    _delete = false;
    _pending = _peer = NULL;
    time(&_time);
}

Socket::~Socket() {
    pthread_kill(_listener, SIGTERM);
    _hosts->erase(_id);
    close(_socket);
    log("socket closed");
    clean();
}

void Socket::start() {

    int protocol = __version;
    Block block(0, NULL, 0);

    log("socket opened");
    send_block(block.set(0, &protocol, sizeof protocol));
    send_block(block.set(0, &_full, sizeof _full));
    if (_full)
        _delete = true;
    else
        pthread_create(&_listener, NULL, &Socket::listener, this);
}

void Socket::clean() {

    Block block(0, NULL, 0);

    if (_pending != NULL) {
        pthread_kill(_pending->_listener, SIGTERM);
        _pending->send_block(block.set(__disconnect, NULL, 0));
        _pending->log("disconnect sent");
        close(_pending->_socket);
        _pending->_delete = true;
        _pending = _pending->_pending = NULL;
    }
    if (_peer != NULL) {
        pthread_kill(_peer->_listener, SIGTERM);
        _peer->send_block(block.set(__disconnect, NULL, 0));
        _peer->log("disconnect sent");
        close(_peer->_socket);
        _peer->_delete = true;
        _peer = _peer->_peer = NULL;
    }
}

void Socket::send_block(const Block &block) {
    send(_socket, &block._cmd, sizeof block._cmd, 0);
    send(_socket, &block._size, sizeof block._size, 0);
    if (block._size)
        send(_socket, block._data, block._size, 0);
}

void Socket::log(const std::string &string) {

    time_t time_t = time(NULL);
    struct tm *tm = localtime(&time_t);
    char *time = asctime(tm);

    time[strlen(time) - 1] = '\0';
    std::cout << time << " Socket " << _id << ": " << string << "\n" << std::flush;
}

void *Socket::listener() {

    int hosts_size;
    host_t::iterator host;
    socket_t::iterator socket;
    Block block(0, NULL, 0);

    signal(SIGTERM, thread_handler);
    try {
        while (true) {
            if (!recv(_socket, &block._cmd, sizeof block._cmd, MSG_WAITALL))
                throw std::runtime_error("dropped connection");
            if (!recv(_socket, &block._size, sizeof block._size, MSG_WAITALL))
                throw std::runtime_error("dropped connection");
            if (block._size > __max_block_size)
                throw std::runtime_error("oversized block received");
            block.set(block._cmd, NULL, block._size);
            if (block._size)
                if (!recv(_socket, block._data, block._size, MSG_WAITALL))
                    throw std::runtime_error("dropped connection");
            time(&_time);
            if (block._cmd == __name && _pending == NULL && _peer == NULL) {
                _name = (char *) block._data;
                log("set name to " + _name);
            } else if (block._cmd == __available && _pending == NULL && _peer == NULL) {
                _hosts->insert(std::make_pair(_id, &_name));
                log("set as host");
            } else if (block._cmd == __hosts && _pending == NULL && _peer == NULL) {
                hosts_size = _hosts->size();
                send_block(block.set(0, &hosts_size, sizeof hosts_size));
                for (host = _hosts->begin(); host != _hosts->end(); host++) {
                    send_block(block.set(0, &host->first, sizeof host->first));
                    send_block(block.set(0, host->second->c_str(), host->second->size() + 1));
                }
                log("hosts list sent");
            } else if (block._cmd == __try && _pending == NULL && _peer == NULL) {
                socket = _sockets->find(*(int *) block._data);
                host = _hosts->find(*(int *) block._data);
                if (socket != _sockets->end() && host != _hosts->end()) {
                    _hosts->erase(socket->second->_id);
                    _pending = socket->second;
                    socket->second->_pending = this;
                    socket->second->send_block(block.set(0, &_id, sizeof _id));
                    socket->second->send_block(block.set(0, _name.c_str(), _name.size() + 1));
                    log("trying " + socket->second->_name);
                }
            } else if (block._cmd == __accept && _pending != NULL && _peer == NULL) {
                if (!*(bool *) block._data) {
                    _pending->send_block(block);
                    log("declined client " + _pending->_name);
                    _pending = socket->second->_pending = NULL;
                    _hosts->insert(std::make_pair(_id, &_name));
                } else {
                    _peer = _pending;
                    _peer->_peer = _peer->_pending;
                    _pending = _peer->_pending = NULL;
                    _peer->send_block(block);
                    log("accepted client " + _peer->_name);
                }
            } else if (block._cmd == __data && _pending == NULL && _peer != NULL)
                _peer->send_block(block);
            else if (block._cmd != __keepalive)
                throw std::runtime_error("disconnect received");
        }
    } catch (const std::exception &exception) {
        log(exception.what());
        clean();
        _delete = true;
    }
    return NULL;
}
