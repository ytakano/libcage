/*
 * Copyright (c) 2009, Yuuki Takano (ytakanoster@gmail.com).
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

#ifndef UDPHANDLER_HPP
#define UDPHANDLER_HPP

#include "common.hpp"
#include "packetbuf.hpp"

#include <event.h>

#include <set>
#include <string>

#ifndef WIN32
        typedef int SOCKET;
#endif // WIN32

namespace libcage {
        class udphandler {
        public:
                class callback {
                public:
                        virtual void operator() (udphandler &udp,
                                                 packetbuf_ptr pbuf,
                                                 sockaddr *from, int fromlen,
                                                 bool is_timeout) = 0;

                        virtual ~callback() {}
                };

                void            set_callback(callback *func);
                void            set_callback(callback *func, timeval *tout);
                void            unset_callback();

                bool            open(int domain, int port);
                void            close();

                void            sendto(const void *msg, int len,
                                       const sockaddr* to,
                                       int tolen);
                void            sendto(const void *msg, int len,
                                       std::string host, int port);

                bool            get_sockaddr(sockaddr_storage *saddr,
                                             std::string host, int port);

                // network byte order
                uint16_t        get_port();

                uint16_t        get_domain();


                static void     init();
                static void     clean_up();

                friend void     udp_callback(SOCKET fd, short event, void *arg);


                udphandler();
                virtual ~udphandler();

        private:
                callback       *m_callback;
                event           m_event;
                SOCKET          m_socket;
                bool            m_opened;
                int             m_domain;

#ifdef DEBUG
        public:
                static void     test_udp();
#endif // DEBUG
        };
}

#endif // UDPHANDLER_HPP
