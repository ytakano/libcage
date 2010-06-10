#include <event.h>

#include <libcage/cage.hpp>

class server_callback;
class client_callback;

class server_callback {
public:
        libcage::cage &m_cage;

        server_callback(libcage::cage &c) : m_cage(c) { }

        void operator() (int desc, libcage::rdp_addr addr,
                         libcage::rdp_event event);
};

class client_callback {
public:
        libcage::cage  &m_cage;

        client_callback(libcage::cage &c) : m_cage(c) { }

        void operator() (int desc, libcage::rdp_addr addr,
                         libcage::rdp_event event);
};

class join_callback
{
public:
        void operator() (bool result);
};


uint16_t port = 10000;
libcage::cage *cage1, *cage2;
int desc_sender;
event *ev;
uint16_t n = 0;


// callback for timer
void
sender(int fd, short ev, void *arg)
{
        cage2->rdp_send(desc_sender, &n, sizeof(n));
        n++;

        // reschedule
        timeval tval;

        tval.tv_sec  = 1;
        tval.tv_usec = 0;

        evtimer_add(::ev, &tval);
}

void
join_callback::operator() (bool result)
{
        if (! result) {
                std::cout << "join: failed" << std::endl;
                cage2->join("localhost", port, *this);
                return;
        }

        std::cout << "join: successed" << std::endl;


        client_callback client_func(*cage2);
        libcage::id_ptr id(new libcage::uint160_t);
        uint8_t addr[CAGE_ID_LEN];

        cage1->get_id(addr);
        id->from_binary(addr, CAGE_ID_LEN);

        cage2->rdp_connect(0, id, 100, client_func);
}

void
server_callback::operator() (int desc, libcage::rdp_addr addr,
                             libcage::rdp_event event)
{
        switch (event) {
        case libcage::ACCEPTED:
                std::cout << "accepted" << std::endl;
                break;
        case libcage::READY2READ:
        {
                uint16_t num;
                int      len;

                for (;;) {
                        len = sizeof(num);
                        m_cage.rdp_receive(desc, &num, &len);
                        
                        if (len <= 0)
                                break;

                        std::cout << "receive: num = " << num
                                  << std::endl;
                }

                break;
        }
        case libcage::BROKEN:
                std::cout << "broken pipe" << std::endl;
                cage1->rdp_close(desc);
                break;
        case libcage::RESET:
                std::cout << "reset by peer" << std::endl;
                cage1->rdp_close(desc);
                break;
        case libcage::FAILED:
                std::cout << "failed to connect" << std::endl;
                cage1->rdp_close(desc);
                break;
        default:
                ;
        }
}

void
client_callback::operator() (int desc, libcage::rdp_addr addr,
                             libcage::rdp_event event)
{
        switch (event) {
        case libcage::CONNECTED:
        {
                std::cout << "connected" << std::endl;

                desc_sender = desc;

                // start timer
                timeval tval;

                ev = new ::event;

                tval.tv_sec  = 0;
                tval.tv_usec = 500 * 1000;

                evtimer_set(ev, sender, NULL);
                evtimer_add(ev, &tval);
                break;
        }
        case libcage::BROKEN:
                std::cout << "broken pipe" << std::endl;
                cage2->rdp_close(desc);
                break;
        case libcage::RESET:
                std::cout << "reset by peer" << std::endl;
                cage2->rdp_close(desc);
                break;
        case libcage::FAILED:
                std::cout << "failed to connect" << std::endl;
                cage2->rdp_close(desc);
                break;
        case libcage::REFUSED:
                std::cout << "connection refused" << std::endl;
                cage2->rdp_close(desc);
                break;
        default:
                ;
        }
}

int
main(int argc, char *argv)
{
#ifdef WIN32
        // initialize winsock
        WSADATA wsaData;
        WSAStartup(MAKEWORD(2,0), &wsaData);
#endif // WIN32

        // initialize libevent
        event_init();

        cage1 = new libcage::cage;
        cage2 = new libcage::cage;

        server_callback server_func(*cage1);


        if (! cage1->open(PF_INET, port)) {
                std::cerr << "cannot open port: Port = "
                          << port
                          << std::endl;
                return -1;
        }
        cage1->set_global();
        cage1->rdp_listen(100, server_func);


        join_callback func;

        if (! cage2->open(PF_INET, port + 1)) {
                std::cerr << "cannot open port: Port = "
                          << port + 1
                          << std::endl;
                return -1;
        }
        cage2->set_global();
        cage2->join("localhost", port, func);


        // handle event loop
        event_dispatch();

        return 0;
}
