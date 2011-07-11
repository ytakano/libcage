/*
 * Copyright (c) 2010, Yuuki Takano (ytakanoster@gmail.com).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the writers nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "packetbuf.hpp"

#include <string.h>

namespace libcage {
        boost::object_pool<packetbuf>   packetbuf::pbuf_pool;

        packetbuf::packetbuf() : m_len(0), m_refc(0)
        {
                m_head = &m_buf[PBUF_DEFAULT_OFFSET];
        }

        void*
        packetbuf::append(int32_t len)
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
        packetbuf::prepend(int32_t len)
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

        int32_t
        packetbuf::get_len()
        {
                return m_len;
        }

        void
        packetbuf::set_len(int32_t len)
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
        packetbuf::rm_head(int32_t len)
        {
                if (len > m_len)
                        return;

                m_head += len;
                m_len  -= len;
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
                if (pbuf->m_refc == 0) {
                        packetbuf::pbuf_pool.destroy(pbuf);
                }
        }
}
