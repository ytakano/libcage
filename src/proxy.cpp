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

#include "proxy.hpp"

#include <openssl/rand.h>

#include <boost/foreach.hpp>

namespace libcage {
        const time_t    proxy::register_timeout = 2;


        size_t
        hash_value(const proxy::_id &i)
        {
                return i.id->hash_value();
        }

        proxy::proxy(const uint160_t &id, udphandler &udp, timer &t,
                     dtun &d, peers &p) :
                m_id(id),
                m_udp(udp),
                m_timer(t),
                m_dtun(d),
                m_peers(p),
                m_is_registering(false),
                m_timer_register(*this)
        {
                RAND_pseudo_bytes((unsigned char*)&m_register_session,
                                  sizeof(m_register_session));
        }

        void
        proxy::timer_register::operator() ()
        {
                m_proxy.m_is_registering  = false;
        }

        void
        proxy::register_func::operator() (std::vector<cageaddr> &nodes)
        {
                BOOST_FOREACH(cageaddr &addr, nodes) {
                        if (p_proxy->m_id == *addr.id)
                                continue;

                        msg_proxy_register reg;

                        memset(&reg, 0, sizeof(reg));

                        p_proxy->m_nonce = mrand48();

                        reg.session = htonl(p_proxy->m_register_session);
                        reg.nonce   = htonl(p_proxy->m_nonce);


                        // start timer
                        timeval tval;

                        tval.tv_sec  = proxy::register_timeout;
                        tval.tv_usec = 0;

                        p_proxy->m_timer.set_timer(&p_proxy->m_timer_register,
                                                   &tval);

                        send_msg(p_proxy->m_udp, &reg.hdr, sizeof(reg),
                                 type_proxy_register, addr, p_proxy->m_id);

                        break;
                }
        }

        void
        proxy::recv_register_reply(void *msg, sockaddr *from)
        {
                msg_proxy_register_reply *reply;
                cageaddr  addr;
                uint160_t dst;
                uint32_t  nonce;

                if (! m_is_registering)
                        return;

                reply = (msg_proxy_register_reply*)msg;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));

                if (dst != m_id)
                        return;

                nonce = ntohl(reply->nonce);

                if (m_nonce != nonce)
                        return;

                addr = new_cageaddr(&reply->hdr, from);
                m_server = addr;

                m_is_registering = false;
                m_timer.unset_timer(&m_timer_register);
        }

        void
        proxy::recv_register(void *msg, sockaddr *from)
        {
                msg_proxy_register *reg;
                cageaddr  addr;
                uint160_t dst;

                reg = (msg_proxy_register*)msg;

                dst.from_binary(reg->hdr.dst, sizeof(reg->hdr.dst));

                if (dst != m_id)
                        return;

                addr = new_cageaddr(&reg->hdr, from);


                boost::unordered_map<_id, _addr>::iterator it;
                _id i;

                i.id = addr.id;
                it = m_registered.find(i);

                if (it == m_registered.end()) {
                        _addr a;

                        a.session   = ntohl(reg->session);
                        a.addr      = addr;
                        a.recv_time = time(NULL);
                        a.last_registered = time(NULL);

                        m_dtun.register_node(addr.id, a.session);
                } else {
                        uint32_t session = ntohl(reg->session);

                        if (it->second.session == session) {
                                it->second.addr      = addr;
                                it->second.recv_time = time(NULL);
                        } else {
                                return;
                        }
                }

                msg_proxy_register_reply  reply;

                reply.nonce = reg->nonce;

                send_msg(m_udp, &reply.hdr, sizeof(reply),
                         type_proxy_register_reply, addr, m_id);
        }

        void
        proxy::register_node()
        {
                if (m_is_registering)
                        return;

                m_is_registering = true;

                register_func func;

                func.p_proxy = this;

                m_dtun.find_node(m_id, func);
        }
}
