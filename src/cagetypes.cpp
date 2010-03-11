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

#include "cagetypes.hpp"

#include "peers.hpp"
#include "udphandler.hpp"

#include <boost/foreach.hpp>

namespace libcage {
        size_t
        hash_value(const _id &i)
        {
                return i.id->hash_value();
        }

        cageaddr
        new_cageaddr(msg_hdr *hdr, sockaddr *saddr)
        {
                cageaddr addr;
                id_ptr   id(new uint160_t);

                id->from_binary(hdr->src, sizeof(hdr->src));

                addr.id = id;

                if (saddr->sa_family == PF_INET) {
                        in_ptr in(new sockaddr_in);
                        memcpy(in.get(), saddr, sizeof(sockaddr_in));
                        addr.domain = domain_inet;
                        addr.saddr  = in;
                } else if (saddr->sa_family == PF_INET6) {
                        in6_ptr in6(new sockaddr_in6);
                        memcpy(in6.get(), saddr, sizeof(sockaddr_in6));
                        addr.domain = domain_inet6;
                        addr.saddr  = in6;
                }
                
                return addr;
        }

        void
        write_nodes_inet(msg_inet *min, std::vector<cageaddr> &nodes)
        {
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
        }

        void
        write_nodes_inet6(msg_inet6 *min6, std::vector<cageaddr> &nodes)
        {
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
        }

        void
        read_nodes_inet(msg_inet *min, int num, std::vector<cageaddr> &nodes,
                        sockaddr *from, peers &p)
        {
                for (int i = 0; i < num; i++) {
                        cageaddr caddr;
                        id_ptr   p_id(new uint160_t);
                        in_ptr   p_in(new sockaddr_in);

                        p_id->from_binary(min->id, sizeof(min->id));

                        if (p.is_timeout(p_id)) {
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
                        caddr.domain = domain_inet;
                        caddr.saddr  = p_in;

                        nodes.push_back(caddr);

                        min++;
                }
        }

        void
        read_nodes_inet6(msg_inet6 *min6, int num, std::vector<cageaddr> &nodes,
                         sockaddr *from, peers &p)
        {
                uint32_t   zero[4];

                memset(zero, 0, sizeof(zero));

                for (int i = 0; i < num; i++) {
                        cageaddr caddr;
                        id_ptr   p_id(new uint160_t);
                        in6_ptr  p_in6(new sockaddr_in6);

                        p_id->from_binary(min6->id, sizeof(min6->id));

                        if (p.is_timeout(p_id)) {
                                min6++;
                                continue;
                        }

                        if (min6->port == 0 &&
                            memcmp(min6->addr, zero, sizeof(zero)) == 0) {
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
                        caddr.domain = domain_inet6;
                        caddr.saddr  = p_in6;

                        nodes.push_back(caddr);
                        
                        min6++;
                }
        }

        void
        send_msg(udphandler &udp, msg_hdr *hdr, uint16_t len, uint8_t type,
                 cageaddr &dst, const uint160_t &src)
        {
                hdr->magic = htons(MAGIC_NUMBER);
                hdr->ver   = CAGE_VERSION;
                hdr->type  = type;
                hdr->len   = htons(len);

                dst.id->to_binary(hdr->dst, sizeof(hdr->dst));
                src.to_binary(hdr->src, sizeof(hdr->src));

                if (dst.domain == domain_inet) {
                        in_ptr in;
                        in = boost::get<in_ptr>(dst.saddr);
                        udp.sendto(hdr, len, (sockaddr*)in.get(),
                                   sizeof(sockaddr_in));
                } else if (dst.domain == domain_inet6) {
                        in6_ptr in6;
                        in6 = boost::get<in6_ptr>(dst.saddr);
                        udp.sendto(hdr, len, (sockaddr*)in6.get(),
                                   sizeof(sockaddr_in6));
                }
        }
}
