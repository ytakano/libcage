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

#ifndef CAGE_HPP
#define CAGE_HPP

#include "common.hpp"

#include "bn.hpp"
#include "dtun.hpp"
#include "natdetector.hpp"
#include "peers.hpp"
#include "timer.hpp"
#include "udphandler.hpp"

namespace libcage {
        class cage {
        public:
                cage();

                bool            open(int domain, uint16_t port);

        private:
                class udp_receiver : public udphandler::callback {
                public:
                        virtual void operator() (udphandler &udp, void *buf,
                                                 int len, sockaddr *from,
                                                 int fromlen,
                                                 bool is_timeout);

                        udp_receiver(cage &c) : m_cage(c) {}

                private:
                        cage   &m_cage;
                };

                udphandler      m_udp;
                timer           m_timer;
                uint160_t       m_id;
                natdetector     m_nat;
                udp_receiver    m_receiver;
                peers           m_peers;
                dtun            m_dtun;

#ifdef DEBUG_NAT
        public:
                static void     test_natdetect();
                static void     test_nattypedetect();
#endif // DEBUG_NAT
        };
}

#endif // CAGE_HPP
