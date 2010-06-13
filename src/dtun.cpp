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

#include "ping.hpp"
#include "proxy.hpp"

#include <openssl/rand.h>

#include <boost/foreach.hpp>

namespace libcage {
        const int       dtun::num_find_node     = 10;
        const int       dtun::max_query         = 6;
        const int       dtun::query_timeout     = 2;
        const int       dtun::request_retry     = 2;
        const int       dtun::request_timeout   = 2;
        const int       dtun::registered_ttl    = 300;
        const int       dtun::timer_interval    = 30;
        const int       dtun::maintain_interval = 120;

        extern void no_action(std::vector<cageaddr> &nodes);

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

        dtun::dtun(rand_uint &rnd, rand_real &drnd, const uint160_t &id,
                   timer &t, peers &p, const natdetector &nat, udphandler &udp,
                   proxy &pr) :
                rttable(rnd, id, t, p),
                m_rnd(rnd),
                m_drnd(drnd),
                m_id(id),
                m_timer(t),
                m_peers(p),
                m_nat(nat),
                m_udp(udp),
                m_proxy(pr),
                m_registering(false),
                m_last_registered(0),
                m_timer_refresh(*this),
                m_is_enabled(true)
        {
                RAND_pseudo_bytes((unsigned char*)&m_register_session,
                                  sizeof(m_register_session));

                m_mask_bit = 1;
                m_last_maintain = 0;
        }

        dtun::~dtun()
        {

        }

        void
        dtun::maintain()
        {
                time_t diff = time(NULL) - m_last_maintain;

                if (diff < maintain_interval)
                        return;


                uint160_t bit = 1;
                uint160_t mask1, mask2;

                mask1 = ~(bit << (160 - m_mask_bit));
                mask2 = ~(bit << (160 - m_mask_bit - 1));

                find_node(m_id & mask1, no_action);
                find_node(m_id & mask2, no_action);

                m_mask_bit += 2;
                if (m_mask_bit > 20)
                        m_mask_bit = 1;

                m_last_maintain = time(NULL);
        }

        void
        dtun::send_ping(cageaddr &dst, uint32_t nonce)
        {
                send_ping_tmpl<msg_dtun_ping>(dst, nonce, type_dtun_ping,
                                              m_id, m_udp);
        }

        void
        dtun::recv_ping(void *msg, sockaddr *from, int fromlen)
        {
                if (m_nat.get_state() != node_global)
                        return;

                recv_ping_tmpl<msg_dtun_ping,
                        msg_dtun_ping_reply>(msg, from, fromlen,
                                             type_dtun_ping_reply,
                                             m_id, m_udp, m_peers);
        }

        void
        dtun::recv_ping_reply(void *msg, sockaddr *from, int fromlen)
        {
                recv_ping_reply_tmpl<msg_dtun_ping_reply>(msg, from, fromlen,
                                                          m_id, m_peers, this);
        }


        void
        dtun::find_node(std::string host, int port, callback_find_node func)
        {
                if (! m_is_enabled)
                        return;

                sockaddr_storage saddr;

                if (! m_udp.get_sockaddr(&saddr, host, port))
                        return;

                find_node((sockaddr*)&saddr, func);
        }

        void
        dtun::find_node(sockaddr *saddr, callback_find_node func)
        {
                if (! m_is_enabled)
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
                        nonce = m_rnd();
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
                        memcpy(in.get(), saddr, sizeof(sockaddr_in));
                        addr.saddr = in;
                } else if (addr.domain == domain_inet6) {
                        in6_ptr in6(new sockaddr_in6);
                        memcpy(in6.get(), saddr, sizeof(sockaddr_in6));
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

                if (q->nodes.size() == 1) {
                        if (is_find_value) {
                                callback_find_value f;
                                f = boost::get<callback_find_value>(func);
                                cageaddr addr1, addr2;
                                f(false, addr1, addr2);
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
                        nonce = m_rnd();
                } while (m_query.find(nonce) != m_query.end());

                q->nonce = nonce;
                m_query[nonce] = q;


                send_find(q);
        }

        void
        dtun::find_node(const uint160_t &dst, callback_find_node func)
        {
                if (! m_is_enabled)
                        return;

                find_nv(dst, func, false);
        }

        void
        dtun::find_value(const uint160_t &dst, callback_find_value func)
        {
                if (! m_is_enabled)
                        return;

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

                        q->num_query++;
                }

                if (q->num_query == 0) {
                        // call callback functions
                        if (q->is_find_value) {
                                cageaddr addr1, addr2;
                                callback_find_value func;
                                func = boost::get<callback_find_value>(q->func);
                                func(false, addr1, addr2);
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
                msg.hdr.ver   = CAGE_VERSION;
                msg.hdr.type  = type;
                msg.hdr.len   = htons(sizeof(msg));

                m_id.to_binary(msg.hdr.src, sizeof(msg.hdr.src));
                dst.id->to_binary(msg.hdr.dst, sizeof(msg.hdr.dst));

                msg.nonce  = htonl(q->nonce);
                msg.domain = htons(m_udp.get_domain());

                if (m_nat.get_state() == node_global) {
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
                char                      buf[1024 * 2];

                if (m_nat.get_state() != node_global) {
                        return;
                }

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

                        reply = (msg_dtun_find_node_reply*)buf;
                        min   = (msg_inet*)reply->addrs;

                        memset(reply, 0, size);

                        write_nodes_inet(min, nodes);
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                nodes.size() * sizeof(*min6);

                        reply = (msg_dtun_find_node_reply*)buf;
                        min6  = (msg_inet6*)reply->addrs;

                        memset(reply, 0, size);

                        write_nodes_inet6(min6, nodes);
                } else {
                        return;
                }


                // fill header
                reply->hdr.magic = htons(MAGIC_NUMBER);
                reply->hdr.ver   = CAGE_VERSION;
                reply->hdr.type  = type_dtun_find_node_reply;
                reply->hdr.len   = htons(size);

                m_id.to_binary(reply->hdr.src, sizeof(reply->hdr.src));
                memcpy(reply->hdr.dst, find_node->hdr.src,
                       sizeof(reply->hdr.dst));


                reply->nonce  = find_node->nonce;
                reply->domain = find_node->domain;
                reply->num    = (uint8_t)nodes.size();

                memcpy(reply->id, find_node->id, sizeof(reply->id));


                // send
                m_udp.sendto(reply, size, from, fromlen);


                // add to rttable and cache
                cageaddr caddr;
                caddr = new_cageaddr(&find_node->hdr, from);

                if (ntohs(find_node->state) == state_global) {
                        add(caddr);
                }

                m_peers.add_node(caddr);
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
                if (m_query.find(nonce) == m_query.end()) {
                        return;
                }

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (dst != m_id) {
                        return;
                }


                id.from_binary(reply->id, sizeof(reply->id));
                q = m_query[nonce];

                if (q->dst != id) {
                        return;
                }

                if (q->is_find_value) {
                        return;
                }


                // stop timer
                src->from_binary(reply->hdr.src, sizeof(reply->hdr.src));
                c_id.id = src;

                if (q->timers.find(c_id) == q->timers.end()) {
                        timer_ptr t;
                        id_ptr    zero(new uint160_t);

                        zero->fill_zero();
                        c_id.id = zero;
                        if (q->timers.find(c_id) == q->timers.end()) {
                                return;
                        }
                        
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

                        if (size != len) {
                                return;
                        }

                        min = (msg_inet*)reply->addrs;

                        read_nodes_inet(min, reply->num, nodes, from, m_peers);
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                sizeof(*min6) * reply->num;

                        if (size != len)
                                return;

                        min6 = (msg_inet6*)reply->addrs;

                        read_nodes_inet6(min6, reply->num, nodes, from,
                                         m_peers);
                } else {
                        return;
                }


                cageaddr  caddr;
                _id       i;

                caddr = new_cageaddr(&reply->hdr, from);
                i.id = caddr.id;

                q->sent.insert(i);
                q->num_query--;

                // add to rttable
                add(caddr);
                m_peers.add_node(caddr);


                // sort
                compare cmp;
                cmp.m_id = &id;
                std::sort(nodes.begin(), nodes.end(), cmp);

                // merge
                std::vector<cageaddr> tmp;

                tmp = q->nodes;
                q->nodes.clear();

                merge_nodes(id, q->nodes, tmp, nodes, num_find_node);

                // send
                send_find(q);
        }

        void
        dtun::register_callback::operator() (std::vector<cageaddr> &nodes)
        {
                p_dtun->m_registering = false;

                BOOST_FOREACH(cageaddr &addr, nodes) {
                        if (*addr.id == p_dtun->m_id)
                                continue;

                        msg_dtun_register reg;

                        memset(&reg, 0, sizeof(reg));

                        reg.hdr.magic = htons(MAGIC_NUMBER);
                        reg.hdr.ver   = CAGE_VERSION;
                        reg.hdr.type  = type_dtun_register;
                        reg.hdr.len   = htons(sizeof(reg));

                        src->to_binary(reg.hdr.src, sizeof(reg.hdr.src));
                        addr.id->to_binary(reg.hdr.dst, sizeof(reg.hdr.dst));

                        reg.session = htonl(session);

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
        dtun::register_node(id_ptr src, uint32_t session)
        {
                if (! m_is_enabled)
                        return;

                // find node
                register_callback func;

                func.p_dtun  = this;
                func.src     = src;
                func.session = session;

                m_registering = true;

                find_node(*src, func);
        }

        void
        dtun::register_node()
        {
                if (m_registering)
                        return;

                id_ptr src(new uint160_t);

                *src = m_id;

                register_node(src, m_register_session);
        }

        bool
        dtun::registered::operator== (const registered &rhs) const
        {
                if (*addr.id != *rhs.addr.id) {
                        return false;
                } else if (addr.domain != rhs.addr.domain) {
                        return false;
                } else if (addr.domain == domain_inet) {
                        in_ptr in1, in2;
                        
                        in1 = boost::get<in_ptr>(addr.saddr);
                        in2 = boost::get<in_ptr>(rhs.addr.saddr);

                        if (in1->sin_port != in2->sin_port) {
                                return false;
                        } else if (in1->sin_addr.s_addr !=
                                   in2->sin_addr.s_addr) {
                                return false;
                        } else {
                                return true;
                        }
                } else if (addr.domain == domain_inet6) {
                        in6_ptr in1, in2;
                        
                        in1 = boost::get<in6_ptr>(addr.saddr);
                        in2 = boost::get<in6_ptr>(rhs.addr.saddr);

                        if (in1->sin6_port != in2->sin6_port) {
                                return false;
                        } else if (memcmp(in1->sin6_addr.s6_addr,
                                          in2->sin6_addr.s6_addr,
                                          sizeof(in6_addr)) != 0) {
                                return false;
                        } else {
                                return true;
                        }
                }
                
                throw "must not reach";

                return false;
        }

        void
        dtun::recv_register(void *msg, sockaddr *from)
        {
                msg_dtun_register *reg = (msg_dtun_register*)msg;
                registered         r;
                uint160_t          fromid;
                _id                i;

                fromid.from_binary(reg->hdr.dst, sizeof(reg->hdr.dst));
                if (fromid != m_id)
                        return;

                r.addr    = new_cageaddr(&reg->hdr, from);
                r.session = ntohl(reg->session);
                r.t       = time(NULL);

                i.id = r.addr.id;


                std::map<_id, registered>::iterator it;
                it = m_registered_nodes.find(i);

                if (it == m_registered_nodes.end()) {
                        m_registered_nodes[i] = r;
                } else if (it->second.session == r.session) {
                        it->second = r;
                } else if (it->second == r) {
                        it->second.t = r.t;
                }

                m_peers.add_node(r.addr, r.session);
        }

        void
        dtun::recv_find_value(void *msg, sockaddr *from, int fromlen)
        {
                msg_dtun_find_value_reply      *reply;
                msg_dtun_find_value *find_value;
                cageaddr             caddr;
                uint160_t            fromid;
                uint16_t             domain;
                char                 buf[1024 * 2];
                id_ptr               id(new uint160_t);

                if (m_nat.get_state() != node_global) {
                        return;
                }

                find_value = (msg_dtun_find_value*)msg;

                fromid.from_binary(find_value->hdr.dst,
                                   sizeof(find_value->hdr.dst));

                if (fromid != m_id) {
                        return;
                }

                domain = ntohs(find_value->domain);
 
                if (domain != m_udp.get_domain()) {
                        return;
                }

                memset(buf, 0, sizeof(buf));

                // fill header
                reply = (msg_dtun_find_value_reply*)buf;
                reply->hdr.magic = htons(MAGIC_NUMBER);
                reply->hdr.ver   = CAGE_VERSION;
                reply->hdr.type  = type_dtun_find_value_reply;

                memcpy(reply->hdr.dst, find_value->hdr.src,
                       sizeof(reply->hdr.dst));
                m_id.to_binary(reply->hdr.src, sizeof(reply->hdr.src));


                reply->nonce  = find_value->nonce;
                reply->domain = find_value->domain;

                memcpy(reply->id, find_value->id, sizeof(reply->id));

                id->from_binary(find_value->id, sizeof(find_value->id));

                // add to rttable
                caddr = new_cageaddr(&find_value->hdr, from);
                if (find_value->state == state_global) {
                        add(caddr);
                }
                m_peers.add_node(caddr);


                // send value
                _id i;
                i.id = id;

                if (m_registered_nodes.find(i) !=
                    m_registered_nodes.end()) {
                        registered reg = m_registered_nodes[i];
                        
                        reply->num  = 1;
                        reply->flag = 1;

                        if (reg.addr.domain == domain_inet) {
                                msg_inet *min;
                                in_ptr    in;
                                int       size;

                                in  = boost::get<in_ptr>(reg.addr.saddr);
                                min = (msg_inet*)reply->addrs;

                                min->port = in->sin_port;
                                min->addr = in->sin_addr.s_addr;

                                reg.addr.id->to_binary(min->id,
                                                       sizeof(min->id));

                                size = sizeof(*reply) - sizeof(reply->addrs) +
                                        sizeof(*min);

                                reply->hdr.len = htons(size);

                                m_udp.sendto(reply, size, from, fromlen);

                                return;
                        } else if (reg.addr.domain == domain_inet6) {
                                msg_inet6 *min6;
                                in6_ptr    in6;
                                int        size;

                                in6  = boost::get<in6_ptr>(reg.addr.saddr);
                                min6 = (msg_inet6*)reply->addrs;

                                min6->port = in6->sin6_port;
                                memcpy(min6->addr, in6->sin6_addr.s6_addr,
                                       sizeof(min6->addr));

                                reg.addr.id->to_binary(min6->id,
                                                       sizeof(min6->id));

                                size = sizeof(*reply) - sizeof(reply->addrs) +
                                        sizeof(*min6);

                                reply->hdr.len = htons(size);

                                m_udp.sendto(reply, size, from, fromlen);

                                return;
                        }
                }

                // send nodes
                std::vector<cageaddr> nodes;
                int size = 0;

                lookup(*id, num_find_node, nodes);

                if (domain == domain_inet) {
                        msg_inet *min;

                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                nodes.size() * sizeof(*min);

                        min   = (msg_inet*)reply->addrs;

                        write_nodes_inet(min, nodes);
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;

                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                nodes.size() * sizeof(*min6);

                        min6  = (msg_inet6*)reply->addrs;

                        write_nodes_inet6(min6, nodes);
                } else {
                        return;
                }

                reply->hdr.len = htons(size);
                reply->num     = nodes.size();

                m_udp.sendto(reply, size, from, fromlen);
        }

        void
        dtun::recv_find_value_reply(void *msg, int len, sockaddr *from)
        {
                msg_dtun_find_value_reply *reply;
                std::vector<cageaddr>      nodes;
                id_ptr    src(new uint160_t);
                query_ptr q;
                uint160_t dst, id;
                uint32_t  nonce;
                uint16_t  domain;
                int       size;
                _id       c_id;

                reply = (msg_dtun_find_value_reply*)msg;

                nonce = ntohl(reply->nonce);
                if (m_query.find(nonce) == m_query.end()) {
                        return;
                }

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (dst != m_id) {
                        return;
                }

                id.from_binary(reply->id, sizeof(reply->id));
                q = m_query[nonce];

                if (q->dst != id) {
                        return;
                }

                if (! q->is_find_value) {
                        return;
                }

                // stop timer
                src->from_binary(reply->hdr.src, sizeof(reply->hdr.src));
                c_id.id = src;

                if (q->timers.find(c_id) != q->timers.end()) {
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

                        if (size != len) {
                                return;
                        }

                        min = (msg_inet*)reply->addrs;

                        read_nodes_inet(min, reply->num, nodes, from,
                                        m_peers);
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                sizeof(*min6) * reply->num;

                        if (size != len) {
                                return;
                        }

                        min6 = (msg_inet6*)reply->addrs;

                        read_nodes_inet6(min6, reply->num, nodes, from,
                                         m_peers);
                }

                cageaddr  caddr;
                timer_ptr t;
                _id       i;

                caddr = new_cageaddr(&reply->hdr, from);
                i.id = caddr.id;

                q->sent.insert(i);
                q->num_query--;

                // add to rttable
                add(caddr);
                m_peers.add_node(caddr);


                // finish find value
                if (reply->flag == 1 && nodes.size() > 0) {
                        // stop all timer
                        std::map<_id, timer_ptr>::iterator it;
                        for (it = q->timers.begin(); it != q->timers.end();
                             ++it) {
                                m_timer.unset_timer(it->second.get());
                        }

                        // call callback
                        callback_find_value func;
                        func = boost::get<callback_find_value>(q->func);
                        func(true, nodes[0], caddr);

                        m_query.erase(nonce);

                        return;
                }


                // sort
                compare cmp;
                cmp.m_id = &id;
                std::sort(nodes.begin(), nodes.end(), cmp);

                // merge
                std::vector<cageaddr> tmp;

                tmp = q->nodes;
                q->nodes.clear();

                merge_nodes(id, q->nodes, tmp, nodes, num_find_node);

                // send
                send_find(q);
        }

        void
        dtun::timer_request::operator() ()
        {
                cageaddr addr;
                req_ptr  q;

                q = p_dtun->m_request[nonce];

                q->func(false, addr);

                p_dtun->m_request.erase(nonce);
        }

        void
        dtun::request_find_value::operator() (bool result, cageaddr &addr,
                                              cageaddr &from)
        {
                req_ptr q;

                q = p_dtun->m_request[nonce];

                if (! result) {
                        q->retry--;
                        if (q->retry > 0) {
                                p_dtun->find_value(q->dst, *this);
                        } else {
                                q->func(result, addr);
                                p_dtun->m_request.erase(nonce);
                        }
                        return;
                }

                // start timer
                timeval tval;

                tval.tv_sec  = 2;
                tval.tv_usec = 0;

                q->timer_req.nonce     = nonce;
                q->timer_req.p_dtun    = p_dtun;
                q->finished_find_value = true;

                p_dtun->m_timer.set_timer(&q->timer_req, &tval);


                // send ping
                msg_hdr hdr;

                hdr.magic = htons(MAGIC_NUMBER);
                hdr.ver   = CAGE_VERSION;
                hdr.type  = type_undefined;
                hdr.len   = htons(sizeof(hdr));

                p_dtun->m_id.to_binary(hdr.src, sizeof(hdr.src));
                addr.id->to_binary(hdr.dst, sizeof(hdr.dst));

                if (addr.domain == domain_inet) {
                        in_ptr in;
                        in = boost::get<in_ptr>(addr.saddr);
                        p_dtun->m_udp.sendto(&hdr, sizeof(hdr),
                                             (sockaddr*)in.get(),
                                             sizeof(sockaddr_in));
                } else if (addr.domain == domain_inet6) {
                        in6_ptr in6;
                        in6 = boost::get<in6_ptr>(addr.saddr);
                        p_dtun->m_udp.sendto(&hdr, sizeof(hdr),
                                             (sockaddr*)in6.get(),
                                             sizeof(sockaddr_in6));
                }


                // send request
                msg_dtun_request req;

                memset(&req, 0, sizeof(req));

                req.hdr.magic = htons(MAGIC_NUMBER);
                req.hdr.ver   = CAGE_VERSION;
                req.hdr.type  = type_dtun_request;
                req.hdr.len   = htons(sizeof(req));

                p_dtun->m_id.to_binary(req.hdr.src, sizeof(req.hdr.src));
                from.id->to_binary(req.hdr.dst, sizeof(req.hdr.dst));

                addr.id->to_binary(req.id, sizeof(req.id));

                req.nonce = htonl(nonce);

                // send
                if (from.domain == domain_inet) {
                        in_ptr in;
                        in = boost::get<in_ptr>(from.saddr);
                        p_dtun->m_udp.sendto(&req, sizeof(req),
                                             (sockaddr*)in.get(),
                                             sizeof(sockaddr_in));
                } else if (from.domain == domain_inet6) {
                        in6_ptr in6;
                        in6 = boost::get<in6_ptr>(from.saddr);
                        p_dtun->m_udp.sendto(&req, sizeof(req),
                                             (sockaddr*)in6.get(),
                                             sizeof(sockaddr_in6));
                }
        }

        void
        dtun::request(const uint160_t &dst, callback_request func)
        {
                request_find_value fv;
                req_ptr  q(new request_query);
                uint32_t nonce;

                do {
                        nonce = m_rnd();
                } while (m_request.find(nonce) != m_request.end());

                q->func   = func;
                q->retry  = request_retry;
                q->p_dtun = this;
                q->dst    = dst;
                q->finished_find_value = false;

                m_request[nonce] = q;

                
                fv.nonce  = nonce;
                fv.p_dtun = this;

                find_value(dst, fv);
        }

        void
        dtun::recv_request(void *msg, sockaddr *from, int fromlen)
        {
                msg_dtun_request *req;
                cageaddr          addr;
                uint160_t         dst;
                id_ptr            id(new uint160_t);

                if (m_nat.get_state() != node_global)
                        return;

                req = (msg_dtun_request*)msg;

                dst.from_binary(req->hdr.dst, sizeof(req->hdr.dst));

                if (dst != m_id)
                        return;

                // add to request cache
                addr = new_cageaddr(&req->hdr, from);
                m_peers.add_node(addr);


                id->from_binary(req->id, sizeof(req->id));

                if (*id == m_id) {
                        // send request reply
                        msg_dtun_request_reply reply;

                        reply.hdr.magic = htons(MAGIC_NUMBER);
                        reply.hdr.ver   = CAGE_VERSION;
                        reply.hdr.type  = type_dtun_request_reply;
                        reply.hdr.len   = htons(sizeof(reply));

                        m_id.to_binary(reply.hdr.src, sizeof(reply.hdr.src));
                        memcpy(reply.hdr.dst, req->hdr.src,
                               sizeof(reply.hdr.dst));

                        reply.nonce = req->nonce;

                        // send
                        m_udp.sendto(&reply, sizeof(reply), from, fromlen);
                } else {
                        // send request by
                        msg_dtun_request_by *req_by;
                        registered reg;
                        uint16_t   size;
                        char       buf[1024];
                        _id        i;

                        i.id = id;

                        if (m_registered_nodes.find(i) ==
                            m_registered_nodes.end()) {
                                return;
                        }

                        reg = m_registered_nodes[i];

                        if (reg.addr.domain != addr.domain)
                                return;

                        memset(buf, 0, sizeof(buf));
                        req_by = (msg_dtun_request_by*)buf;

                        req_by->hdr.magic = htons(MAGIC_NUMBER);
                        req_by->hdr.ver   = CAGE_VERSION;
                        req_by->hdr.type  = type_dtun_request_by;

                        m_id.to_binary(req_by->hdr.src,
                                       sizeof(req_by->hdr.src));
                        reg.addr.id->to_binary(req_by->hdr.dst,
                                               sizeof(req_by->hdr.dst));

                        req_by->nonce = req->nonce;

                        if (addr.domain == domain_inet) {
                                sockaddr_in *sin;
                                in_ptr       in;
                                msg_inet    *min;

                                size = sizeof(*req_by) - sizeof(req_by->addr) +
                                        sizeof(*min);

                                req_by->hdr.len = htons(size);
                                req_by->domain  = htons(domain_inet);

                                sin = (sockaddr_in*)from;
                                min = (msg_inet*)req_by->addr;

                                min->port = sin->sin_port;
                                min->addr = sin->sin_addr.s_addr;

                                addr.id->to_binary(min->id,
                                                   sizeof(min->id));

                                in = boost::get<in_ptr>(reg.addr.saddr);

                                m_udp.sendto(req_by, size,
                                             (sockaddr*)in.get(),
                                             sizeof(sockaddr_in));
                        } else if (addr.domain == domain_inet6) {
                                sockaddr_in6 *sin6;
                                in6_ptr       in6;
                                msg_inet6    *min6;

                                size = sizeof(*req_by) - sizeof(req_by->addr) +
                                        sizeof(*min6);

                                req_by->hdr.len = htons(size);
                                req_by->domain  = htons(domain_inet6);

                                sin6 = (sockaddr_in6*)from;
                                min6 = (msg_inet6*)req_by->addr;

                                min6->port = sin6->sin6_port;

                                memcpy(min6->addr, sin6->sin6_addr.s6_addr,
                                       sizeof(in6_addr));

                                addr.id->to_binary(min6->id,
                                                   sizeof(min6->id));

                                in6 = boost::get<in6_ptr>(reg.addr.saddr);
                                m_udp.sendto(req_by, size,
                                             (sockaddr*)in6.get(),
                                             sizeof(sockaddr_in6));
                        }
                }
        }

        void
        dtun::recv_request_by(void *msg, int len, sockaddr *from)
        {
                msg_dtun_request_reply  reply;
                msg_dtun_request_by    *req;
                id_ptr    dst(new uint160_t);
                uint16_t  domain;
                int       size;

                req = (msg_dtun_request_by*)msg;

                dst->from_binary(req->hdr.dst, sizeof(req->hdr.dst));
                if (*dst != m_id) {
                        if (! m_proxy.is_registered(dst))
                                return;

                        dst->to_binary(reply.hdr.src, sizeof(reply.hdr.src));
                } else {
                        m_id.to_binary(reply.hdr.src, sizeof(reply.hdr.src));
                }

                reply.hdr.magic = htons(MAGIC_NUMBER);
                reply.hdr.ver   = CAGE_VERSION;
                reply.hdr.type  = type_dtun_request_reply;
                reply.hdr.len   = htons(sizeof(reply));

                reply.nonce = req->nonce;

                domain = ntohs(req->domain);
                if (domain == domain_inet) {
                        sockaddr_in  sin;
                        msg_inet    *min;

                        size = sizeof(*req) - sizeof(req->addr) + sizeof(*min);

                        if (size != len)
                                return;

                        min = (msg_inet*)req->addr;

                        memcpy(reply.hdr.dst, min->id, sizeof(reply.hdr.dst));
                        memset(&sin, 0, sizeof(sin));

                        sin.sin_family = PF_INET;
                        sin.sin_port   = min->port;
                        sin.sin_addr.s_addr = min->addr;

                        m_udp.sendto(&reply, sizeof(reply), (sockaddr*)&sin,
                                     sizeof(sin));
                } else if (domain == domain_inet6) {
                        sockaddr_in6  sin6;
                        msg_inet6    *min6;

                        size = sizeof(*req) - sizeof(req->addr) + sizeof(*min6);
                        
                        if (size != len)
                                return;

                        min6 = (msg_inet6*)req->addr;

                        memcpy(reply.hdr.dst, min6->id, sizeof(reply.hdr.dst));
                        memset(&sin6, 0, sizeof(sin6));

                        sin6.sin6_family = PF_INET6;
                        sin6.sin6_port   = min6->port;
                        memcpy(sin6.sin6_addr.s6_addr, min6->addr,
                               sizeof(in6_addr));

                        m_udp.sendto(&reply, sizeof(reply), (sockaddr*)&sin6,
                                     sizeof(sin6));
                }
        }

        void
        dtun::recv_request_reply(void *msg, sockaddr *from)
        {
                msg_dtun_request_reply *reply;
                req_ptr   q;
                cageaddr  addr;
                uint160_t dst;
                uint32_t  nonce;

                reply = (msg_dtun_request_reply*)msg;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (dst != m_id)
                        return;

                nonce = ntohl(reply->nonce);
                if (m_request.find(nonce) == m_request.end())
                        return;

                q = m_request[nonce];


                addr = new_cageaddr(&reply->hdr, from);
                if (q->dst != *addr.id)
                        return;


                m_request.erase(nonce);

                // stop timer
                m_timer.unset_timer(&q->timer_req);

                // add to request cache
                m_peers.add_node(addr);

                // call callback function
                q->func(true, addr);
        }

        void
        dtun::refresh()
        {
                std::map<_id, registered>::iterator it;
                time_t now = time(NULL);

                for (it = m_registered_nodes.begin();
                     it != m_registered_nodes.end();) {

                        time_t diff = now - it->second.t;
                        if (diff > registered_ttl) {
                                m_registered_nodes.erase(it++);
                        } else {
                                ++it;
                        }
                }
        }

        void
        dtun::timer_refresh::operator() ()
        {
                if (! m_dtun.m_is_enabled)
                        return;

                m_dtun.refresh();
                m_dtun.register_node();
                m_dtun.maintain();

                if (m_dtun.is_zero()) {
                        try {
                                cageaddr addr;

                                addr = m_dtun.m_peers.get_first();

                                if (addr.domain == domain_inet) {
                                        in_ptr in;
                                        in = boost::get<in_ptr>(addr.saddr);
                                        m_dtun.find_node((sockaddr*)in.get(),
                                                         &no_action);
                                } else if (addr.domain ==
                                           domain_inet6) {
                                        in6_ptr in6;
                                        in6 = boost::get<in6_ptr>(addr.saddr);
                                        m_dtun.find_node((sockaddr*)in6.get(), &no_action);
                                }
                        } catch (std::out_of_range) {
                        }
                }


                // reschedule
                timeval       tval;

                tval.tv_sec  = (long)((double)dtun::timer_interval *
                                      m_dtun.m_drnd() +
                                      (double)dtun::timer_interval);
                tval.tv_usec = 0;

                m_dtun.m_timer.set_timer(this, &tval);
        }

        void
        dtun::set_enabled(bool enabled)
        {
                m_is_enabled = enabled;
        }
}
