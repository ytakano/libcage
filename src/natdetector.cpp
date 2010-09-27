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

#include "natdetector.hpp"
#include "cagetypes.hpp"
#include "proxy.hpp"

namespace libcage {
        const time_t    natdetector::echo_timeout   = 3;
        const time_t    natdetector::timer_interval = 30;

        extern void no_action(std::vector<cageaddr> &nodes);

        void
        natdetector::timer_echo_wait1::operator() ()
        {
                m_nat->m_state = undefined;

                // delete this
                // do not reference menber variables after this code
                m_nat->m_timers.erase(m_nonce);
        }

        void
        natdetector::timer_echo_wait2::operator() ()
        {
                m_nat->m_state = nat;
                m_nat->m_reply.erase(m_nonce);

                // delete this
                // do not reference menber variables after this code
                m_nat->m_timers.erase(m_nonce);
        }

        void
        natdetector::udp_receiver::operator() (udphandler &udp,
                                               packetbuf_ptr pbuf,
                                               sockaddr *from, int fromlen,
                                               bool is_timeout)
        {
                if (is_timeout) {
                        m_nat->m_state = nat;
                        m_udp.close();

                        // delete this
                        // do not reference menber variables after this code
                        m_nat->m_udps.erase(m_nonce);
                } else {
                        if (pbuf->get_len() ==
                            (int)sizeof(msg_nat_echo_redirect_reply)) {
                                msg_hdr *hdr = (msg_hdr*)pbuf->get_data();

                                if (hdr->type ==
                                    type_nat_echo_redirect_reply) {
#ifdef DEBUG_NAT
                                        printf("recv nat_echo_redirect_reply\n");
#endif // DEBUG_NAT
                                        m_nat->recv_echo_redirect_reply(pbuf->get_data());
                                }
                        } else {
                                m_nat->m_state = undefined;
                        }

                        m_udp.close();

                        // delete this
                        // do not reference menber variables after this code
                        m_nat->m_udps.erase(m_nonce);
                }
        }

        natdetector::natdetector(rand_uint &rnd, udphandler &udp, timer &t,
                                 const uint160_t &id, dht &d, peers &p,
                                 proxy &pr) :
                m_rnd(rnd),
                m_state(undefined),
                m_timer(t),
                m_udp(udp),
                m_id(id),
                m_dht(d),
                m_peers(p),
                m_proxy(pr),
                m_global_port(0),
                m_timer_nat(*this)
        {

        }

        natdetector::~natdetector()
        {
        
        }

        void
        natdetector::detect_nat(sockaddr *saddr, int slen)
        {
                if (m_state != undefined) {
                        return;
                }

                msg_nat_echo echo;

                get_echo(echo);

                // send
                m_udp.sendto(&echo, sizeof(echo), saddr, slen);
        }

        void
        natdetector::detect_nat(std::string host, int port)
        {
                if (m_state != undefined) {
                        return;
                }

                msg_nat_echo echo;

                get_echo(echo);

                // send
                m_udp.sendto(&echo, sizeof(echo), host, port);
        }

        void
        natdetector::get_echo(msg_nat_echo &echo)
        {
                memset(&echo, 0, sizeof(echo));

                echo.hdr.magic = htons(MAGIC_NUMBER);
                echo.hdr.ver   = CAGE_VERSION;
                echo.hdr.type  = type_nat_echo;
                echo.hdr.len   = htons(sizeof(echo));

                m_id.to_binary(echo.hdr.src, sizeof(echo.hdr.src));

                uint32_t nonce;
                for (;;) {
                        nonce = m_rnd();
                        if (m_timers.find(nonce) == m_timers.end())
                                break;
                }

                echo.nonce = htonl(nonce);


                // set timer
                timeval tval;

                tval.tv_sec  = echo_timeout;
                tval.tv_usec = 0;

                callback_ptr func(new timer_echo_wait1);
                timer_echo_wait1 *p = (timer_echo_wait1*)func.get();

                p->m_nonce = nonce;
                p->m_nat   = this;

                m_timer.set_timer(func.get(), &tval);

                m_timers[nonce] = func;
                m_state = echo_wait1;
        }

        void
        natdetector::recv_echo(void *msg, sockaddr *from, int fromlen)
        {
                msg_nat_echo *echo = (msg_nat_echo*)msg;
                msg_nat_echo_reply reply;

                memset(&reply, 0, sizeof(reply));

                reply.hdr.magic = htons(MAGIC_NUMBER);
                reply.hdr.ver   = CAGE_VERSION;
                reply.hdr.type  = type_nat_echo_reply;
                reply.hdr.len   = htons(sizeof(reply));

                m_id.to_binary(reply.hdr.src, sizeof(reply.hdr.src));
                memcpy(reply.hdr.dst, echo->hdr.src, sizeof(reply.hdr.dst));

                reply.nonce  = echo->nonce;

                if (from->sa_family == PF_INET) {
                        sockaddr_in *saddr = (sockaddr_in*)from;

                        reply.domain = htons(domain_inet);
                        reply.port   = saddr->sin_port;
                        memcpy(reply.addr, &saddr->sin_addr,
                               sizeof(saddr->sin_addr));
                } else if (from->sa_family == PF_INET6) {
                        sockaddr_in6 *saddr = (sockaddr_in6*)from;

                        reply.domain = htons(domain_inet6);
                        reply.port   = saddr->sin6_port;
                        memcpy(reply.addr, &saddr->sin6_addr,
                               sizeof(saddr->sin6_addr));
                }

                m_udp.sendto(&reply, sizeof(reply), from, fromlen);
        }

        void
        natdetector::recv_echo_reply_wait1(void *msg, sockaddr *from,
                                           int fromlen)
        {
                msg_nat_echo_reply *reply = (msg_nat_echo_reply*)msg;
                int                 nonce1 = ntohl(reply->nonce);

                if (m_timers.find(nonce1) == m_timers.end())
                        return;

                m_timer.unset_timer(m_timers[nonce1].get());
                m_timers.erase(nonce1);

                // open another UDP socket
                timeval       t;
                udp_ptr       udp(new udp_receiver);
                udp_receiver *p_udp = (udp_receiver*)udp.get();
                uint32_t      nonce2;

                for (;;) {
                        nonce2 = m_rnd();
                        if (m_udps.find(nonce2) == m_udps.end())
                                break;
                }
                
                t.tv_sec  = echo_timeout;
                t.tv_usec = 0;

                p_udp->m_udp.open(from->sa_family, 0);
                p_udp->m_udp.set_callback(udp.get(), &t);

                p_udp->m_nonce = nonce2;
                p_udp->m_nat   = this;

                m_udps[nonce2] = udp;


                // send echo_redirect
                msg_nat_echo_redirect redirect;

                memset(&redirect, 0, sizeof(redirect));
                
                redirect.hdr.magic = htons(MAGIC_NUMBER);
                redirect.hdr.ver   = CAGE_VERSION;
                redirect.hdr.type  = type_nat_echo_redirect;
                redirect.hdr.len   = htons(sizeof(redirect));

                m_id.to_binary(redirect.hdr.src, sizeof(redirect.hdr.src));
                memcpy(redirect.hdr.dst, reply->hdr.src,
                       sizeof(redirect.hdr.dst));

                redirect.nonce = htonl(nonce2);
                redirect.port  = p_udp->m_udp.get_port();

                m_udp.sendto(&redirect, sizeof(redirect), from, fromlen);


                m_state = echo_redirect_wait;
        }

        void
        natdetector::recv_echo_reply_wait2(void *msg)
        {
                msg_nat_echo_reply *reply = (msg_nat_echo_reply*)msg;

                uint32_t nonce = ntohl(reply->nonce);
                if (m_timers.find(nonce) == m_timers.end())
                        return;

                if (m_reply[nonce] == 0) {
                        m_reply[nonce] = reply->port;
                } else if (m_reply[nonce] == reply->port) {
                        m_state = cone_nat;

                        m_timer.unset_timer(m_timers[nonce].get());

                        m_reply.erase(nonce);
                        m_timers.erase(nonce);

                        join_dht();

#ifdef DEBUG_NAT
                        printf("cone nat\n");
#endif // DEBUG_NAT
                } else {
                        m_state = symmetric_nat;

                        m_timer.unset_timer(m_timers[nonce].get());

                        m_reply.erase(nonce);
                        m_timers.erase(nonce);

                        m_proxy.register_node();
                }
        }

        void
        natdetector::recv_echo_reply(void *msg, sockaddr *from, int fromlen)
        {
                if (m_state == echo_wait1)
                        recv_echo_reply_wait1(msg, from, fromlen);

                if (m_state == echo_wait2)
                        recv_echo_reply_wait2(msg);
        }

        void
        natdetector::recv_echo_redirect(void *msg, sockaddr *from, int fromlen)
        {
                msg_nat_echo_redirect      *redirect;
                msg_nat_echo_redirect_reply reply;

                memset(&reply, 0, sizeof(reply));

                redirect = (msg_nat_echo_redirect*)msg;

                reply.hdr.magic = htons(MAGIC_NUMBER);
                reply.hdr.ver   = CAGE_VERSION;
                reply.hdr.type  = type_nat_echo_redirect_reply;
                reply.hdr.len   = htons(sizeof(reply));

                m_id.to_binary(reply.hdr.src, sizeof(reply.hdr.src));
                memcpy(reply.hdr.dst, redirect->hdr.src, sizeof(reply.hdr.dst));

                reply.nonce = redirect->nonce;

                if (from->sa_family == PF_INET) {
                        sockaddr_in *in = (sockaddr_in*)from;

                        reply.port   = in->sin_port;
                        reply.domain = domain_inet;
                        memcpy(reply.addr, &in->sin_addr, sizeof(in->sin_addr));
                        
                        in->sin_port = redirect->port;
                } else if (from->sa_family == PF_INET6) {
                        sockaddr_in6 *in6 = (sockaddr_in6*)from;

                        reply.port     = in6->sin6_port;
                        reply.domain = domain_inet6;
                        memcpy(reply.addr, &in6->sin6_addr,
                               sizeof(in6->sin6_addr));

                        in6->sin6_port = redirect->port;
                }

                m_udp.sendto(&reply, sizeof(reply), from, fromlen);
        }

        void
        natdetector::recv_echo_redirect_reply(void *msg)
        {
                if (m_state != echo_redirect_wait) {
                        m_state = undefined;
                        return;
                }

                msg_nat_echo_redirect_reply *reply;
                uint32_t                     nonce;

                reply = (msg_nat_echo_redirect_reply*)msg;
                nonce = ntohl(reply->nonce);

                if (m_udps.find(nonce) == m_udps.end()) {
                        m_state = undefined;
                        return;
                }

                m_global_port = reply->port;
                memcpy(m_global_addr, reply->addr, sizeof(m_global_addr));

                m_state = global;

                join_dht();
        }

        void
        natdetector::detect_nat_type(sockaddr *saddr1, sockaddr *saddr2,
                                     int slen)
        {
                if (m_state != nat)
                        return;

                // send echo
                msg_nat_echo echo;

                memset(&echo, 0, sizeof(echo));

                echo.hdr.magic = htons(MAGIC_NUMBER);
                echo.hdr.ver   = CAGE_VERSION;
                echo.hdr.type  = type_nat_echo;
                echo.hdr.len   = htons(sizeof(echo));

                m_id.to_binary(echo.hdr.src, sizeof(echo.hdr.src));

                uint32_t nonce;
                for (;;) {
                        nonce = m_rnd();
                        if (m_timers.find(nonce) == m_timers.end())
                                break;
                }

                echo.nonce = htonl(nonce);


                // set timer
                timeval tval;

                tval.tv_sec  = echo_timeout;
                tval.tv_usec = 0;

                callback_ptr func(new timer_echo_wait2);
                timer_echo_wait2 *p = (timer_echo_wait2*)func.get();

                p->m_nonce = nonce;
                p->m_nat   = this;

                m_timer.set_timer(func.get(), &tval);


                // send
                m_udp.sendto(&echo, sizeof(echo), saddr1, slen);
                m_udp.sendto(&echo, sizeof(echo), saddr2, slen);


                m_timers[nonce] = func;
                m_reply[nonce]  = 0;
                m_state         = echo_wait2;
        }

        node_state
        natdetector::get_state() const
        {
                switch (m_state) {
                case undefined:
                        return node_undefined;
                case echo_wait1:
                        return node_undefined;
                case echo_redirect_wait:
                        return node_undefined;
                case global:
                        return node_global;
                case nat:
                        return node_nat;
                case echo_wait2:
                        return node_nat;
                case symmetric_nat:
                        return node_symmetric;
                case cone_nat:
                        return node_cone;
                }

                return node_state(undefined);
        }

        void
        natdetector::join_dht()
        {
                try {
                        cageaddr addr;
                        in_ptr   in;

                        addr = m_peers.get_first();
                        in   = boost::get<in_ptr>(addr.saddr);

                        m_dht.find_node((sockaddr*)in.get(), &no_action);
                } catch (std::out_of_range) {
                        return;
                }
        }

        void
        natdetector::timer_nat::operator() ()
        {
                if (m_nat.m_state == global ||
                    m_nat.m_state == cone_nat ||
                    m_nat.m_state == symmetric_nat)
                        return;

                // reschedule
                timeval tval;

                tval.tv_sec  = natdetector::timer_interval;
                tval.tv_usec = 0;

                m_nat.m_timer.set_timer(this, &tval);

                if (m_nat.m_state == echo_wait1 ||
                    m_nat.m_state == echo_wait2 ||
                    m_nat.m_state == echo_redirect_wait)
                        return;


                try {
                        cageaddr addr1, addr2;
                        in_ptr   in1, in2;

                        addr1 = m_nat.m_peers.get_first();
                        in1 = boost::get<in_ptr>(addr1.saddr);

                        if (m_nat.m_state == undefined) {
                                m_nat.detect_nat((sockaddr*)in1.get(),
                                                 sizeof(sockaddr_in));
                                return;
                        }

                        addr2 = addr1;
                        for (;;) {
                                addr2 = m_nat.m_peers.get_next(addr2.id);
                                in2 = boost::get<in_ptr>(addr2.saddr);

                                if (in1->sin_addr.s_addr !=
                                    in2->sin_addr.s_addr)
                                        break;
                        }

                        m_nat.detect_nat_type((sockaddr*)in1.get(),
                                              (sockaddr*)in2.get(),
                                              sizeof(sockaddr_in));
                } catch (std::out_of_range) {
                        return;
                }
        }

        natdetector::timer_nat::~timer_nat()
        {
                m_nat.m_timer.unset_timer(this);
        }

        void
        natdetector::detect(std::string host, int port)
        {
                if (m_udp.get_domain() == domain_inet6) {
                        m_state = global;
                        return;
                }

                if (m_state == global || m_state == cone_nat ||
                    m_state == symmetric_nat)
                        return;

                m_timer.unset_timer(&m_timer_nat);


                timeval tval;

                detect_nat(host, port);

                tval.tv_sec  = timer_interval;
                tval.tv_usec = 0;

                m_timer.set_timer(&m_timer_nat, &tval);
        }
}
