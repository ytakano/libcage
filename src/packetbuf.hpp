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

#ifndef PACKETBUF_HPP
#define PACKETBUF_HPP

#include "common.hpp"

#include <stdint.h>

#include <boost/intrusive_ptr.hpp>
#include <boost/pool/object_pool.hpp>

#define PBUF_SIZE           1024
#define PBUF_DEFAULT_OFFSET 128

namespace libcage {
        class packetbuf;

        void    intrusive_ptr_add_ref(packetbuf *pbuf);
        void    intrusive_ptr_release(packetbuf *pbuf);

        typedef boost::intrusive_ptr<packetbuf> packetbuf_ptr;

        class packetbuf {
        public:
                packetbuf();

                void*           append(int32_t len);
                void*           prepend(int32_t len);
                void*           get_data();
                int32_t         get_len();
                void            set_len(int32_t len);
                void            use_whole();
                void            rm_head(int32_t len);

                static packetbuf_ptr    construct();

                friend void     intrusive_ptr_add_ref(packetbuf *pbuf);
                friend void     intrusive_ptr_release(packetbuf *pbuf);

        private:
                static const int buf_max;

                uint8_t         m_buf[PBUF_SIZE];
                uint8_t        *m_head;
                int32_t         m_len;
                int32_t         m_refc;

                static boost::object_pool<packetbuf>    pbuf_pool;
        };
}

#endif // PACKETBUF_HPP
