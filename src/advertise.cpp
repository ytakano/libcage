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

#include "advertise.hpp"

namespace libcage {
        const time_t    advertise::advertise_ttl     = 300;
        const time_t    advertise::advertise_timeout = 2;
        const time_t    advertise::refresh_interval  = 100;


        advertise::advertise(rand_uint &rnd, rand_real &drnd,
                             const uint160_t &id, timer &tm, udphandler &udp,
                             peers &p, dtun &d) :
                m_rnd(rnd),
                m_drnd(drnd),
                m_id(id),
                m_timer(tm),
                m_udp(udp),
                m_peers(p),
                m_dtun(d),
                m_timer_refresh(*this)
        {

        }

        advertise::~advertise()
        {
                boost::unordered_map<uint32_t, timer_ptr>::iterator it;

                for (it = m_advertising.begin(); it != m_advertising.end();
                     ++it) {
                        m_timer.unset_timer(it->second.get());
                }
        }

        void
        advertise::timer_advertise::operator() ()
        {
                p_advertise->m_advertising.erase(nonce);
        }

        void
        advertise::advertise_to(uint160_t &id, uint16_t domain,
                                uint16_t port, void *addr)
        {
                boost::unordered_map<uint160_t, time_t>::iterator it;

                it = m_advertised.find(id);
                if (it != m_advertised.end() && it->second < advertise_ttl / 2)
                        return;

                m_advertised[id] = time(NULL);


                timer_ptr tm(new timer_advertise);
                uint32_t  nonce;

                for (;;) {
                        nonce = m_rnd();
                        if (m_advertising.find(nonce) == m_advertising.end())
                                break;
                }

                tm->p_advertise = this;
                tm->nonce       = nonce;

                m_advertising[nonce] = tm;


                // start timer
                timeval tval;

                tval.tv_sec  = advertise_timeout;
                tval.tv_usec = 0;

                m_timer.set_timer(tm.get(), &tval);


                msg_advertise adv;

                memset(&adv, 0, sizeof(adv));

                adv.hdr.magic = htons(MAGIC_NUMBER);
                adv.hdr.ver   = CAGE_VERSION;
                adv.hdr.type  = type_advertise;
                adv.hdr.len   = htons(sizeof(adv));

                id.to_binary(adv.hdr.dst, sizeof(adv.hdr.dst));
                m_id.to_binary(adv.hdr.src, sizeof(adv.hdr.src));

                adv.nonce   = htonl(nonce);
                adv.session = htonl(m_dtun.get_session());

                if (domain == domain_inet) {
                        sockaddr_in in;

                        memset(&in, 0, sizeof(in));

                        in.sin_family = PF_INET;
                        in.sin_port   = port;
                        in.sin_addr.s_addr   = ((uint32_t*)addr)[0];

                        m_udp.sendto(&adv, sizeof(adv), (sockaddr*)&in,
                                     sizeof(in));
                } else if (domain == domain_inet6) {
                        sockaddr_in6 in6;

                        memset(&in6, 0, sizeof(in6));

                        in6.sin6_family = PF_INET6;
                        in6.sin6_port   = port;

                        memcpy(in6.sin6_addr.s6_addr, addr, sizeof(in6_addr));

                        m_udp.sendto(&adv, sizeof(adv), (sockaddr*)&in6,
                                     sizeof(in6));
                }
        }

        void
        advertise::recv_advertise(void *msg, sockaddr *from)
        {
                msg_advertise *adv;
                cageaddr  addr;
                uint160_t dst;
                uint32_t  session;

                adv = (msg_advertise*)msg;

                dst.from_binary(adv->hdr.dst, sizeof(adv->hdr.dst));
                if (dst != m_id)
                        return;

                session = ntohl(adv->session);

                addr = new_cageaddr(&adv->hdr, from);

                if (! m_peers.add_node(addr, session))
                        return;


                msg_advertise_reply reply;

                memset(&reply, 0, sizeof(reply));

                reply.nonce   = adv->nonce;
                reply.session = htonl(m_dtun.get_session());

                send_msg(m_udp, &reply.hdr, sizeof(reply), type_advertise_reply,
                         addr, m_id);
        }

        void
        advertise::recv_advertise_reply(void *msg, sockaddr *from)
        {
                msg_advertise_reply *reply;
                cageaddr  addr;
                uint160_t dst;
                uint32_t  nonce;
                uint32_t  session;

                reply = (msg_advertise_reply*)msg;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));

                if (dst != m_id)
                        return;

                nonce = ntohl(reply->nonce);

                boost::unordered_map<uint32_t, timer_ptr>::iterator it;

                it = m_advertising.find(nonce);
                if (it == m_advertising.end())
                        return;

                m_timer.unset_timer(it->second.get());
                m_advertising.erase(nonce);

                session = ntohl(reply->session);
                addr = new_cageaddr(&reply->hdr, from);

                m_peers.add_node(addr, session);
        }

        void
        advertise::refresh()
        {
                boost::unordered_map<uint160_t, time_t>::iterator it;
                time_t now;

                now = time(NULL);

                for (it = m_advertised.begin(); it != m_advertised.end();) {
                        time_t diff = now - it->second;
                        if (diff > advertise_ttl) {
                                m_advertised.erase(it++);
                        } else {
                                ++it;
                        }
                }
        }
}
