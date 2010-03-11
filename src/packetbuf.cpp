#include "packetbuf.hpp"

#include <string.h>

namespace libcage {
        packetbuf::packetbuf() : m_len(0)
        {
                m_begin = &m_buf[128];
        }

        packetbuf::~packetbuf()
        {

        }

        bool
        packetbuf::append(const void *buf, int len)
        {
                if (m_begin + m_len + len > &m_buf[sizeof(m_buf)]) {
                        return false;
                } else {
                        memcpy(&m_begin[m_len], buf, len);
                        m_len += len;
                        return true;
                }
        }

        bool
        packetbuf::prepend(const void *buf, int len)
        {
                if (m_begin - len < m_buf) {
                        return false;
                } else {
                        memcpy(m_begin - len, buf, len);
                        m_len   += len;
                        m_begin -= len;
                        return true;
                }
        }
}
