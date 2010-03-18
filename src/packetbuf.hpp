#ifndef PACKETBUF_HPP
#define PACKETBUF_HPP

#include "common.hpp"

#include <stdint.h>

#include <boost/intrusive_ptr.hpp>
#include <boost/pool/object_pool.hpp>

namespace libcage {
        class packetbuf;

        void    intrusive_ptr_add_ref(packetbuf *pbuf);
        void    intrusive_ptr_release(packetbuf *pbuf);

        typedef boost::intrusive_ptr<packetbuf> packetbuf_ptr;

        class packetbuf {
        public:
                packetbuf();
                virtual ~packetbuf();

                void*           append(int len);
                void*           prepend(int len);
                void*           get_data();
                int             get_len();
                void            set_len(int len);
                void            use_whole();

                static packetbuf_ptr    construct();

                friend void     intrusive_ptr_add_ref(packetbuf *pbuf);
                friend void     intrusive_ptr_release(packetbuf *pbuf);

        private:
                uint8_t         m_buf[1024 * 2];
                uint8_t        *m_head;
                int             m_len;
                int             m_refc;

                static boost::object_pool<packetbuf>    pbuf_pool;
        };
}

#endif // PACKETBUF_HPP
