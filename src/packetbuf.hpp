#ifndef PACKETBUF_HPP
#define PACKETBUF_HPP

#include "common.hpp"

#include <stdint.h>

namespace libcage {
        class packetbuf {
        public:
                packetbuf();
                virtual ~packetbuf();

                bool append(const void *buf, int len);
                bool prepend(const void *buf, int len);

        private:
                uint8_t         m_buf[1024 * 2];
                uint8_t        *m_begin;
                int             m_len;
        };
}


#endif // PROTOBUF_HPP
