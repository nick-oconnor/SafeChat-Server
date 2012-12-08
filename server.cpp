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

Server::Server(int argc, char *argv[]) {

    std::string str;
    std::ifstream config_file;

    _config_path = std::string(getenv("HOME")) + "/.SafeChat-server";
    config_file.open(_config_path.c_str());
    if (config_file) {
        while (std::getline(config_file, str)) {
            if (str.substr(0, 5) == "port=") {
                _port = atoi(str.substr(5).c_str());
            } else if (str.substr(0, 12) == "max_sockets=") {
                _max_sockets = atoi(str.substr(12).c_str());
            }
        }
        config_file.close();
    }
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p") && i + 1 < argc) {
            _port = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-c") && i + 1 < argc) {
            _max_sockets = atoi(argv[++i]);
        } else if (!strcmp(argv[i], "-v")) {
            std::cout << "SafeChat-Server version " << __version << "\n";
            exit(EXIT_SUCCESS);
        } else {
            print_help();
            std::cerr << "SafeChat-Server: Unknown argument '" << argv[i] << "'\n";
            exit(EXIT_FAILURE);
        }
    }
    if (_port < 1 || _port > 65535) {
        print_help();
        std::cerr << "SafeChat-Server: Invalid port number\n";
        exit(EXIT_FAILURE);
    }
    if (_max_sockets < 1) {
        print_help();
        std::cerr << "SafeChat-Server: Invalid number of maximum connections\n";
        exit(EXIT_FAILURE);
    }
}

Server::~Server() {

    std::ofstream config_file(_config_path.c_str());

    pthread_kill(_cleaner, SIGTERM);
    close(_socket);
    if (!config_file) {
        std::cerr << "Error: can't write '" << _config_path << "'.\n";
    } else {
        config_file << "Config file for SafeChat-Server\n\nport=" << _port << "\nmax_sockets=" << _max_sockets;
        config_file.close();
    }
}

void Server::start() {

    int new_socket;
    std::pair < std::map<int, Socket *>::iterator, bool> pair;
    socklen_t addr_size = sizeof (sockaddr_in);
    sockaddr_in addr;

    _socket = socket(AF_INET, SOCK_STREAM, 0);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(_port);
    if (bind(_socket, (sockaddr *) & addr, sizeof addr)) {
        std::cerr << "Error: can't bind to port " << _port << ".\n";
        exit(EXIT_FAILURE);
    }
    pthread_create(&_cleaner, NULL, &Server::cleaner, this);
    listen(_socket, 3);
    while (true) {
        new_socket = accept(_socket, (sockaddr *) & addr, &addr_size);
        pair = _sockets.insert(std::make_pair(new_socket, new Socket(new_socket, _sockets.size() == (unsigned) _max_sockets ? true : false, &_sockets, &_hosts)));
        pthread_create(&pair.first->second->_listener, NULL, &Socket::listener, pair.first->second);
    }
}

void *Server::cleaner() {

    std::map<int, Socket *>::iterator itr;

    signal(SIGTERM, thread_handler);
    while (true) {
        sleep(1);
        itr = _sockets.begin();
        while (itr != _sockets.end()) {
            if (itr->second->_terminate || difftime(time(NULL), itr->second->_time) > __timeout) {
                delete itr->second;
                _sockets.erase(itr++);
            } else {
                itr++;
            }
        }
    }
    return NULL;
}

void Server::print_help() {
    std::cout << "SafeChat-Server (version " << __version << ") - (c) 2012 Nicholas Pitt \nhttps://www.xphysics.net/\n\n    -p <port> Specify the port the server binds to\n    -c <numb> Specify the maximum number of connections the server accepts\n    -v Displays the version\n\n" << std::flush;
}
