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

#include "peers.hpp"

#include <boost/foreach.hpp>

namespace libcage {
        const time_t    peers::timeout_ttl    = 120;
        const time_t    peers::map_ttl        = 360;
        const time_t    peers::timer_interval = 120;

        size_t
        hash_value(const peers::_id &i)
        {
                return i.id->hash_value();
        }

        bool
        peers::_addr::operator== (const peers::_addr &rhs) const
        {
                if (domain != rhs.domain) {
                        return false;
                } else if (domain == domain_inet) {
                        in_ptr n1, n2;

                        n1 = boost::get<in_ptr>(saddr);
                        n2 = boost::get<in_ptr>(rhs.saddr);

                        return (n1->sin_port == n2->sin_port &&
                                n1->sin_addr.s_addr == n2->sin_addr.s_addr);
                } else if (domain == domain_inet6) {
                        in6_ptr n1, n2;

                        n1 = boost::get<in6_ptr>(saddr);
                        n2 = boost::get<in6_ptr>(rhs.saddr);

                        return (n1->sin6_port == n2->sin6_port &&
                                memcmp(&n1->sin6_addr, &n2->sin6_addr,
                                       sizeof(in6_addr)) == 0);
                }

                throw "must not reach";

                return false;
        }

        bool
        peers::_addr::operator< (const peers::_addr &rhs) const
        {
                if (domain != rhs.domain) {
                        return domain < rhs.domain;
                } else if (domain == domain_inet) {
                        in_ptr n1, n2;

                        n1 = boost::get<in_ptr>(saddr);
                        n2 = boost::get<in_ptr>(rhs.saddr);

                        if (n1->sin_addr.s_addr != n2->sin_addr.s_addr) {
                                return n1->sin_addr.s_addr <
                                        n2->sin_addr.s_addr;
                        } else {
                                return n1->sin_port < n2->sin_port;
                        }
                } else if (domain == domain_inet6) {
                        in6_ptr n1, n2;

                        n1 = boost::get<in6_ptr>(saddr);
                        n2 = boost::get<in6_ptr>(rhs.saddr);

                        int r;
                        r = memcmp(&n1->sin6_addr, &n2->sin6_addr,
                                   sizeof(in6_addr));

                        if (r < 0) {
                                return true;
                        } else if (r > 0) {
                                return false;
                        } else {
                                return n1->sin6_port < n2->sin6_port;
                        }
                }

                throw "must not reach";

                return false;
        }

        peers::peers(timer &t) : m_timer(t), m_timer_func(*this)
        {

        }

        cageaddr
        peers::get_addr(id_ptr id)
        {
                cageaddr addr;
                _addr    a;
                _id      i;

                i.id = id;

                // throws std::out_of_range
                a = m_map.left.at(i);

                addr.domain = a.domain;
                addr.saddr  = a.saddr;

                return addr;
        }

        void
        peers::get_id(cageaddr &addr, std::vector<id_ptr> &id)
        {
                // _bimap_its its;
                _addr      a;
                _id        i;

                a.domain = addr.domain;
                a.saddr  = addr.saddr;

                BOOST_FOREACH(_bimap::right_reference rp,
                              m_map.right.equal_range(a)) {
                        id.push_back(rp.second.id);
                }
        }

        void
        peers::remove_id(id_ptr id)
        {
                _id   i;

                i.id = id;

                m_map.left.erase(i);
        }

        void
        peers::remove_addr(cageaddr &addr)
        {
                _addr a;
                
                a.domain = addr.domain;
                a.saddr  = addr.saddr;

                m_map.right.erase(a);
        }

        void
        peers::add_node(cageaddr &addr)
        {
                add_node(addr, 0);
        }

        void
        peers::add_node(cageaddr &addr, uint32_t session)
        {
                _id   i;

                if (addr.domain != domain_inet && addr.domain == domain_inet6)
                        return;

                i.id      = addr.id;
                i.t       = time(NULL);
                i.session = session;

                _bimap::left_iterator it = m_map.left.find(i);

                if (it != m_map.left.end()) {
                        if (it->first.session == session ||
                            it->first.session == 0) {
                                _addr a;
                                a.domain = addr.domain;
                                a.saddr  = addr.saddr;
                                m_map.insert(value_t(i, a));
                                m_timeout.erase(i);
                        } else {
                                _addr a1, a2;
                                a1 = m_map.left.at(i);
                                a2.domain = addr.domain;
                                a2.saddr  = addr.saddr;

                                if (a1 == a2) {
                                        // update time
                                        it->first.t = i.t;
                                        m_timeout.erase(i);
                                }
                        }
                } else {
                        _addr a;
                        a.domain = addr.domain;
                        a.saddr  = addr.saddr;
                        m_map.insert(value_t(i, a));
                        m_timeout.erase(i);
                }
        }

        void
        peers::add_node_force(cageaddr &addr)
        {
                _addr a;
                _id   i;

                if (addr.domain != domain_inet && addr.domain != domain_inet6)
                        return;

                a.domain = addr.domain;
                a.saddr  = addr.saddr;

                i.id      = addr.id;
                i.t       = time(NULL);
                i.session = 0;

                m_map.insert(value_t(i, a));
        }

        void
        peers::refresh()
        {
                time_t now = time(NULL);

                boost::unordered_set<_id>::iterator it1;
                for (it1 = m_timeout.begin(); it1 != m_timeout.end();) {
                        time_t diff = now - it1->t;

                        if (diff > timeout_ttl) {
                                m_timeout.erase(it1++);
                        } else {
                                ++it1;
                        }
                }


                _bimap::left_iterator it2;
                for (it2 = m_map.left.begin(); it2 != m_map.left.end();) {
                        time_t diff = now - it2->first.t;

                        if (diff > map_ttl) {
                                m_map.left.erase(it2++);
                        } else {
                                ++it2;
                        }
                }
        }

        void
        peers::add_timeout(id_ptr id)
        {
                _id i;

                i.t  = time(NULL);
                i.id = id;

                m_timeout.insert(i);
        }

        bool
        peers::is_timeout(id_ptr id)
        {
                _id i;

                i.id = id;

                if (m_timeout.find(i) == m_timeout.end())
                        return false;

                return true;
        }

#ifdef DEBUG
        void
        peers::test_peers()
        {
                timer    t;
                peers    p(t);
                cageaddr addr;
                id_ptr   id1(new uint160_t);
                id_ptr   id2(new uint160_t);
                in_ptr   in1(new sockaddr_in);
                in_ptr   in2(new sockaddr_in);


                // add node
                *id1 = 100;
                memset(in1.get(), 0, sizeof(sockaddr_in));
                in1->sin_port = 10000;
                in1->sin_addr.s_addr = htonl((192UL << 24) + (168UL << 16) + 1);

                memset(in2.get(), 0, sizeof(sockaddr_in));
                in2->sin_port = 10000;
                in2->sin_addr.s_addr = htonl((192UL << 24) + (168UL << 16) + 1);

                addr.id     = id1;
                addr.domain = domain_inet;
                addr.saddr  = in1;

                p.add_node(addr, 0);

                *id2 = 200;
                addr.id     = id2;
                addr.saddr  = in2;

                p.add_node(addr, 0);
                printf("added: ID = 200\n");


                // get by ID
                try {
                        addr = p.get_addr(id1);
                        printf("get_addr: ID = 100\n");
                } catch (std::out_of_range) {
                        printf("failed get_addr: ID = 100\n");
                }

                try {
                        addr = p.get_addr(id2);
                        printf("get_addr: ID = 200\n");
                } catch (std::out_of_range) {
                        printf("failed get_addr: ID = 200\n");
                }


                // get by addr
                std::vector<id_ptr> ids;

                addr.domain = domain_inet;
                addr.saddr  = in1;

                p.get_id(addr, ids);

                std::vector<id_ptr>::iterator it;
                for (it = ids.begin(); it != ids.end(); ++it) {
                        uint32_t n = (uint32_t)**it;
                        printf("get_id: ID = %d\n", n);
                }


                // remove id
                p.remove_id(id1);
                printf("remove: ID = 100\n");

                try {
                        addr = p.get_addr(id1);
                        printf("get_addr: ID = 100\n");
                } catch (std::out_of_range) {
                        printf("failed get_addr: ID = 100\n");
                }

                ids.clear();
                p.get_id(addr, ids);
                for (it = ids.begin(); it != ids.end(); ++it) {
                        uint32_t n = (uint32_t)**it;
                        printf("get_id: ID = %d\n", n);
                }


                // add again
                addr.id     = id1;
                addr.domain = domain_inet;
                addr.saddr  = in1;

                p.add_node(addr, 0);
                printf("added: ID = 100\n");


                // remove addr
                p.remove_addr(addr);
                printf("remove_addr\n");

                ids.clear();
                p.get_id(addr, ids);
                for (it = ids.begin(); it != ids.end(); ++it) {
                        uint32_t n = (uint32_t)**it;
                        printf("get_id: ID = %d\n", n);
                }
        }
#endif // DEBUG
}
