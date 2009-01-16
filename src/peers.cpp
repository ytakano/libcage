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

namespace libcage {
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

        peers::peers()
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

        id_ptr
        peers::get_id(cageaddr &addr)
        {
                _addr a;
                _id   i;

                a.domain = addr.domain;
                a.saddr  = addr.saddr;

                // throws std::out_of_range
                i = m_map.right.at(a);

                return i.id;
        }

        void
        peers::remove_id(id_ptr id)
        {
                _id i;

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
        peers::add_addr(cageaddr &addr)
        {
                _addr a;
                _id   i;

                a.domain = addr.domain;
                a.saddr  = addr.saddr;

                i.id = addr.id;
                i.t  = time(NULL);

                m_map.insert(value_t(i, a));
        }
}
