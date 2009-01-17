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

#include "rttable.hpp"


namespace libcage {
        const int       rttable::max_entry = 20;
        const int       rttable::ping_timeout = 2;

        void
        rttable::timer_ping::operator() ()
        {
                std::list<cageaddr> &row = m_rttable->m_table[m_i];

                row.pop_front();
                row.push_back(m_addr_new);

                m_rttable->m_ping_send.erase(m_i);
                m_rttable->m_nodes.erase(*m_addr_old.id);
                m_rttable->m_nodes.insert(*m_addr_new.id);

                // add timed out
                m_rttable->m_peers.add_timeout(m_addr_old.id);

                // delete this
                // do not reference menber variables after this code
                m_rttable->m_ping_wait.erase(m_nonce);
        }

        rttable::rttable(const uint160_t &id, timer &t, peers &p) :
                m_id(id), m_timer(t), m_peers(p)
        {

        }

        rttable::~rttable()
        {

        }

        void
        rttable::add(const cageaddr &addr)
        {
                int i;

                if (*addr.id == m_id)
                        return;

                i = id2i(*addr.id);

                if (i < 0 || m_nodes.find(*addr.id) != m_nodes.end())
                        return;

                if (m_table.find(i) == m_table.end()) {
                        m_table[i].push_back(addr);
                        m_nodes.insert(*addr.id);
                } else if ((int)m_table[i].size() < max_entry) {
                        m_table[i].push_back(addr);
                        m_nodes.insert(*addr.id);
                } else if (m_ping_send.find(i) != m_ping_send.end()){
                        return;
                } else {
                        uint32_t nonce;
                        for (;;) {
                                nonce = (uint32_t)mrand48();
                                if (m_ping_wait.find(nonce) ==
                                    m_ping_wait.end())
                                        break;
                        }


                        // start timer
                        cageaddr  &addr_old(m_table[i].front());
                        timer_ptr  func(new timer_ping);
                        timeval    tval;
                        
                        func->m_rttable  = this;
                        func->m_addr_old = addr_old;
                        func->m_addr_new = addr;
                        func->m_nonce    = nonce;
                        func->m_i        = i;

                        tval.tv_sec  = ping_timeout;
                        tval.tv_usec = 0;
                        m_timer.set_timer(func.get(), &tval);


                        // set state
                        m_ping_wait[nonce] = func;
                        m_ping_send.insert(i);


                        // send ping
                        send_ping(addr_old, nonce);
                }
        }

        void
        rttable::remove(const uint160_t &id)
        {
                if (m_nodes.find(id) == m_nodes.end())
                        return;

                int                  i = id2i(id);
                std::list<cageaddr> &row = m_table[i];

                std::list<cageaddr>::iterator it;
                for (it = row.begin(); it != row.end(); ++it) {
                        if (*it->id == id) {
                                row.erase(it);
                                break;
                        }
                }

                m_nodes.erase(id);
                
        }

        class compare {
        public:
                const uint160_t        *m_id;

                bool operator() (const cageaddr &lhs,
                                 const cageaddr &rhs) const
                {
                        return (*m_id ^ *lhs.id) < (*m_id ^ *rhs.id);
                }
        };

        void
        rttable::lookup(const uint160_t &id, int num,
                        std::vector<cageaddr> &ret)
        {
                std::vector<int> is;
                int              n;

                n = id2i4lookup(id, num, is);

                if (n < num)
                        id2i4lookupR(id, num - n, is);

                std::vector<int>::iterator    i;
                std::list<cageaddr>::iterator j;

                for (i = is.begin(); i != is.end(); ++i) {
                        if (*i == -1) {
                                cageaddr addr;
                                addr.id  = id_ptr(new uint160_t);
                                *addr.id = m_id;

                                addr.domain = domain_loopback;
                                ret.push_back(addr);
                        } else {
                                std::list<cageaddr> &row = m_table[*i];
                                for (j = row.begin(); j != row.end(); ++j) {
                                        ret.push_back(*j);
                                }
                        }
                }

                compare cmp;
                cmp.m_id = &id;

                std::sort(ret.begin(), ret.end(), cmp);

                if ((int)ret.size() > num)
                        ret.resize(num);
        }

        void
        rttable::recv_ping_reply(cageaddr &src, uint32_t nonce)
        {
                if (m_ping_wait.find(nonce) == m_ping_wait.end())
                        return;

                timer_ptr t = m_ping_wait[nonce];

                if (*src.id != *t->m_addr_old.id ||
                    src.domain != t->m_addr_old.domain) {
                        return;
                }

                if (src.domain == domain_inet) {
                        in_ptr in1, in2;
                        in1 = boost::get<in_ptr>(src.saddr);
                        in2 = boost::get<in_ptr>(t->m_addr_old.saddr);

                        if (in1->sin_port == in2->sin_port &&
                            in1->sin_addr.s_addr == in2->sin_addr.s_addr) {
                                std::list<cageaddr> &row = m_table[t->m_i];
                                cageaddr             addr;

                                row.pop_front();
                                row.push_back(t->m_addr_old);

                                m_timer.unset_timer(t.get());
                                m_ping_wait.erase(nonce);
                                m_ping_send.erase(t->m_i);
                        }
                } else if (src.domain == domain_inet6) {
                        in6_ptr in6_1, in6_2;
                        in6_1 = boost::get<in6_ptr>(src.saddr);
                        in6_2 = boost::get<in6_ptr>(t->m_addr_old.saddr);

                        if (in6_1->sin6_port == in6_2->sin6_port &&
                            memcmp(&in6_1->sin6_addr, &in6_2->sin6_addr,
                                   sizeof(in6_addr) == 0)) {
                                std::list<cageaddr> &row = m_table[t->m_i];
                                cageaddr             addr;

                                row.pop_front();
                                row.push_back(t->m_addr_old);

                                m_timer.unset_timer(t.get());
                                m_ping_wait.erase(nonce);
                                m_ping_send.erase(t->m_i);
                        }
                }
        }

        void
        rttable::send_ping(cageaddr &dst, uint32_t nonce)
        {

        }

        int
        rttable::id2i(const uint160_t &id)
        {
                uint160_t d;
                uint160_t mask;
                uint160_t zero;
                int       i;

                d      = m_id ^ id;
                mask   = 1;
                mask <<= 159;
                i      = 159;

                zero.fill_zero();

                for (i = 159; i >= 0; i--) {
                        if ((d & mask) > zero)
                                break;
                        mask >>= 1;
                }
                
                return i;
        }

        int
        rttable::id2i4lookup(const uint160_t &id, int max,
                             std::vector<int> &ret)
        {
                uint160_t id0 = id;
                uint160_t id1 = 1;
                int       i;
                int       n = 0;

                while (n < max) {
                        uint160_t id3 = m_id ^ id0;
                        if (id3.is_zero()) {
                                n++;
                                ret.push_back(-1);
                        }

                        i = id2i(id0);
                        if (m_table.find(i) != m_table.end()) {
                                n += m_table[i].size();
                                ret.push_back(i);
                        }

                        id0 ^= (id1 << i);
                }

                return n;
        }

        int
        rttable::id2i4lookupR(const uint160_t &id, int max,
                              std::vector<int> &ret)
        {
                std::map<int, std::list<cageaddr> >::iterator it;
                uint160_t id1 = 1;
                int       n   = 0;

                for (it = m_table.begin(); it != m_table.end(); ++it) {
                        if (n >= max)
                                break;

                        uint160_t bits = id1 << it->first;
                        bits &= id;

                        if (! bits.is_zero()) {
                                n += it->second.size();
                                ret.push_back(it->first);
                        }
                }

                return n;
        }

        void
        rttable::print_table()
        {
                std::string str;
                std::map<int, std::list<cageaddr> >::iterator i;
                std::list<cageaddr>::iterator                 j;

                str = m_id.to_string();
                printf("MyID = %s\n", str.c_str());

                for (i = m_table.begin(); i != m_table.end(); ++i) {
                        std::list<cageaddr> &row = i->second;
                        printf("  i = %d\n", i->first);

                        for (j = row.begin(); j != row.end(); ++j) {
                                str = j->id->to_string();
                                printf("    ID = %s,\n", str.c_str());

                                if (j->domain == domain_inet) {
                                        in_ptr in;
                                        in = boost::get<in_ptr>(j->saddr);

                                        printf("    Port = %d\n", in->sin_port);
                                } else if (j->domain == domain_inet6) {
                                        in6_ptr in6;
                                        in6 = boost::get<in6_ptr>(j->saddr);

                                        printf("    Port = %d\n",
                                               in6->sin6_port);
                                }
                        }
                }
        }

#ifdef DEBUG
        void
        rttable::test_rttable()
        {
                uint160_t id;
                timer     t;
                peers     p(t);
                rttable   table(id, t, p);
                int       i;

                id = 0;

                // add
                for (i = 1; i < 21; i++) {
                        cageaddr addr;
                        id_ptr id_dst(new uint160_t);
                        in_ptr in_dst(new sockaddr_in);

                        *id_dst = i;

                        memset(in_dst.get(), 0, sizeof(sockaddr_in));
                        in_dst->sin_port = 10000 + i;
                        in_dst->sin_addr.s_addr = htonl(127 << 24 + 1);

                        addr.id     = id_dst;
                        addr.domain = domain_inet;
                        addr.saddr  = in_dst;

                        table.add(addr);
                }

                table.print_table();


                // lookup
                std::vector<cageaddr> nodes;
                uint160_t             dst = 0x0f;
                table.lookup(dst, 10, nodes);

                printf("\nlookup: Dst = %s\n", dst.to_string().c_str());

                std::vector<cageaddr>::iterator it;
                for (it = nodes.begin(); it != nodes.end(); ++it) {
                        std::string str;
                        uint160_t   dist;

                        str = it->id->to_string();
                        printf("  ID = %s,\n", str.c_str());

                        dist = *it->id ^ dst;
                        printf("Dist = %s\n\n", dist.to_string().c_str());
                }


                // remove
                table.remove(dst);
                table.print_table();
        }
#endif // DEBUG
}
