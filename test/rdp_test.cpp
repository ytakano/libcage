#include <iostream>

#include <event.h>

#include <libcage/cagetypes.hpp>
#include <libcage/rdp.hpp>


int desc1, desc2;

class sender;

class server_callback {
public:
        libcage::rdp   &m_rdp;

        server_callback(libcage::rdp &r) : m_rdp(r) { }

        void operator() (int desc, libcage::rdp_addr addr,
                         libcage::rdp_event event);
};

class client_callback {
public:
        libcage::rdp   &m_rdp;
        libcage::timer &m_timer;
        sender         &m_sender;

        client_callback(libcage::rdp &r, libcage::timer &tm, sender &sndr)
                : m_rdp(r), m_timer(tm), m_sender(sndr) { }

        void operator() (int desc, libcage::rdp_addr addr,
                         libcage::rdp_event event);
};

class sender : public libcage::timer::callback {
public:
        libcage::rdp &m_rdp;
        uint16_t      m_num;
        int           m_desc;

        sender(libcage::rdp &r) : m_rdp(r) { m_num = 0; }

        virtual void operator() ();
};

class output_callback {
public:
        libcage::id_ptr  m_id;
        libcage::rdp    &m_rdp;

        output_callback(libcage::id_ptr id, libcage::rdp &r)
                : m_id(id), m_rdp(r) { }

        void operator() (libcage::id_ptr id_dst, libcage::packetbuf_ptr pbuf);
};

void
server_callback::operator() (int desc, libcage::rdp_addr addr,
                             libcage::rdp_event event)
{
        switch (event) {
        case libcage::ACCEPTED:
                std::cout << "accepted: src = "
                          << addr.sport
                          << ", dest = "
                          << addr.dport << std::endl;
                break;
        case libcage::READY2READ:
        {
                uint16_t num;
                int      len;

                for (;;) {
                        len = sizeof(num);
                        m_rdp.receive(desc, &num, &len);
                        
                        if (len <= 0)
                                break;

                        std::cout << "receive: num = " << num
                                  << std::endl;
                }

                break;
        }
        case libcage::BROKEN:
                std::cout << "broken pipe" << std::endl;
                m_rdp.close(desc);
                break;
        case libcage::RESET:
                std::cout << "reset by peer" << std::endl;
                m_rdp.close(desc);
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
                std::cout << "conneced: src = "
                          << addr.sport
                          << ", dest = "
                          << addr.dport << std::endl;

                m_sender.m_desc = desc;

                timeval tval;

                tval.tv_sec  = 0;
                tval.tv_usec = 500 * 1000;

                m_timer.set_timer(&m_sender, &tval);
                break;
        }
        case libcage::BROKEN:
                std::cout << "broken pipe" << std::endl;
                m_rdp.close(desc);
                break;
        case libcage::RESET:
                std::cout << "reset by peer" << std::endl;
                m_rdp.close(desc);
                break;
        case libcage::FAILED:
                std::cout << "failed to connect" << std::endl;
                m_rdp.close(desc);
                break;
        default:
                ;
        }
}

void
sender::operator() ()
{
        std::cout << "send: num = " << m_num << std::endl;
        if (m_rdp.send(m_desc, &m_num, sizeof(m_num)) < 0) {
                std::cout << "failed to send " << m_num
                          << std::endl;
                return;
        }
        m_num++;

        if (m_num > 1000) {
                std::cout << "close connection" << std::endl;
                m_rdp.close(m_desc);
                return;
        }

        timeval tval;

        tval.tv_sec  = 0;
        tval.tv_usec = 500 * 1000;

        get_timer()->set_timer(this, &tval);
}

void
output_callback::operator() (libcage::id_ptr id_dst,
                             libcage::packetbuf_ptr pbuf)
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

                m_rdp.input_dgram(m_id, pbuf2);
                        
                if (n > 0) {
                        std::cout << "duplicated!: n = " << n
                                  << std::endl;
                }

                n++;
        } while (rand() % 10 == 0);
}

int
main(int argc, char *argv[])
{
        event_init();

        boost::mt19937     gen;
        libcage::uint_dist dist(0, ~0);
        libcage::rand_uint rnd(gen, dist);


        libcage::timer  my_timer;
        libcage::rdp    my_rdp(rnd, my_timer);
        libcage::id_ptr my_id(new libcage::uint160_t);
        sender          my_sender(my_rdp);
        output_callback out_func(my_id, my_rdp);
        server_callback server_func(my_rdp);
        client_callback client_func(my_rdp, my_timer, my_sender);

        my_rdp.set_callback_dgram_out(out_func);

        *my_id = 1;

        desc1 = my_rdp.listen(100, server_func);
        if (desc1 < 0) {
                std::cerr << "failed to listen" << std::endl;
                return -1;
        }

        desc2 = my_rdp.connect(101, my_id, 100, client_func);

        event_dispatch();

        return 0;
}
