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

#include <boost/foreach.hpp>

namespace libcage {
        const int       dht::num_find_node    = 6;
        const int       dht::max_query        = 3;
        const int       dht::query_timeout    = 3;

        size_t
        hash_value(const dht::_id &i)
        {
                return i.id->hash_value();
        }

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

        void
        dht::send_msg(msg_hdr *msg, uint16_t len, uint8_t type, cageaddr &dst)
        {
                msg->magic = htons(MAGIC_NUMBER);
                msg->ver   = CAGE_VERSION;
                msg->type  = type;
                msg->len   = htons(len);

                dst.id->to_binary(msg->dst, sizeof(msg->dst));
                m_id.to_binary(msg->src, sizeof(msg->src));

                if (dst.domain == domain_inet) {
                        in_ptr in;
                        in = boost::get<in_ptr>(dst.saddr);

                        m_udp.sendto(msg, len, (sockaddr*)in.get(),
                                     sizeof(sockaddr_in));
                } else if (dst.domain == domain_inet6) {
                        in6_ptr in6;
                        in6 = boost::get<in6_ptr>(dst.saddr);

                        m_udp.sendto(msg, len, (sockaddr*)in6.get(),
                                     sizeof(sockaddr_in6));
                }
        }

        void
        dht::find_nv(const uint160_t &dst, callback_func func,
                     bool is_find_value, char *key = NULL, int keylen = 0)
        {
                query_ptr q(new query);

                lookup(dst, num_find_node, q->nodes);

                if (q->nodes.size() == 0) {
                        if (is_find_value) {
                                callback_find_value f;
                                f = boost::get<callback_find_value>(func);
                                f(false, NULL, 0);
                        } else {
                                callback_find_node f;
                                f = boost::get<callback_find_node>(func);
                                f(q->nodes);
                        }
                        return;
                }

                id_ptr p_dst(new uint160_t);
                *p_dst = dst;

                q->dst           = p_dst;
                q->num_query     = 0;
                q->is_find_value = is_find_value;
                q->func          = func;

                if (is_find_value) {
                        q->key    = boost::shared_array<char>(new char[keylen]);
                        q->keylen = keylen;
                        memcpy(q->key.get(), key, keylen);
                }

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
        dht::find_node(const uint160_t &dst, callback_find_node func)
        {
                find_nv(dst, func, false);
        }

        void
        dht::send_find(query_ptr q)
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

                        t->nonce = q->nonce;
                        t->id    = i;
                        t->p_dht = this;

                        tval.tv_sec  = query_timeout;
                        tval.tv_usec = 0;

                        q->timers[i] = t;
                        q->sent.insert(i);

                        m_timer.set_timer(t.get(), &tval);


                        // send find node
                        if (q->is_find_value) {
                                // TODO:
                                // send_find_value(addr, q);
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
                                func(false, NULL, 0);
                        } else {
                                callback_find_node func;
                                func = boost::get<callback_find_node>(q->func);
                                func(q->nodes);
                        }

                        // stop all timers
                        boost::unordered_map<_id, timer_ptr>::iterator it;
                        for (it = q->timers.begin(); it != q->timers.end();
                             ++it) {
                                m_timer.unset_timer(it->second.get());
                        }

                        // remove query
                        m_query.erase(q->nonce);
                }
        }

        void
        dht::find_node_func::operator() (bool result, cageaddr &addr)
        {
                if (! result)
                        return;

                msg_dht_find_node msg;

                memset(&msg, 0, sizeof(msg));

                msg.nonce  = htons(nonce);
                msg.domain = htons(addr.domain);

                dst->to_binary(msg.id, sizeof(msg.id));

                p_dht->send_msg(&msg.hdr, sizeof(msg), type_dht_find_node,
                                addr);
        }

        void
        dht::send_find_node(cageaddr &dst, query_ptr q)
        {
                try {
                        m_peers.get_addr(dst.id);

                        msg_dht_find_node msg;

                        memset(&msg, 0, sizeof(msg));

                        msg.nonce  = htons(q->nonce);
                        msg.domain = htons(dst.domain);

                        q->dst->to_binary(msg.id, sizeof(msg.id));

                        send_msg(&msg.hdr, sizeof(msg), type_dht_find_node,
                                 dst);
                } catch (std::out_of_range) {
                        find_node_func func;

                        func.dst   = q->dst;
                        func.nonce = q->nonce;
                        func.p_dht = this;

                        m_dtun.request(*dst.id, func);
                }
        }

        void
        dht::timer_query::operator() ()
        {
                query_ptr q = p_dht->m_query[nonce];
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

                        p_dht->m_peers.add_timeout(id.id);
                        p_dht->remove(*id.id);
                }

                // send find node
                p_dht->send_find(q);
        }

        void
        dht::recv_find_node(void *msg, sockaddr *from)
        {
                msg_dht_find_node_reply *reply;
                msg_dht_find_node       *req;
                std::vector<cageaddr>    nodes;
                uint160_t dst;
                uint160_t zero;
                uint160_t id;
                uint16_t  domain;
                cageaddr  addr;
                char      buf[1024 * 2];
                int       len = 0;

                req = (msg_dht_find_node*)msg;

                zero.fill_zero();
                dst.from_binary(req->hdr.dst, sizeof(req->hdr.dst));
                if (dst != m_id && dst != zero)
                        return;

                domain = ntohs(req->domain);
                if (domain != m_udp.get_domain())
                        return;

                addr = new_cageaddr(&req->hdr, from);

                // add to rttable and request cache
                m_peers.add_node(addr);
                add(addr);

                // lookup
                id.from_binary(req->id, sizeof(req->id));
                lookup(id, num_find_node, nodes);

                // send reply
                memset(buf, 0, sizeof(buf));
                reply = (msg_dht_find_node_reply*)buf;

                reply->nonce  = req->nonce;
                reply->domain = req->domain;
                reply->num    = htons(nodes.size());

                memcpy(reply->id, req->id, sizeof(reply->id));

                if (domain == domain_inet) {
                        msg_inet *min;

                        len = sizeof(msg_inet) * nodes.size() +
                                sizeof(*reply) - sizeof(reply->addrs);

                        min   = (msg_inet*)reply->addrs;
                        write_nodes_inet(min, nodes);
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;

                        len = sizeof(msg_inet6) * nodes.size() +
                                sizeof(*reply) - sizeof(reply->addrs);

                        min6  = (msg_inet6*)reply->addrs;
                        write_nodes_inet6(min6, nodes);
                } else {
                        return;
                }

                send_msg(&reply->hdr, len, type_dht_find_node_reply, addr);
        }

        void
        dht::recv_find_node_reply(void *msg, int len, sockaddr *from)
        {
                msg_dht_find_node_reply *reply;
                std::vector<cageaddr>    nodes;
                query_ptr q;
                uint160_t dst, id;
                cageaddr  addr;
                uint32_t  nonce;
                uint16_t  domain;
                int       size;
                _id       c_id;


                reply = (msg_dht_find_node_reply*)msg;

                nonce = ntohl(reply->nonce);
                if (m_query.find(nonce) == m_query.end())
                        return;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (dst != m_id)
                        return;

                id.from_binary(reply->id, sizeof(reply->id));
                q = m_query[nonce];

                if (*q->dst != id)
                        return;

                if (q->is_find_value)
                        return;

                addr = new_cageaddr(&reply->hdr, from);

                // add rttable and request cache
                m_peers.add_node(addr);
                add(addr);

                // stop timer
                c_id.id = addr.id;
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

                _id i;

                i.id = addr.id;

                q->sent.insert(i);
                q->num_query--;

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
}
