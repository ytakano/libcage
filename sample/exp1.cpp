#include <stdlib.h>
#include <iostream>

#include <sys/types.h>
#include <unistd.h>

// include libevent's header
#include <event.h>

// include libcage's header
#include <libcage/cage.hpp>

int max_node = 100;
const int max_process = 1;

const int port     = 10000;
libcage::cage *boot;
libcage::cage *cage;

const char *host;
int host_port = 10000;

double t1;

event ev;

int count = 0;
const int max_count = 1000;

void timer_callback(int fd, short ev, void *arg);
void timer_get(int fd, short ev, void *arg);

double
gettimeofday_sec()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + (double)tv.tv_usec*1e-6;
}

class join_callback
{
public:
        int n;
        int p;

        void operator() (bool result)
        {
                // print state
                if (result) {
                        std::cerr << "join: successed, n = "
                                  << n
                                  << std::endl;
                } else  {
                        std::cerr << "join: failed, n = "
                                  << n
                                  << std::endl;

                        //cage[n].join("localhost", 10000, *this);
                        //return;
                }

                int v = p * max_node + n;
                cage[n].put(&v, sizeof(v), &v, sizeof(v), (1 << 16) - 1);

                n++;

                if (n < max_node) {
                        // start nodes recursively
                        timeval tval;

                        tval.tv_sec  = 0;
                        tval.tv_usec = 400000;

                        join_callback *func = new join_callback;

                        *func = *this;

                        evtimer_set(&ev, timer_callback, func);
                        evtimer_add(&ev, &tval);
                } else {
                        // start timer
                        timeval tval;

                        tval.tv_sec  = 1;
                        tval.tv_usec = 0;

                        evtimer_set(&ev, timer_get, NULL);
                        evtimer_add(&ev, &tval);
                }
        }
};

class get_callback {
public:
        void operator() (bool result, void *buf, int len)
        {
                double t2 = gettimeofday_sec();

                if (result) {
                        double diff = t2 - t1;
                        std::cout << diff << std::endl;
                }

                if (count > max_count)
                        exit(0);

                timeval tval;

                tval.tv_sec  = 1;
                tval.tv_usec = 0;

                evtimer_set(&ev, timer_get, NULL);
                evtimer_add(&ev, &tval);
        }
};


join_callback func;

// callback for join
void
timer_callback(int fd, short ev, void *arg)
{
        join_callback *func = (join_callback*)arg;

        if (! cage[func->n].open(PF_INET, 0)) {
                std::cerr << "cannot open port"
                          << std::endl;
                return;
        }
        
        cage[func->n].set_global();
        cage[func->n].join(host, host_port, *func);

        delete func;
}

void
timer_get(int fd, short ev, void *arg)
{
        uint32_t n1, n2, n3;

        n1  = mrand48();
        n1 %= max_process;
        n1  = n1 > 0 ? n1 : -n1;

        n2  = mrand48();
        n2 %= max_node;
        n2  = n2 > 0 ? n2 : -n2;

        n3  = mrand48();
        n3 %= max_node;
        n3  = n2 > 0 ? n2 : -n2;

        int n = n1 * max_node + n2;

        get_callback func;

        t1 = gettimeofday_sec();

        count++;

        cage[n3].get(&n, sizeof(n), func);
}


int
main(int argc, char *argv[])
{
        if (argc < 4)
                return -1;

        max_node = atoi(argv[1]);
        host = argv[2];
        host_port = atoi(argv[3]);

        int i;

        // fork
        for (i = 0; i < max_process; i++) {
                pid_t pid = fork();
                if (pid > 0)
                        break;
        }


        // initialize libevent
        event_init();


        // start other nodes
        cage = new libcage::cage[max_node];

        join_callback func;

        func.n = 0;
        func.p = i;

        if (! cage[0].open(PF_INET, 0)) {
                std::cerr << "cannot open port"
                          << std::endl;
                return -1;
        }
        cage[0].set_global();
        cage[0].join(host, host_port, func);

        // handle event loop
        event_dispatch();

        return 0;
}
