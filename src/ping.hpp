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

#ifndef PING_HPP
#define PING_HPP

#include "common.hpp"

#include "bn.hpp"
#include "cagetypes.hpp"
#include "udphandler.hpp"

namespace libcage {
        template <typename MSG>
        void
        send_ping_tmpl(cageaddr &dst, uint32_t nonce, uint8_t type,
                       const uint160_t &id, udphandler &udp)
        {
                MSG ping;

                memset(&ping, 0, sizeof(ping));

                ping.hdr.magic = htons(MAGIC_NUMBER);
                ping.hdr.ver   = CAGE_VERSION;
                ping.hdr.type  = type;
                ping.hdr.len   = htons(sizeof(ping));

                id.to_binary(ping.hdr.src, sizeof(ping.hdr.src));
                dst.id->to_binary(ping.hdr.dst, sizeof(ping.hdr.dst));

                ping.nonce = htonl(nonce);

                if (dst.domain == domain_inet) {
                        in_ptr in;
                        in = boost::get<in_ptr>(dst.saddr);
                        udp.sendto(&ping, sizeof(ping), (sockaddr*)in.get(),
                                   sizeof(sockaddr_in));
                } else {
                        in6_ptr in6;
                        in6 = boost::get<in6_ptr>(dst.saddr);
                        udp.sendto(&ping, sizeof(ping), (sockaddr*)in6.get(),
                                   sizeof(sockaddr_in6));
                }
        };

        template <typename MSG, typename MSG_REPLY>
        void
        recv_ping_tmpl(void *msg, sockaddr *from, int fromlen, uint8_t type,
                       const uint160_t &id, udphandler &udp, peers &p)
        {
                MSG       *ping = (MSG*)msg;
                MSG_REPLY  reply;
                uint160_t  dst;

                dst.from_binary(ping->hdr.dst, sizeof(ping->hdr.dst));
                if (dst != id)
                        return;


                // send ping reply
                memset(&reply, 0, sizeof(reply));

                reply.hdr.magic = htons(MAGIC_NUMBER);
                reply.hdr.ver   = CAGE_VERSION;
                reply.hdr.type  = type;
                reply.hdr.len   = htons(sizeof(reply));

                id.to_binary(reply.hdr.src, sizeof(reply.hdr.src));
                memcpy(reply.hdr.dst, ping->hdr.src, sizeof(reply.hdr.dst));

                reply.nonce = ping->nonce;

                udp.sendto(&reply, sizeof(reply), from, fromlen);


                // add to peers
                cageaddr addr = new_cageaddr(&ping->hdr, from);
                p.add_node(addr);
        }

        template <typename MSG>
        void
        recv_ping_reply_tmpl(void *msg, sockaddr *from, int fromlen,
                             const uint160_t &id, peers &p, rttable *rt)
        {
                MSG       *reply = (MSG*)msg;
                uint160_t  dst;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (dst != id)
                        return;

                cageaddr addr = new_cageaddr(&reply->hdr, from);

                rt->recv_ping_reply(addr, ntohl(reply->nonce));


                // add to peers
                p.add_node(addr);
        }
}

#endif // PING_HPP
