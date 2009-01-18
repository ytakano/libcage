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

namespace libcage {
        dtun::dtun(const uint160_t &id, timer &t, peers &p,
                   const natdetector &nat, udphandler &udp) :
                rttable(id, t, p), m_id(id), m_timer(t), m_peers(p), m_nat(nat),
                m_udp(udp)
        {

        }

        dtun::~dtun()
        {

        }

        void
        dtun::send_ping(cageaddr &dst, uint32_t nonce)
        {
                msg_dtun_ping ping;

                memset(&ping, 0, sizeof(ping));

                ping.hdr.magic = MAGIC_NUMBER;
                ping.hdr.ver   = CAGE_VERSION;
                ping.hdr.type  = type_dtun_ping;

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

                reply.hdr.magic = MAGIC_NUMBER;
                reply.hdr.ver   = CAGE_VERSION;
                reply.hdr.type  = type_dtun_ping_reply;

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
}
