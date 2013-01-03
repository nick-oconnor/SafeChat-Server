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

#ifndef server_h
#define	server_h

#include <fstream>
#include <iomanip>
#include <stdlib.h>
#include "connection.h"

class Server {
public:

    Server(int argc, char **argv);
    ~Server();

    int start();

    static void *cleaner(void *server) {
        return ((Server *) server)->cleaner();
    }

    static void thread_handler(int signal) {
        pthread_exit(NULL);
    }

private:

    int _port, _max_connections, _socket;
    std::string _config_path;
    Connection::connections_t _connections;
    Connection::hosts_t _hosts;
    pthread_t _cleaner;

    void *cleaner();

};

#endif
