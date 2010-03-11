#ifndef PACKETBUF_HPP
#define PACKETBUF_HPP

#include "common.hpp"

#include <stdint.h>

#include <boost/pool/object_pool.hpp>

namespace libcage {
        class packetbuf {
        public:
                packetbuf();
                virtual ~packetbuf();

                void*           append(int len);
                void*           prepend(int len);
                void*           get_data();
                int             get_len();

                static packetbuf*       construct();
                static void             destroy(packetbuf *p);

        private:
                uint8_t         m_buf[1024 * 2];
                uint8_t        *m_begin;
                int             m_len;

                static boost::object_pool<packetbuf>    pbuf;
        };
}


#endif // PACKETBUF_HPP
