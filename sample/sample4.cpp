#include <stdlib.h>
#include <iostream>

// include libevent's header
#include <event.h>

// include libcage's header
#include <libcage/cage.hpp>

const int max_node = 100;
const int port     = 10000;
libcage::cage *cage;
event *ev;

void
print_id(uint8_t *id)
{
        printf("%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x" \
               "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\n",
               id[ 0], id[ 1], id[ 2], id[ 3], id[ 4],
               id[ 5], id[ 6], id[ 7], id[ 8], id[ 9],
               id[10], id[11], id[12], id[13], id[14],
               id[15], id[16], id[17], id[18], id[19]);
}

void
recv_dgram(void *buf, size_t len, uint8_t *id)
{
        printf("recv datagram: from = ");
        print_id(id);
}

// callback for timer
void
timer_callback(int fd, short ev, void *arg)
{
        int n1, n2;

        n1 = abs(mrand48()) % max_node;
        n2 = abs(mrand48()) % max_node;


        // send datagram
        uint8_t  id[CAGE_ID_LEN];
        char    *str = "Hello, world!";

        cage[n2].get_id(id);

        printf("send datagram: to = ");
        print_id(id);

        cage[n1].send_dgram(str, strlen(str), id);


        // reschedule
        timeval tval;

        tval.tv_sec  = 1;
        tval.tv_usec = 0;

        evtimer_add(::ev, &tval);
}

class join_callback
{
public:
        int n;

        void operator() (bool result)
        {
                // print state
                if (result)
                        std::cout << "join: successed, n = "
                                  << n
                                  << std::endl;
                else
                        std::cout << "join: failed, n = "
                                  << n
                                  << std::endl;

                cage[n].print_state();

                // put data
                cage[n].put(&n, sizeof(n), &n, sizeof(n), 300);

                n++;

                if (n < max_node) {
                        // start nodes recursively
                        if (! cage[n].open(PF_INET, port + n)) {
                                std::cerr << "cannot open port: Port = "
                                          << port + n
                                          << std::endl;
                                return;
                        }

                        cage[n].set_dgram_callback(recv_dgram);
                        cage[n].join("localhost", 10000, *this);
                } else {
                        // start timer
                        timeval tval;

                        ev = new event;

                        tval.tv_sec  = 1;
                        tval.tv_usec = 0;

                        evtimer_set(ev, timer_callback, NULL);
                        evtimer_add(ev, &tval);
                }
        }
};

int
main(int argc, char *argv[])
{
#ifdef WIN32
        // initialize winsock
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0), &wsaData);
#endif // WIN32

        // initialize libevent
        event_init();


        srand48(time(NULL));


        cage = new libcage::cage[max_node];

        // start bootstrap node
        if (! cage[0].open(PF_INET, port)) {
                std::cerr << "cannot open port: Port = "
                          << port
                          << std::endl;
                return -1;
        }
        cage[0].set_global();


        // start other nodes
        join_callback func;
        func.n = 1;

        if (! cage[1].open(PF_INET, port + func.n)) {
                std::cerr << "cannot open port: Port = "
                          << port + func.n
                          << std::endl;
                return -1;
        }
        cage[1].set_global();
        cage[1].join("localhost", 10000, func);


        // handle event loop
        event_dispatch();

        return 0;
}
