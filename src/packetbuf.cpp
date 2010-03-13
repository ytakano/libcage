#include "packetbuf.hpp"

#include <string.h>

namespace libcage {
        boost::object_pool<packetbuf>   packetbuf::pbuf;

        packetbuf::packetbuf() : m_len(0), m_ref_count(0)
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

        packetbuf*
        packetbuf::construct()
        {
                return pbuf.construct();
        }

        void
        packetbuf::destroy(packetbuf *p)
        {
                pbuf.destroy(p);
        }

        void
        packetbuf::inc_refc()
        {
                m_ref_count++;
        }

        void
        packetbuf::dec_refc()
        {
                m_ref_count++;
        }

        int
        packetbuf::get_refc()
        {
                return m_ref_count;
        }
}
