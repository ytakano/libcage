#include <stdlib.h>
#include <iostream>

#include <boost/foreach.hpp>

// include libevent's header
#include <event.h>

// include libcage's header
#include <libcage/cage.hpp>

const int max_node = 20;
const int port     = 10000;
libcage::cage *cage;
event *ev;

// callback function for get

// dht::value_t       --- class {
//                         public:
//                                 boost::shared_array<char> value;
//                                 int len;
//                        };
// dht::value_set     --- boost::unordered_set<dht::value_t>
// dht::value_set_ptr --- boost::shared_ptr<dht::value_set>
void
get_func(bool result, libcage::dht::value_set_ptr vset)
{
        if (result) {
                printf("succeeded in getting: n =");

                libcage::dht::value_set::iterator it;
                BOOST_FOREACH(const libcage::dht::value_t &val, *vset) {
                        printf(" %d", *(int*)val.value.get());
                }
                printf("\n");
        } else {
                printf("failed in getting:\n");
        }

        timeval tval;

        tval.tv_sec  = 1;
        tval.tv_usec = 0;

        evtimer_add(ev, &tval);
}

// callback for timer
void
timer_callback(int fd, short ev, void *arg)
{
        int n1, n2;

        n1 = abs(mrand48()) % max_node;

        for (;;) {
                n2 = abs(mrand48()) % max_node;
                if (n2 != 0)
                        break;
        }

        // get at random
        printf("get %d\n", n2);
        cage[n1].get(&n2, sizeof(n2), get_func);
}

class join_callback
{
public:
        int n;

        void operator() (bool result)
        {
                // print state
                if (result)
                        std::cout << "join: succeeded, n = "
                                  << n
                                  << std::endl;
                else
                        std::cout << "join: failed, n = "
                                  << n
                                  << std::endl;

                //cage[n].print_state();

                // put data
                cage[n].put(&n, sizeof(n), &n, sizeof(n), 30000);

                n++;

                if (n < max_node) {
                        // start nodes recursively
                        if (! cage[n].open(PF_INET, port + n)) {
                                std::cerr << "cannot open port: Port = "
                                          << port + n
                                          << std::endl;
                                return;
                        }

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
