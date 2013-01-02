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

#include "server.h"

Server::Server(int argc, char **argv) {

    std::string string;
    std::ifstream config_file;

    try {
        _config_path = std::string(getenv("HOME")) + "/.safechat-server";
        config_file.open(_config_path.c_str());
        if (!config_file)
            throw std::runtime_error("can't read config file");
        while (std::getline(config_file, string))
            if (string.substr(0, 5) == "port=")
                _port = atoi(string.substr(5).c_str());
            else if (string.substr(0, 12) == "max_sockets=")
                _max_sockets = atoi(string.substr(12).c_str());
        config_file.close();
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
    }
    try {
        for (int i = 1; i < argc; i++) {
            string = argv[i];
            if (string == "-p" && i + 1 < argc)
                _port = atoi(argv[++i]);
            else if (string == "-s" && i + 1 < argc)
                _max_sockets = atoi(argv[++i]);
            else
                throw std::runtime_error("unknown argument " + std::string(argv[i]));
        }
        if (_port < 1 || _port > 65535)
            throw std::runtime_error("invalid port number");
        if (_max_sockets < 1)
            throw std::runtime_error("invalid number of max sockets");
    } catch (const std::exception &exception) {
        std::cout << "SafeChat-Server (version " << __version << ") - (c) 2012 Nicholas Pitt \nhttps://www.xphysics.net/\n\n    -p <port> Specifies the port the server binds to\n    -s <numb> Specifies the maximum number of sockets the server opens\n\n" << std::flush;
        std::cerr << "Error: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
}

Server::~Server() {

    std::ofstream config_file(_config_path.c_str());

    try {
        std::cout << "\n" << std::flush;
        pthread_kill(_cleaner, SIGTERM);
        close(_socket);
        if (!config_file)
            throw std::runtime_error("can't write config file");
        config_file << "Config file for SafeChat-Server\n\nport=" << _port << "\nmax_sockets=" << _max_sockets;
        config_file.close();
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
    }
}

int Server::start() {

    int socket_num, id = 1;
    std::pair < Socket::socket_t::iterator, bool> pair;
    sockaddr_in addr;
    socklen_t addr_size = sizeof (sockaddr_in);
    Socket *socket_ptr;

    try {
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(_port);
        if (bind(_socket, (sockaddr *) & addr, sizeof addr))
            throw std::runtime_error("can't bind to port");
        pthread_create(&_cleaner, NULL, &Server::cleaner, this);
        listen(_socket, 3);
        while (true) {
            socket_num = accept(_socket, (sockaddr *) & addr, &addr_size);
            if (_sockets.size() < (unsigned) _max_sockets)
                socket_ptr = new Socket(id, socket_num, false, &_sockets, &_hosts);
            else
                socket_ptr = new Socket(id, socket_num, true, &_sockets, &_hosts);
            pair = _sockets.insert(std::make_pair(id, socket_ptr));
            pair.first->second->start();
            id++;
        }
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}

void *Server::cleaner() {

    Socket::socket_t::iterator socket;

    signal(SIGTERM, thread_handler);
    while (true) {
        sleep(1);
        socket = _sockets.begin();
        while (socket != _sockets.end())
            if (socket->second->_delete) {
                delete socket->second;
                _sockets.erase(socket++);
            } else if (difftime(time(NULL), socket->second->_time) > __timeout) {
                socket->second->log("connection timed out");
                delete socket->second;
                _sockets.erase(socket++);
            } else
                socket++;
    }
    return NULL;
}
