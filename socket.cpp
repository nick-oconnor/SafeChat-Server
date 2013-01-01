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

Socket::Socket(int socket, bool full, socket_t *sockets, host_t *hosts) {
    _version = __version;
    _socket = socket;
    _full = full;
    _sockets = sockets;
    _hosts = hosts;
    _terminated = false;
    _peer = NULL;
    time(&_time);
}

void Socket::terminate() {

    Block block(0, NULL, 0);

    _hosts->erase(_socket);
    if (_peer != NULL) {
        _peer->log("Disconnect command sent, listener terminated");
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

void Socket::log(const std::string &str) {

    time_t time_t = time(NULL);
    struct tm *tm = localtime(&time_t);
    char *time = asctime(tm);

    time[strlen(time) - 1] = '\0';
    std::cout << time << " <" << _socket << "> " << str << "\n" << std::flush;
}

void *Socket::listener() {

    Block block(0, NULL, 0);

    signal(SIGTERM, thread_handler);
    log("Connection established, listener created");
    send_block(block.set(0, &_version, sizeof _version));
    send_block(block.set(0, &_full, sizeof _full));
    try {
        if (_full)
            throw std::runtime_error("Server full, connection suspended");
        while (true)
            try {
                if (!recv(_socket, &block._cmd, sizeof block._cmd, MSG_WAITALL))
                    throw std::runtime_error("Dropped connection, listener terminated");
                if (!recv(_socket, &block._size, sizeof block._size, MSG_WAITALL))
                    throw std::runtime_error("Dropped connection, listener terminated");
                if (block._size > __max_block_size)
                    throw std::runtime_error("Oversized block received, listener terminated");
                block.set(block._cmd, NULL, block._size);
                if (block._size)
                    if (!recv(_socket, block._data, block._size, MSG_WAITALL))
                        throw std::runtime_error("Dropped connection, listener terminated");
                time(&_time);
                if (block._cmd == __set_name && _peer == NULL) {
                    _name = std::string(block._data);
                    log("Set name to: " + _name);
                } else if (block._cmd == __set_available && _peer == NULL) {
                    _hosts->insert(std::make_pair(_socket, _name));
                    log("Set as host");
                } else if (block._cmd == __get_hosts && _peer == NULL) {

                    int hosts_size = _hosts->size();
                    host_t::iterator itr;

                    send_block(block.set(0, &hosts_size, sizeof hosts_size));
                    for (itr = _hosts->begin(); itr != _hosts->end(); itr++) {
                        send_block(block.set(0, &itr->first, sizeof itr->first));
                        send_block(block.set(0, itr->second.c_str(), itr->second.size() + 1));
                    }
                    log("Hosts list sent");
                } else if (block._cmd == __try_host && _peer == NULL) {

                    socket_t::iterator itr;

                    itr = _sockets->find(*(int *) block._data);
                    if (itr != _sockets->end()) {
                        itr->second->send_block(block.set(0, &_socket, sizeof _socket));
                        itr->second->send_block(block.set(0, _name.c_str(), _name.size() + 1));
                        _hosts->erase(itr->first);
                        log("Trying host: " + itr->second->_name);
                    }
                } else if (block._cmd == __accept_client && _peer == NULL) {

                    socket_t::iterator itr;

                    itr = _sockets->find(*(int *) block._data);
                    if (itr != _sockets->end()) {
                        _peer = itr->second;
                        itr->second->_peer = this;
                        _peer->send_block(block);
                        log("Accepted client: " + itr->second->_name);
                    }
                } else if (block._cmd == __decline_client && _peer == NULL) {

                    socket_t::iterator itr;

                    itr = _sockets->find(*(int *) block._data);
                    if (itr != _sockets->end()) {
                        itr->second->send_block(block);
                        _hosts->insert(std::make_pair(_socket, _name));
                        log("Declined client: " + itr->second->_name);
                    }
                } else if (block._cmd == __send_data && _peer != NULL)
                    _peer->send_block(block);
                else if (block._cmd != __keep_alive)
                    throw std::runtime_error("Disconnect command received, listener terminated");
            } catch (const std::exception &exception) {
                log(exception.what());
                terminate();
                pthread_exit(NULL);
            }
    } catch (const std::exception &exception) {
        log(exception.what());
        pthread_exit(NULL);
    }
    return NULL;
}

void Socket::send_block(Block &block) {
    send(_socket, &block._cmd, sizeof block._cmd, 0);
    send(_socket, &block._size, sizeof block._size, 0);
    if (block._size)
        send(_socket, block._data, block._size, 0);
}
