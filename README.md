SafeChat-Server is server software required by the SafeChat client.

Dependencies:

    pthread development library

Build commands:

    make build - compiles and links the binary
    make install - installs the binary to /usr/bin
    make remove - removes the binary from /usr/bin
    make clean - removes object and binary files
    make {main|server|connection} - recompiles that specific part of the program
