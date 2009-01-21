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

#include "dht.hpp"

#include "ping.hpp"

namespace libcage {
        dht::dht(const uint160_t &id, timer &t, peers &p, udphandler &udp,
                 dtun &dt) :
                rttable(id, t, p),
                m_id(id),
                m_timer(t),
                m_peers(p),
                m_udp(udp),
                m_dtun(dt)
        {

        }

        dht::~dht()
        {

        }

        void
        dht::ping_func::operator() (bool result, cageaddr &addr)
        {
                if (result) {
                        send_ping_tmpl<msg_dht_ping>(addr, nonce, type_dht_ping,
                                                     p_dht->m_id,
                                                     p_dht->m_udp);
                }
        }

        void
        dht::send_ping(cageaddr &dst, uint32_t nonce)
        {
                try {
                        m_peers.get_addr(dst.id);
                        send_ping_tmpl<msg_dht_ping>(dst, nonce, type_dht_ping,
                                                     m_id, m_udp);
                } catch (std::out_of_range) {
                        ping_func func;

                        func.dst   = dst;
                        func.nonce = nonce;
                        func.p_dht = this;

                        m_dtun.request(*dst.id, func);
                }
        }

        void
        dht::recv_ping(void *msg, sockaddr *from, int fromlen)
        {
                recv_ping_tmpl<msg_dht_ping,
                        msg_dht_ping_reply>(msg, from, fromlen,
                                            type_dht_ping_reply,
                                            m_id, m_udp, m_peers);
        }

        void
        dht::recv_ping_reply(void *msg, sockaddr *from, int fromlen)
        {
                recv_ping_reply_tmpl<msg_dht_ping_reply>(msg, from, fromlen,
                                                         m_id, m_peers, this);
        }
}
