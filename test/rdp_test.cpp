#include <iostream>

#include <event.h>
#include <libcage/rdp.hpp>


int desc1, desc2;


class event_callback {
public:
        class sender : public libcage::timer::callback {
        public:
                libcage::rdp   *p_rdp;
                uint16_t num;
                int      desc;

                virtual void operator() () {
                        std::cout << "send: num = " << num << std::endl;
                        if (p_rdp->send(desc, &num, sizeof(num)) < 0) {
                                std::cout << "failed to send " << num
                                          << std::endl;
                                return;
                        }
                        num++;

                        if (num > 10) {
                                std::cout << "close connection" << std::endl;
                                p_rdp->close(desc);
                                return;
                        }

                        timeval tval;

                        tval.tv_sec  = 0;
                        tval.tv_usec = 500 * 1000;

                        get_timer()->set_timer(this, &tval);
                }
        };

        libcage::id_ptr id;
        libcage::rdp    *p_rdp;
        libcage::timer  *p_timer;
        sender           sender;

        void operator() (int desc, libcage::rdp_addr addr,
                         libcage::rdp_event event)
        {
                switch (event) {
                case libcage::ACCEPTED:
                        std::cout << "accepted: src = "
                                  << addr.sport
                                  << ", dest = "
                                  << addr.dport << std::endl;
                        break;
                case libcage::CONNECTED:
                        std::cout << "conneced: src = "
                                  << addr.sport
                                  << ", dest = "
                                  << addr.dport << std::endl;

                        sender.p_rdp = p_rdp;
                        sender.desc  = desc;
                        sender.num   = 0;


                        timeval tval;

                        tval.tv_sec  = 0;
                        tval.tv_usec = 500 * 1000;

                        p_timer->set_timer(&sender, &tval);
                        break;
                case libcage::READY2READ:
                        uint16_t num;
                        int      len;

                        for (;;) {
                                len = sizeof(num);
                                p_rdp->receive(desc, &num, &len);

                                if (len <= 0)
                                        break;

                                std::cout << "receive: num = " << num
                                          << std::endl;
                        }

                        break;
                case libcage::RESET:
                        std::cout << "reset by peer" << std::endl;
                        p_rdp->close(desc);
                        break;
                case libcage::FAILED:
                        std::cout << "failed to connect" << std::endl;
                        p_rdp->close(desc);
                        break;
                default:
                        ;
                }
        }
};

class output_callback {
public:
        libcage::id_ptr  id;
        libcage::rdp    *p_rdp;

        void operator() (libcage::id_ptr id_dst, libcage::packetbuf_ptr pbuf)
        {
                if ((rand() % 5) == 0) {
                       std::cout << "dropped!" << std::endl;
                       return;
                }

                int n = 0;
                do {
                        libcage::packetbuf_ptr pbuf2;

                        pbuf2 = libcage::packetbuf::construct();

                        pbuf2->append(pbuf->get_len());

                        memcpy(pbuf2->get_data(), pbuf->get_data(),
                               pbuf->get_len());

                        p_rdp->input_dgram(id, pbuf2);

                        if (n > 0) {
                                std::cout << "duplicated!: n = " << n
                                          << std::endl;
                        }

                        n++;
                } while (rand() % 3 == 0);
        }
};

int
main(int argc, char *argv[])
{
        srand(time(NULL));
        event_init();

        libcage::timer my_timer;
        libcage::rdp   my_rdp(my_timer);
        libcage::id_ptr id(new libcage::uint160_t);
        event_callback  ev_func;
        output_callback out_func;

        *id = 1;

        ev_func.id      = id;
        ev_func.p_rdp   = &my_rdp;
        ev_func.p_timer = &my_timer;
        out_func.id     = id;
        out_func.p_rdp  = &my_rdp;

        my_rdp.set_callback_rdp_event(ev_func);
        my_rdp.set_callback_dgram_out(out_func);

        desc1 = my_rdp.listen(100);
        if (desc1 < 0) {
                std::cerr << "failed to listen" << std::endl;
                return -1;
        }

        desc2 = my_rdp.connect(101, id, 100);

        event_dispatch();

        return 0;
}
