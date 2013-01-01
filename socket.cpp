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

Socket::Socket(int id, int socket, socket_t *sockets, host_t *hosts) {
    _id = id;
    _socket = socket;
    _sockets = sockets;
    _hosts = hosts;
    _terminated = false;
    _peer = NULL;
    time(&_time);
}

void Socket::terminate() {

    Block block(0, NULL, 0);

    _hosts->erase(_id);
    if (_peer != NULL) {
        _peer->log("disconnect sent");
        _peer->send_block(block.set(__disconnect, NULL, 0));
        pthread_kill(_peer->_listener, SIGTERM);
        close(_peer->_socket);
        _peer->_terminated = true;
        _peer->_peer = NULL;
        _peer = NULL;
    }
    close(_socket);
    _terminated = true;
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

    try {
        signal(SIGTERM, thread_handler);
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
            if (block._cmd == __name && _peer == NULL) {
                _name = (char *) block._data;
                log("set name to " + _name);
            } else if (block._cmd == __available && _peer == NULL) {
                _hosts->insert(std::make_pair(_id, _name));
                log("set as host");
            } else if (block._cmd == __hosts && _peer == NULL) {
                hosts_size = _hosts->size();
                send_block(block.set(0, &hosts_size, sizeof hosts_size));
                for (host = _hosts->begin(); host != _hosts->end(); host++) {
                    send_block(block.set(0, &host->first, sizeof host->first));
                    send_block(block.set(0, host->second.c_str(), host->second.size() + 1));
                }
                log("hosts list sent");
            } else if (block._cmd == __try && _peer == NULL) {
                socket = _sockets->find(*(int *) block._data);
                host = _hosts->find(*(int *) block._data);
                if (socket != _sockets->end() && host != _hosts->end()) {
                    _hosts->erase(socket->second->_id);
                    socket->second->send_block(block.set(0, &_id, sizeof _id));
                    socket->second->send_block(block.set(0, _name.c_str(), _name.size() + 1));
                    log("trying " + socket->second->_name);
                }
            } else if (block._cmd == __accept && _peer == NULL) {
                socket = _sockets->find(*(int *) block._data);
                if (socket != _sockets->end()) {
                    _peer = socket->second;
                    socket->second->_peer = this;
                    _peer->send_block(block.set(__accept, NULL, 0));
                    log("accepted client " + socket->second->_name);
                }
            } else if (block._cmd == __decline && _peer == NULL) {
                socket = _sockets->find(*(int *) block._data);
                if (socket != _sockets->end()) {
                    socket->second->send_block(block.set(__decline, NULL, 0));
                    _hosts->insert(std::make_pair(_id, _name));
                    log("declined client " + socket->second->_name);
                }
            } else if (block._cmd == __data && _peer != NULL)
                _peer->send_block(block);
            else if (block._cmd != __keepalive)
                throw std::runtime_error("disconnect received");
        }
    } catch (const std::exception &exception) {
        log(exception.what());
        terminate();
        pthread_exit(NULL);
    }
    return NULL;
}
