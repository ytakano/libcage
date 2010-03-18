#include "packetbuf.hpp"

#include <string.h>

namespace libcage {
        boost::object_pool<packetbuf>   packetbuf::pbuf_pool;

        packetbuf::packetbuf() : m_len(0), m_refc(0)
        {
                m_head = &m_buf[128];
        }

        packetbuf::~packetbuf()
        {

        }

        void*
        packetbuf::append(int len)
        {
                if (m_head + m_len + len > &m_buf[sizeof(m_buf)]) {
                        return NULL;
                } else {
                        void *p = &m_head[m_len];
                        m_len += len;
                        return p;
                }
        }

        void*
        packetbuf::prepend(int len)
        {
                if (m_head - len < m_buf) {
                        return NULL;
                } else {
                        m_len  += len;
                        m_head -= len;
                        return m_head;
                }
        }

        void*
        packetbuf::get_data()
        {
                return m_head;
        }

        int
        packetbuf::get_len()
        {
                return m_len;
        }

        void
        packetbuf::set_len(int len)
        {
                m_len = len;
        }

        void
        packetbuf::use_whole()
        {
                m_head = m_buf;
                m_len = sizeof(m_buf);
        }

        packetbuf_ptr
        packetbuf::construct()
        {
                packetbuf_ptr p(pbuf_pool.construct());
                return p;
        }

        void
        intrusive_ptr_add_ref(packetbuf *pbuf)
        {
                pbuf->m_refc++;
        }

        void
        intrusive_ptr_release(packetbuf *pbuf)
        {
                pbuf->m_refc--;
                if (pbuf->m_refc == 0)
                        packetbuf::pbuf_pool.destroy(pbuf);
        }
}
