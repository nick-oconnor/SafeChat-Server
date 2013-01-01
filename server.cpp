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

    try {
        _config_path = std::string(getenv("HOME")) + "/.safechat-server";
        config_file.open(_config_path.c_str());
        if (config_file) {
            while (std::getline(config_file, str))
                if (str.substr(0, 5) == "port=")
                    _port = atoi(str.substr(5).c_str());
                else if (str.substr(0, 12) == "max_sockets=")
                    _max_sockets = atoi(str.substr(12).c_str());
            config_file.close();
        }
        for (int i = 1; i < argc; i++) {
            str = argv[i];
            if (str == "-p" && i + 1 < argc)
                _port = atoi(argv[++i]);
            else if (str == "-s" && i + 1 < argc)
                _max_sockets = atoi(argv[++i]);
            else if (str == "-v") {
                std::cout << "SafeChat-Server version " << __version << "\n";
                exit(EXIT_SUCCESS);
            } else
                throw "unknown argument '" + std::string(argv[i]) + "'";
        }
        if (_port < 1 || _port > 65535)
            throw "invalid port number";
        if (_max_sockets < 1)
            throw "invalid number of max sockets";
    } catch (std::string error) {
        std::cout << "SafeChat-Server (version " << __version << ") - (c) 2012 Nicholas Pitt \nhttps://www.xphysics.net/\n\n    -p <port> Specifies the port the server binds to\n    -s <numb> Specifies the maximum number of sockets the server opens\n    -v Displays the version\n\n" << std::flush;
        std::cerr << "SafeChat-Server: " << error << "\n";
        exit(EXIT_FAILURE);
    }
}

Server::~Server() {

    std::ofstream config_file(_config_path.c_str());

    try {
        pthread_kill(_cleaner, SIGTERM);
        close(_socket);
        if (!config_file)
            throw "can't write " + _config_path;
        else {
            config_file << "Config file for SafeChat-Server\n\nport=" << _port << "\nmax_sockets=" << _max_sockets;
            config_file.close();
        }
    } catch (std::string error) {
        std::cerr << "Error: " << error << ".\n";
    }
}

void Server::start() {

    int new_socket;
    std::pair < std::map<int, Socket *>::iterator, bool> pair;
    socklen_t addr_size = sizeof (sockaddr_in);
    sockaddr_in addr;

    try {
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(_port);
        if (bind(_socket, (sockaddr *) & addr, sizeof addr))
            throw "can't bind to port " + _port;
        pthread_create(&_cleaner, NULL, &Server::cleaner, this);
        listen(_socket, 3);
        while (true) {
            new_socket = accept(_socket, (sockaddr *) & addr, &addr_size);
            pair = _sockets.insert(std::make_pair(new_socket, new Socket(new_socket, _sockets.size() >= (unsigned) _max_sockets ? true : false, &_sockets, &_hosts)));
            pthread_create(&pair.first->second->_listener, NULL, &Socket::listener, pair.first->second);
        }
    } catch (std::string error) {
        std::cerr << "Error: " << error << ".\n";
        exit(EXIT_FAILURE);
    }
}

void *Server::cleaner() {

    std::map<int, Socket *>::iterator itr;

    signal(SIGTERM, thread_handler);
    while (true) {
        sleep(1);
        itr = _sockets.begin();
        while (itr != _sockets.end())
            if (itr->second->_terminated) {
                delete itr->second;
                _sockets.erase(itr++);
            } else if (difftime(time(NULL), itr->second->_time) > __time_out) {
                itr->second->log("Connection timed out, listener terminated");
                pthread_kill(itr->second->_listener, SIGTERM);
                itr->second->terminate();
                delete itr->second;
                _sockets.erase(itr++);
            } else
                itr++;
    }
    return NULL;
}
