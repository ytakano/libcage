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

#include "dtun.hpp"

#include <openssl/rand.h>

#include <boost/foreach.hpp>

namespace libcage {
        const int       dtun::num_find_node    = 6;
        const int       dtun::max_query        = 3;
        const int       dtun::query_timeout    = 2;
        const int       dtun::register_timeout = 10;

        void
        dtun::timer_query::operator() ()
        {
                query_ptr q = p_dtun->m_query[nonce];
                timer_ptr t = q->timers[id];
                uint160_t zero;

                q->sent.insert(id);
                q->num_query--;
                q->timers.erase(id);

                zero.fill_zero();

                if (*id.id != zero) {
                        std::vector<cageaddr> tmp;
                        
                        tmp = q->nodes;
                        q->nodes.clear();
                        BOOST_FOREACH(cageaddr &addr, tmp) {
                                if (*addr.id != *id.id)
                                        q->nodes.push_back(addr);
                        }

                        p_dtun->m_peers.add_timeout(id.id);
                        p_dtun->remove(*id.id);
                }

                // send find node
                p_dtun->send_find(q);
        }

        dtun::dtun(const uint160_t &id, timer &t, peers &p,
                   const natdetector &nat, udphandler &udp) :
                rttable(id, t, p),
                m_id(id),
                m_timer(t),
                m_peers(p),
                m_nat(nat),
                m_udp(udp),
                m_timer_register(*this),
                m_registering(false),
                m_last_registerd(0)
        {
                RAND_pseudo_bytes((unsigned char*)&m_register_session,
                                  sizeof(m_register_session));
        }

        dtun::~dtun()
        {

        }

        void
        dtun::send_ping(cageaddr &dst, uint32_t nonce)
        {
                msg_dtun_ping ping;

                memset(&ping, 0, sizeof(ping));

                ping.hdr.magic = htons(MAGIC_NUMBER);
                ping.hdr.ver   = htons(CAGE_VERSION);
                ping.hdr.type  = htons(type_dtun_ping);

                m_id.to_binary(ping.hdr.src, sizeof(ping.hdr.src));
                dst.id->to_binary(ping.hdr.dst, sizeof(ping.hdr.dst));

                ping.nonce = htonl(nonce);

                if (dst.domain == domain_inet) {
                        sockaddr* saddr;
                        saddr = (sockaddr*)boost::get<sockaddr_in*>(dst.saddr);
                        m_udp.sendto(&ping, sizeof(ping), saddr,
                                     sizeof(sockaddr_in));
                } else {
                        sockaddr* saddr;
                        saddr = (sockaddr*)boost::get<sockaddr_in6*>(dst.saddr);
                        m_udp.sendto(&ping, sizeof(ping), saddr,
                                     sizeof(sockaddr_in6));
                }
        }

        void
        dtun::recv_ping(void *msg, sockaddr *from, int fromlen)
        {
                msg_dtun_ping       *ping = (msg_dtun_ping*)msg;
                msg_dtun_ping_reply  reply;
                uint160_t            fromid;

                fromid.from_binary(ping->hdr.dst, sizeof(ping->hdr.dst));
                if (fromid != m_id)
                        return;


                // send ping reply
                memset(&reply, 0, sizeof(reply));

                reply.hdr.magic = htons(MAGIC_NUMBER);
                reply.hdr.ver   = htons(CAGE_VERSION);
                reply.hdr.type  = htons(type_dtun_ping_reply);

                m_id.to_binary(reply.hdr.src, sizeof(reply.hdr.src));
                memcpy(reply.hdr.dst, ping->hdr.src, sizeof(reply.hdr.dst));

                reply.nonce = ping->nonce;

                m_udp.sendto(&reply, sizeof(reply), from, fromlen);


                // add to peers
                cageaddr addr = new_cageaddr(&ping->hdr, from);
                m_peers.add_node(addr);
        }

        void
        dtun::recv_ping_reply(void *msg, sockaddr *from, int fromlen)
        {
                msg_dtun_ping_reply *reply = (msg_dtun_ping_reply*)msg;
                uint160_t            fromid;

                fromid.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (fromid != m_id)
                        return;

                cageaddr addr = new_cageaddr(&reply->hdr, from);

                rttable::recv_ping_reply(addr, ntohl(reply->nonce));


                // add to peers
                m_peers.add_node(addr);
        }

        void
        dtun::find_node(std::string host, int port, callback_find_node func)
        {
                sockaddr_storage saddr;

                if (! m_udp.get_sockaddr(&saddr, host, port))
                        return;

                // initialize query
                query_ptr q(new query);

                q->dst           = m_id;
                q->num_query     = 1;
                q->is_find_value = false;
                q->func          = func;

                // add my id
                _id i;
                i.id = id_ptr(new uint160_t);
                *i.id = m_id;

                q->sent.insert(i);

                uint32_t nonce;
                do {
                        nonce = mrand48();
                } while (m_query.find(nonce) != m_query.end());

                q->nonce = nonce;
                m_query[nonce] = q;


                // start timer
                timeval   tval;
                timer_ptr t(new timer_query);
                id_ptr    zero(new uint160_t);
                _id       zero_id;

                zero->fill_zero();
                zero_id.id = zero;

                t->nonce  = q->nonce;
                t->id     = zero_id;
                t->p_dtun = this;

                tval.tv_sec  = query_timeout;
                tval.tv_usec = 0;

                q->timers[zero_id] = t;

                m_timer.set_timer(t.get(), &tval);


                // send find node
                cageaddr addr;
                addr.id = zero;

                addr.domain = m_udp.get_domain();
                if (addr.domain == domain_inet) {
                        in_ptr in(new sockaddr_in);
                        memcpy(in.get(), &saddr, sizeof(sockaddr_in));
                        addr.saddr = in;
                } else if (addr.domain == domain_inet6) {
                        in6_ptr in6(new sockaddr_in6);
                        memcpy(in6.get(), &saddr, sizeof(sockaddr_in6));
                        addr.saddr = in6;
                }

                send_find_node(addr, q);
        }

        void
        dtun::find_nv(const uint160_t &dst, callback_func func,
                      bool is_find_value)
        {
                query_ptr q(new query);

                lookup(dst, num_find_node, q->nodes);

                if (q->nodes.size()) {
                        if (is_find_value) {
                                callback_find_value f;
                                f = boost::get<callback_find_value>(func);
                                cageaddr addr;
                                f(false, addr);
                        } else {
                                callback_find_node f;
                                f = boost::get<callback_find_node>(func);
                                f(q->nodes);
                        }
                        return;
                }

                q->dst           = dst;
                q->num_query     = 0;
                q->is_find_value = is_find_value;
                q->func          = func;

                // add my id
                _id i;
                i.id = id_ptr(new uint160_t);
                *i.id = m_id;

                q->sent.insert(i);


                uint32_t nonce;
                do {
                        nonce = mrand48();
                } while (m_query.find(nonce) != m_query.end());

                q->nonce = nonce;
                m_query[nonce] = q;


                send_find(q);
        }

        void
        dtun::find_node(const uint160_t &dst, callback_find_node func)
        {
                find_nv(dst, func, false);
        }

        void
        dtun::find_value(const uint160_t &dst, callback_find_value func)
        {
                // TODO: check local cache

                find_nv(dst, func, true);
        }

        void
        dtun::send_find(query_ptr q)
        {
                BOOST_FOREACH(cageaddr &addr, q->nodes) {
                        if (q->num_query >= max_query) {
                                break;
                        }

                        _id i;
                        i.id = addr.id;

                        if (q->sent.find(i) != q->sent.end()) {
                                continue;
                        }

                        // start timer
                        timeval   tval;
                        timer_ptr t(new timer_query);

                        t->nonce  = q->nonce;
                        t->id     = i;
                        t->p_dtun = this;

                        tval.tv_sec  = query_timeout;
                        tval.tv_usec = 0;

                        q->timers[i] = t;
                        q->sent.insert(i);

                        m_timer.set_timer(t.get(), &tval);


                        // send find node
                        if (q->is_find_value) {
                                send_find_value(addr, q);
                        } else {
                                send_find_node(addr, q);
                        }
                }

                if (q->num_query == 0) {
                        // call callback functions
                        if (q->is_find_value) {
                                cageaddr addr;
                                callback_find_value func;
                                func = boost::get<callback_find_value>(q->func);
                                func(false, addr);
                        } else {
                                callback_find_node func;
                                func = boost::get<callback_find_node>(q->func);
                                func(q->nodes);
                        }

                        // stop all timers
                        std::map<_id, timer_ptr>::iterator it;
                        for (it = q->timers.begin(); it != q->timers.end();
                             ++it) {
                                m_timer.unset_timer(it->second.get());
                        }

                        // remove query
                        m_query.erase(q->nonce);
                }
        }

        template<typename MSG>
        void
        dtun::send_find_nv(uint16_t type, cageaddr &dst, query_ptr q)
        {
                MSG msg;

                memset(&msg, 0, sizeof(msg));

                msg.hdr.magic = htons(MAGIC_NUMBER);
                msg.hdr.ver   = htons(CAGE_VERSION);
                msg.hdr.type  = htons(type);

                m_id.to_binary(msg.hdr.src, sizeof(msg.hdr.src));
                dst.id->to_binary(msg.hdr.dst, sizeof(msg.hdr.dst));

                msg.nonce  = htonl(q->nonce);
                msg.domain = htons(m_udp.get_domain());

                if (m_nat.is_global()) {
                        msg.state = htons(state_global);
                } else {
                        msg.state = htons(state_nat);
                }

                q->dst.to_binary(msg.id, sizeof(msg.id));

                if (dst.domain == domain_inet) {
                        in_ptr in = boost::get<in_ptr>(dst.saddr);
                        m_udp.sendto(&msg, sizeof(msg),
                                     (sockaddr*)in.get(),
                                     sizeof(sockaddr_in));
                } else if (dst.domain == domain_inet6) {
                        in6_ptr in6 = boost::get<in6_ptr>(dst.saddr);
                        m_udp.sendto(&msg, sizeof(msg),
                                     (sockaddr*)in6.get(),
                                     sizeof(sockaddr_in6));
                }
        }

        void
        dtun::send_find_node(cageaddr &dst, query_ptr q)
        {
                send_find_nv<msg_dtun_find_node>(type_dtun_find_node, dst, q);
        }

        void
        dtun::send_find_value(cageaddr &dst, query_ptr q)
        {
                send_find_nv<msg_dtun_find_value>(type_dtun_find_value, dst, q);
        }

        void
        dtun::recv_find_node(void *msg, sockaddr *from, int fromlen)
        {
                msg_dtun_find_node       *find_node;
                msg_dtun_find_node_reply *reply;
                std::vector<cageaddr>     nodes;
                uint160_t                 dst, id;
                int                       size;
                uint8_t                  *buf;

                find_node = (msg_dtun_find_node*)msg;

                dst.from_binary(find_node->hdr.dst,
                                sizeof(find_node->hdr.dst));

                if (dst != m_id && ! dst.is_zero()) {
                        return;
                }

                if (ntohs(find_node->domain) != m_udp.get_domain()) {
                        return;
                }

                
                // lookup rttable
                id.from_binary(find_node->id, sizeof(find_node->id));
                lookup(id, num_find_node, nodes);

                uint16_t domain = m_udp.get_domain();
                if (domain == domain_inet) {
                        msg_inet *min;
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                nodes.size() * sizeof(*min);
                        buf = new uint8_t[size];

                        reply = (msg_dtun_find_node_reply*)buf;
                        min   = (msg_inet*)reply->addrs;

                        memset(reply, 0, size);

                        BOOST_FOREACH(cageaddr &addr, nodes) {
                                if (addr.domain == domain_loopback) {
                                        min->port = 0;
                                        min->addr = 0;
                                } else {
                                        in_ptr in;
                                        in = boost::get<in_ptr>(addr.saddr);

                                        min->port = in->sin_port;
                                        min->addr = in->sin_addr.s_addr;
                                }
                                addr.id->to_binary(min->id, sizeof(min->id));

                                min++;
                        }
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                nodes.size() * sizeof(*min6);
                        buf = new uint8_t[size];

                        reply = (msg_dtun_find_node_reply*)buf;
                        min6  = (msg_inet6*)reply->addrs;

                        memset(reply, 0, size);

                        BOOST_FOREACH(cageaddr &addr, nodes) {
                                if (addr.domain == domain_loopback) {
                                        min6->port = 0;
                                        memset(min6->addr, 0,
                                               sizeof(min6->addr));
                                } else {
                                        in6_ptr in6;
                                        in6 = boost::get<in6_ptr>(addr.saddr);

                                        min6->port = in6->sin6_port;
                                        memcpy(min6->addr, 
                                               in6->sin6_addr.s6_addr,
                                               sizeof(min6->addr));
                                }
                                addr.id->to_binary(min6->id, sizeof(min6->id));

                                min6++;
                        }
                } else {
                        return;
                }


                // fill header
                reply->hdr.magic = htons(MAGIC_NUMBER);
                reply->hdr.ver   = htons(CAGE_VERSION);
                reply->hdr.type  = htons(type_dtun_find_node_reply);

                m_id.to_binary(reply->hdr.src, sizeof(reply->hdr.src));
                memcpy(reply->hdr.dst, find_node->hdr.src,
                       sizeof(reply->hdr.dst));


                reply->nonce  = find_node->nonce;
                reply->domain = find_node->domain;
                reply->num    = (uint8_t)nodes.size();

                memcpy(reply->id, find_node->id, sizeof(reply->id));


                // send
                m_udp.sendto(buf, size, from, fromlen);


                // add to rttable and cache
                cageaddr caddr;
                caddr = new_cageaddr(&find_node->hdr, from);

                if (ntohs(find_node->state) == state_global) {
                        add(caddr);
                }

                m_peers.add_node(caddr);

                delete buf;
        }

        void
        dtun::recv_find_node_reply(void *msg, int len, sockaddr *from)
        {
                msg_dtun_find_node_reply *reply;
                std::vector<cageaddr>     nodes;
                id_ptr    src(new uint160_t);
                query_ptr q;
                uint160_t dst, id;
                uint32_t  nonce;
                uint16_t  domain;
                int       size;
                _id       c_id;

                
                reply = (msg_dtun_find_node_reply*)msg;

                nonce = ntohl(reply->nonce);
                if (m_query.find(nonce) == m_query.end())
                        return;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (dst != m_id)
                        return;


                id.from_binary(reply->id, sizeof(reply->id));
                q = m_query[nonce];

                if (q->dst != id)
                        return;


                // stop timer
                src->from_binary(reply->hdr.src, sizeof(reply->hdr.src));
                c_id.id = src;

                if (q->timers.find(c_id) == q->timers.end()) {
                        timer_ptr t;
                        id_ptr    zero(new uint160_t);

                        zero->fill_zero();
                        c_id.id = zero;
                        if (q->timers.find(c_id) == q->timers.end())
                                return;
                        
                        t = q->timers[c_id];
                        m_timer.unset_timer(t.get());
                        q->timers.erase(c_id);
                } else {
                        timer_ptr t;
                        t = q->timers[c_id];
                        m_timer.unset_timer(t.get());
                        q->timers.erase(c_id);
                }


                // read nodes
                domain = ntohs(reply->domain);
                if (domain == domain_inet) {
                        msg_inet *min;
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                sizeof(*min) * reply->num;

                        if (size != len)
                                return;

                        min = (msg_inet*)reply->addrs;
                        for (int i = 0; i < reply->num; i++) {
                                cageaddr caddr;
                                id_ptr   p_id(new uint160_t);
                                in_ptr   p_in(new sockaddr_in);

                                p_id->from_binary(min->id, sizeof(min->id));

                                if (m_peers.is_timeout(p_id)) {
                                        min++;
                                        continue;
                                }

                                if (min->port == 0 && min->addr == 0) {
                                        memcpy(p_in.get(), from,
                                               sizeof(sockaddr_in));
                                } else {
                                        memset(p_in.get(), 0,
                                               sizeof(sockaddr_in));
                                        p_in->sin_family      = PF_INET;
                                        p_in->sin_port        = min->port;
                                        p_in->sin_addr.s_addr = min->addr;
                                }

                                caddr.id     = p_id;
                                caddr.domain = domain;
                                caddr.saddr  = p_in;

                                nodes.push_back(caddr);

                                min++;
                        }
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;
                        uint32_t   zero[4];
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                sizeof(*min6) * reply->num;

                        if (size != len)
                                return;

                        memset(zero, 0, sizeof(zero));
                        min6 = (msg_inet6*)reply->addrs;
                        for (int i = 0; i < reply->num; i++) {
                                cageaddr caddr;
                                id_ptr   p_id(new uint160_t);
                                in6_ptr  p_in6(new sockaddr_in6);

                                p_id->from_binary(min6->id, sizeof(min6->id));

                                if (m_peers.is_timeout(p_id)) {
                                        min6++;
                                        continue;
                                }

                                if (min6->port == 0 &&
                                    memcmp(min6->addr, zero,
                                           sizeof(zero)) == 0) {
                                        memcpy(p_in6.get(), from,
                                               sizeof(sockaddr_in6));
                                } else {
                                        memset(p_in6.get(), 0,
                                               sizeof(sockaddr_in6));
                                        p_in6->sin6_family = PF_INET6;
                                        p_in6->sin6_port   = min6->port;
                                        memcpy(p_in6->sin6_addr.s6_addr,
                                               min6->addr, sizeof(min6->addr));
                                }

                                caddr.id     = p_id;
                                caddr.domain = domain;
                                caddr.saddr  = p_in6;

                                nodes.push_back(caddr);

                                min6++;
                        }
                }


                cageaddr  caddr;
                timer_ptr t;
                _id       i;

                caddr = new_cageaddr(&reply->hdr, from);
                i.id = caddr.id;

                q->sent.insert(i);
                q->num_query--;


                // sort
                compare cmp;
                cmp.m_id = &id;
                std::sort(nodes.begin(), nodes.end(), cmp);

                // merge
                std::vector<cageaddr> tmp;
                std::vector<cageaddr>::iterator it1, it2;
                std::set<uint160_t> already;
                int                 n = 0;

                tmp = q->nodes;
                q->nodes.clear();

                it1 = tmp.begin();
                it2 = nodes.begin();

                while (n < num_find_node) {
                        if (it1 == tmp.end() && it2 == nodes.end()) {
                                break;
                        } 

                        if (it1 == tmp.end()) {
                                if (already.find(*it2->id) == already.end()) {
                                        q->nodes.push_back(*it2);
                                        already.insert(*it2->id);
                                }
                                ++it2;
                        } else if (it2 == nodes.end()) {
                                if (already.find(*it1->id) == already.end()) {
                                        q->nodes.push_back(*it1);
                                        already.insert(*it1->id);
                                }
                                ++it1;
                        } else if (*it1->id == *it2->id) {
                                if (already.find(*it1->id) == already.end()) {
                                        q->nodes.push_back(*it1);
                                        already.insert(*it1->id);
                                }
                                ++it1;
                                ++it2;
                        } else if ((*it1->id ^ id) < (*it2->id ^ id)) {
                                if (already.find(*it1->id) == already.end()) {
                                        q->nodes.push_back(*it1);
                                        already.insert(*it1->id);
                                }
                                ++it1;
                        } else {
                                if (already.find(*it2->id) == already.end()) {
                                        q->nodes.push_back(*it2);
                                        already.insert(*it2->id);
                                }
                                ++it2;
                        }

                        n++;
                }

                // send
                send_find(q);
        }

        void
        dtun::timer_register::operator() ()
        {
                m_dtun.m_timer.unset_timer(this);
                m_dtun.m_registering = false;
        }

        void
        dtun::register_callback::operator() (std::vector<cageaddr> &nodes)
        {
                BOOST_FOREACH(cageaddr &addr, nodes) {
                        msg_dtun_register reg;

                        memset(&reg, 0, sizeof(reg));

                        reg.hdr.magic = htons(MAGIC_NUMBER);
                        reg.hdr.ver   = htons(CAGE_VERSION);
                        reg.hdr.type  = htons(type_dtun_register);

                        p_dtun->m_id.to_binary(reg.hdr.src,
                                               sizeof(reg.hdr.src));
                        addr.id->to_binary(reg.hdr.dst, sizeof(reg.hdr.dst));

                        reg.session = p_dtun->m_register_session;

                        if (addr.domain == domain_inet) {
                                in_ptr in = boost::get<in_ptr>(addr.saddr);
                                p_dtun->m_udp.sendto(&reg, sizeof(reg),
                                                     (sockaddr*)in.get(),
                                                     sizeof(sockaddr_in));
                        } else if (addr.domain == domain_inet6) {
                                in6_ptr in6 = boost::get<in6_ptr>(addr.saddr);
                                p_dtun->m_udp.sendto(&reg, sizeof(reg),
                                                     (sockaddr*)in6.get(),
                                                     sizeof(sockaddr_in6));
                        }
                }
        }

        void
        dtun::register_node()
        {
                if (m_registering)
                        return;

                register_callback func;

                func.p_dtun = this;

                find_node(m_id, func);

                m_registering = true;


                // start timer
                timeval tval;

                tval.tv_sec  = register_timeout;
                tval.tv_usec = 0;

                m_timer.set_timer(&m_timer_register, &tval);
        }
}
