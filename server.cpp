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

    _config_path = std::string(getenv("HOME")) + "/.safechat-server";
    try {
        config_file.open(_config_path.c_str());
        if (!config_file)
            throw std::runtime_error("can't read configuration file");
        while (std::getline(config_file, string))
            if (string.substr(0, 5) == "port=")
                _port = atoi(string.substr(5).c_str());
            else if (string.substr(0, 16) == "max_connections=")
                _max_connections = atoi(string.substr(16).c_str());
        config_file.close();
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".\n";
    }
    try {
        for (int i = 1; i < argc; i++) {
            string = argv[i];
            if (string == "-p" && i + 1 < argc)
                _port = atoi(argv[++i]);
            else if (string == "-c" && i + 1 < argc)
                _max_connections = atoi(argv[++i]);
            else
                throw std::runtime_error("unknown argument " + std::string(argv[i]));
        }
        if (_port < 1 || _port > 65535)
            throw std::runtime_error("invalid port number");
        if (_max_connections < 1)
            throw std::runtime_error("invalid maximum number of connections");
    } catch (const std::exception &exception) {
        std::cout << "SafeChat-Server (version " << std::fixed << std::setprecision(1) << __version << ") - (c) 2013 Nicholas Pitt \nhttps://www.xphysics.net/\n\n    -p <port> Specifies the port the server binds to\n    -c <numb> Specifies the maximum number of connections\n" << std::endl;
        std::cerr << "Error: " << exception.what() << ".\n";
        exit(EXIT_FAILURE);
    }
}

Server::~Server() {

    std::ofstream config_file(_config_path.c_str());

    pthread_kill(_cleaner, SIGTERM);
    close(_socket);
    try {
        if (!config_file)
            throw std::runtime_error("can't write configuration file");
        config_file << "Configuration file for SafeChat-Server\n\nport=" << _port << "\nmax_connections=" << _max_connections;
        config_file.close();
    } catch (const std::exception &exception) {
        std::cerr << "\nError: " << exception.what() << ".";
    }
    std::cout << std::endl;
}

void Server::start() {

    int new_socket;
    std::pair < Connection::connections_t::iterator, bool> pair;
    sockaddr_in addr;
    socklen_t addr_size = sizeof (sockaddr_in);
    Connection *connection;

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
            new_socket = accept(_socket, (sockaddr *) & addr, &addr_size);
            if (_connections.size() < (unsigned) _max_connections)
                connection = new Connection(new_socket, false, &_connections, &_peers);
            else
                connection = new Connection(new_socket, true, &_connections, &_peers);
            pair = _connections.insert(std::make_pair(new_socket, connection));
            pair.first->second->start();
        }
    } catch (const std::exception &exception) {
        std::cerr << "Error: " << exception.what() << ".";
        this->~Server();
        exit(EXIT_FAILURE);
    }
}

void *Server::cleaner() {

    Connection::connections_t::iterator connection;

    signal(SIGTERM, thread_handler);
    while (true) {
        sleep(1);
        connection = _connections.begin();
        while (connection != _connections.end())
            if (connection->second->is_terminated()) {
                delete connection->second;
                _connections.erase(connection++);
            } else if (difftime(time(NULL), connection->second->get_time()) > __timeout) {
                connection->second->log("connection timed out");
                delete connection->second;
                _connections.erase(connection++);
            } else
                connection++;
    }
    return NULL;
}
