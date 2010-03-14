#include "packetbuf.hpp"

#include <string.h>

namespace libcage {
        boost::object_pool<packetbuf>   packetbuf::pbuf_pool;

        packetbuf::packetbuf() : m_len(0), m_refc(0)
        {
                m_begin = &m_buf[128];
        }

        packetbuf::~packetbuf()
        {

        }

        void*
        packetbuf::append(int len)
        {
                if (m_begin + m_len + len > &m_buf[sizeof(m_buf)]) {
                        return NULL;
                } else {
                        void *p = &m_begin[m_len];
                        m_len += len;
                        return p;
                }
        }

        void*
        packetbuf::prepend(int len)
        {
                if (m_begin - len < m_buf) {
                        return NULL;
                } else {
                        m_len   += len;
                        m_begin -= len;
                        return m_begin;
                }
        }

        void*
        packetbuf::get_data()
        {
                return m_begin;
        }

        int
        packetbuf::get_len()
        {
                return m_len;
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
