
#include <stdlib.h>
#include <iostream>

// include libevent's header
#include <event.h>

// include libcage's header
#include <libcage/cage.hpp>

libcage::cage *cage;

void
join_callback(bool result)
{
        if (result)
                std::cout << "join: successed" << std::endl;
        else
                std::cout << "join: failed" << std::endl;

        cage->print_state();
}


int
main(int argc, char *argv[])
{
        if (argc < 2) {
                std::cerr << "usage: ./sample1 port [host port]\n\n"
                          << "example:\n"
                          << "  $ ./sample1 10000 &\n"
                          << "  $ ./sample1 10001 localhost 10000\n"
                          << std::endl;

                return -1;
        }

        // initialize

#ifdef WIN32
        // initialize winsock
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0), &wsaData);
#endif // WIN32

        // initialize libevent
        event_init();



        // create cage instance after initialize
        cage = new libcage::cage;

        int           port;
        port = atoi(argv[1]);

        // open UDP
        if (! cage->open(PF_INET, port)) {
                std::cerr << "cannot open port: Port = "
                          << port
                          << std::endl;
                return -1;
        }

        // set as global node
        cage->set_global();

        if (argc >= 4) {
                // join to the network
                int dst_port = atoi(argv[3]);
                cage->join(argv[2], dst_port, &join_callback);
        }


        // handle event loop
        event_dispatch();

        return 0;
}
