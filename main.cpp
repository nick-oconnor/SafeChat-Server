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

#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <pthread.h>
#define __timeout 30
#define __block_size 1024
#define __null 0
#define __keep_alive 1
#define __set_name 2
#define __set_available 3
#define __get_hosts 4
#define __try_host 5
#define __decline_client 6
#define __accept_client 7
#define __send_data 8

class LocalHost {
private:

    class Block {
    public:

        unsigned char _data[__block_size];

        Block(int cmd) {
            memset(_data, '\0', __block_size);
            _data[0] = (unsigned char) cmd;
        }

        Block(int cmd, const std::string &string) {
            _data[0] = (unsigned char) cmd;
            strncpy((char *) &_data[1], string.c_str(), __block_size - 1);
        }

        Block(int cmd, const char *c_string) {
            _data[0] = (unsigned char) cmd;
            strncpy((char *) &_data[1], c_string, __block_size - 1);
        }

        int cmd() const {
            return (int) _data[0];
        }

        const unsigned char *bin() const {
            return &_data[1];
        }

        const std::string str() const {
            return std::string((char *) &_data[1]);
        }

        long num() const {
            return atol((char *) &_data[1]);
        }
    };

    class Connection {
    public:

        int _socket;
        bool _available, _terminated;
        std::string _name;
        std::map<int, Connection> *_connections;
        std::map<int, std::string> *_hosts;
        pthread_t _connection;
        time_t _time;
        Connection *_peer;

        Connection(int socket, std::map<int, Connection> *connections, std::map<int, std::string> *hosts) {
            _socket = socket;
            _connections = connections;
            _hosts = hosts;
            _available = _terminated = false;
            _peer = NULL;
            time(&_time);
        }

        static void *connection(void *connection) {
            return ((Connection *) connection)->connection();
        }

        void terminate() {
            if (_peer != NULL) {
                pthread_kill(_peer->_connection, SIGTERM);
                _peer->Send(Block(__null));
                _peer->_peer = NULL;
                _peer->_terminated = true;
                _peer = NULL;
            }
            _terminated = true;
            pthread_exit(NULL);
        }

    private:

        void *connection() {

            Block block(__null);

            signal(SIGTERM, exit);
            while (true) {
                Recv(block);
                if (block.cmd() == __keep_alive) {
                    time(&_time);
                }
                else if (block.cmd() == __set_name && _peer == NULL) {
                    _name = block.str();
                }
                else if (block.cmd() == __set_available && _peer == NULL) {
                    _hosts->insert(std::make_pair(_socket, _name));
                }
                else if (block.cmd() == __get_hosts && _peer == NULL) {

                    std::map<int, std::string>::iterator itr;

                    Send(Block(__null, int2str(_hosts->size())));
                    for (itr = _hosts->begin(); itr != _hosts->end(); itr++) {
                        Send(Block(__null, int2str(itr->first)));
                        Send(Block(__null, itr->second));
                    }
                }
                else if (block.cmd() == __try_host && _peer == NULL) {

                    std::map<int, Connection>::iterator itr;

                    itr = _connections->find(block.num());
                    if (itr == _connections->end()) {
                        terminate();
                    }
                    else {
                        itr->second.Send(Block(__null, int2str(_socket)));
                        itr->second.Send(Block(__null, _name));
                        _hosts->erase(itr->first);
                    }
                }
                else if (block.cmd() == __accept_client && _peer == NULL) {

                    std::map<int, Connection>::iterator itr;

                    itr = _connections->find(block.num());
                    if (itr == _connections->end()) {
                        terminate();
                    }
                    else {
                        _peer = &itr->second;
                        itr->second._peer = this;
                        _peer->Send(block);
                    }
                }
                else if (block.cmd() == __decline_client && _peer == NULL) {

                    std::map<int, Connection>::iterator itr;

                    itr = _connections->find(block.num());
                    if (itr == _connections->end()) {
                        terminate();
                    }
                    else {
                        itr->second.Send(block);
                        _hosts->insert(std::make_pair(_socket, _name));
                    }
                }
                else if (block.cmd() == __send_data && _peer != NULL) {
                    _peer->Send(block);
                }
                else {
                    terminate();
                }
            }
            return NULL;
        }

        void Send(const Block &input) {
            if (!send(_socket, input._data, __block_size, 0)) {
                terminate();
            }
        }

        Block &Recv(Block &output) {
            if (!recv(_socket, (void *) output._data, __block_size, MSG_WAITALL)) {
                terminate();
            }
            return output;
        }

        std::string int2str(long num) {

            std::stringstream string_stream;

            string_stream << num;
            return string_stream.str();
        }
    };

    int _port, _max_connections, _socket;
    std::map<int, Connection> _connections;
    std::map<int, std::string> _hosts;
    std::string _config_path;
    pthread_t _cleaner;

public:

    LocalHost(int argc, char *argv[]) {

        std::string line;

        _config_path = std::string(getenv("HOME")) + "/.safechat-server";
        std::ifstream config_stream(_config_path.c_str());
        if (config_stream) {
            while (std::getline(config_stream, line)) {
                if (line.substr(0, 5) == "port=") {
                    _port = atoi(line.substr(5).c_str());
                }
                else if (line.substr(0, 16) == "max_connections=") {
                    _max_connections = atoi(line.substr(16).c_str());
                }
            }
            config_stream.close();
        }
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "-p") && i + 1 < argc) {
                _port = atoi(argv[++i]);
            }
            else if (!strcmp(argv[i], "-c") && i + 1 < argc) {
                _max_connections = atoi(argv[++i]);
            }
            else if (!strcmp(argv[i], "--help")) {
                std::cerr << "SafeChat-Server - (c) 2012 Nicholas Pitt \nhttps://www.xphysics.net/\n\n\t-p <port> Port to bind to.\n\t\t\t(from 1 to 65535)\n\t-c <numb> Maximum number of connections.\n\t--help    Displays this message.\n";
                exit(EXIT_SUCCESS);
            }
            else {
                std::cerr << "Invalid argument (" << argv[i] << "). Enter \"safechat-server --help\" for details.\n";
                exit(EXIT_FAILURE);
            }
        }
        if (_port < 1 || _port > 65535) {
            std::cerr << "Invalid port number (-p). Enter \"safechat-server --help\" for details.\n";
            exit(EXIT_FAILURE);
        }
        if (_max_connections < 1) {
            std::cerr << "Invalid number of maximum connections (-c). Enter \"safechat-server --help\" for details.\n";
            exit(EXIT_FAILURE);
        }
    }

    ~LocalHost() {
        std::ofstream config_stream(_config_path.c_str());
        if (!config_stream) {
            std::cerr << "Error writing configuration file to " << _config_path << ".\n";
        }
        else {
            config_stream << "Configuration file for SafeChat-Server\n\nport=" << _port << "\nmax_connections=" << _max_connections;
            config_stream.close();
        }
    }

    void start_server() {

        socklen_t address_size = sizeof (sockaddr_in);
        sockaddr_in address;

        pthread_create(&_cleaner, NULL, &LocalHost::cleaner, this);
        _socket = socket(AF_INET, SOCK_STREAM, 0);
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(_port);
        if (bind(_socket, (sockaddr *) & address, sizeof (address))) {
            std::cerr << "Error binding to port " << _port << ".\n";
            raise(SIGINT);
        }
        listen(_socket, 3);
        while (true) {

            int socket;
            std::pair < std::map<int, Connection>::iterator, bool> pair;

            socket = accept(_socket, (sockaddr *) & address, &address_size);
            if (_connections.size() == (size_t) _max_connections) {
                close(socket);
            }
            else {
                pair = _connections.insert(std::make_pair(socket, Connection(socket, &_connections, &_hosts)));
                if (!pair.second) {
                    close(socket);
                }
                else {
                    pthread_create(&pair.first->second._connection, NULL, &Connection::connection, &pair.first->second);
                }
            }
        }
    }

    static void *cleaner(void *local_host) {
        return ((LocalHost *) local_host)->cleaner();
    }

    static void exit(int signal) {
        pthread_exit(NULL);
    }

private:

    void *cleaner() {

        double time_inactive;
        std::map<int, Connection>::iterator itr;

        signal(SIGTERM, exit);
        while (true) {
            sleep(1);
            itr = _connections.begin();
            while (itr != _connections.end()) {
                time_inactive = difftime(time(NULL), itr->second._time);
                if (itr->second._terminated) {
                    _connections.erase(itr++);
                }
                else if (time_inactive >= __timeout) {
                    itr->second.terminate();
                    _connections.erase(itr++);
                }
                else {
                    itr++;
                }
            }
        }
        return NULL;
    }
};

LocalHost *local_host;

void save_config_wrapper(int signal) {
    delete local_host;
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[]) {

    local_host = new LocalHost(argc, argv);

    signal(SIGINT, save_config_wrapper);
    local_host->start_server();
    delete local_host;
    return 0;
}
