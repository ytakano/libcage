#include <stdlib.h>
#include <unistd.h>
#include <iostream>

#include <boost/foreach.hpp>

// include libevent's header
#include <event.h>

// include libcage's header
#include <libcage/cage.hpp>

const int max_node = 50;
const int port     = 20000;
const int proc_num = 200;

libcage::cage *cage;
event   *ev;
int      proc = -1;
timeval  tval1;
int      get_key;

// callback function for get
void
get_func(bool result, libcage::dht::value_set_ptr vset)
{
        double  diff;
        timeval tval2;
        gettimeofday(&tval2, NULL);

        diff  = tval2.tv_sec - tval1.tv_sec;
        diff += tval2.tv_usec / 1000000.0 - tval1.tv_usec / 1000000.0;

        if (result) {
                std::cout << "succeeded in getting: sec = " << diff
                          << "[s], key = " << get_key << ", values =";

                libcage::dht::value_set::iterator it;
                BOOST_FOREACH(const libcage::dht::value_t &val, *vset) {
                        std::cout << " " <<  *(int*)val.value.get() << ",";
                }
                std::cout << std::endl;
        } else {
                std::cout << "failed in getting: sec = " << diff << "[s]"
                          << ", key = " << get_key << std::endl;
        }

        timeval tval;

        tval.tv_sec  = abs(mrand48()) % proc_num + 1;
        tval.tv_usec = 0;

        evtimer_add(ev, &tval);
}

// callback for timer
void
timer_callback(int fd, short ev, void *arg)
{
        int n;

        n = abs(mrand48()) % max_node;

        for (;;) {
                get_key = abs(mrand48()) % (max_node * proc_num);
                if (get_key != 0)
                        break;
        }

        // get at random
        std::cout << "get: proc = " << proc << ", n = " << n << std::endl;
        gettimeofday(&tval1, NULL);

        char buf[1024 * 4];
        memset(buf, 0, sizeof(buf));
        *(int*)buf = get_key;

        cage[n].get(buf, sizeof(buf), get_func);
}

class join_callback
{
public:
        int idx;
        int n;

        void operator() (bool result)
        {
                // print state
                if (result) {
                        std::cout << "join: succeeded, n = "
                                  << n
                                  << std::endl;
                } else {
                        std::cout << "join: failed, n = "
                                  << n
                                  << std::endl;

                        cage[idx].join("localhost", port, *this);
                        return;
                }

                // put data
                char buf[1024 * 4];
                memset(buf, 0, sizeof(buf));
                *(int*)buf = n;
                cage[idx].put(buf, sizeof(buf), &n, sizeof(n), 60000);

                n++;
                idx++;

                if (idx < max_node) {
                        // start nodes recursively
                        if (! cage[idx].open(PF_INET, port + n, false)) {
                                std::cerr << "cannot open port: Port = "
                                          << port + n
                                          << std::endl;
                                return;
                        } else {
                                std::cerr << "open port: Port = "
                                          << port + n
                                          << std::endl;
                        }

                        cage[idx].join("localhost", port, *this);
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

        time_t t = time(NULL) - 1;
        int   i = 0;
        pid_t pid;

        pid = fork();

        if (pid == 0) {
                event_init();

                cage = new libcage::cage;

                srand48(t);

                if (! cage->open(PF_INET, port, false)) {
                        std::cerr << "cannot open port: Port = "
                                  << port
                                  << std::endl;
                        return -1;
                }

                event_dispatch();

                return 0;
        }

        sleep(1);
                
        for (i = 0; i < proc_num; i++) {
                pid = fork();
                if (pid == 0) {
                        event_init();

                        cage = new libcage::cage[max_node];

                        srand48(t + i + 1);

                        proc = i;

                        // start other nodes
                        join_callback func;
                        func.n   = proc * max_node + 1;
                        func.idx = 0;

                        if (! cage[0].open(PF_INET, port + func.n, false)) {
                                std::cerr << "cannot open port: Port = "
                                          << port + func.n
                                          << std::endl;
                                return -1;
                        }
                        cage[0].join("localhost", port, func);

                        event_dispatch();

                        return 0;
                }
        }

        for (;;) {
                sleep(10000);
        }

        return 0;
}
