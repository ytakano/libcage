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

#include "dgram.hpp"

#include "advertise.hpp"
#include "proxy.hpp"

namespace libcage {
        void
        dgram::request_func::operator() (bool result, cageaddr &addr)
        {
                _id i;

                if (result) {
                        p_dgram->send_queue(dst);
                } else {
                        i.id = dst;
                        p_dgram->m_queue.erase(i);
                        p_dgram->m_data_pool.erase(i);
                }

                i.id = dst;

                p_dgram->m_requesting.erase(i);
        }

        void
        dgram::find_node_func::operator() (std::vector<cageaddr> &nodes)
        {
                _id i;

                try {
                        // throw std::out_of_range
                        p_dgram->m_peers.get_addr(dst);
                        p_dgram->send_queue(dst);
                } catch (std::out_of_range) {
                        i.id = dst;
                        p_dgram->m_queue.erase(i);
                        p_dgram->m_data_pool.erase(i);
                }

                i.id = dst;

                p_dgram->m_requesting.erase(i);
        }

        void
        dgram::send_dgram(packetbuf_ptr pbuf, id_ptr id, uint8_t type)
        {
                push2queue(id, pbuf, m_id, type);

                request(id);
        }

        void
        dgram::send_dgram(const void *msg, int len, id_ptr id)
        {
                send_dgram(msg, len, id, m_id);
        }

        void
        dgram::send_dgram(const void *msg, int len, id_ptr id,
                          const uint160_t &src)
        {
                if (len < 0)
                        return;
                push2queue(id, msg, len, src);

                request(id);
        }

        void
        dgram::request(id_ptr id)
        {
                _id i;

                i.id = id;

                if (m_requesting.find(i) == m_requesting.end()) {
                        try {
                                m_peers.get_addr(id); // throw std::out_of_range
                                send_queue(id);
                        } catch (std::out_of_range) {
                                // request
                                if (m_dtun.is_enabled()) {
                                        request_func func;

                                        func.p_dgram = this;
                                        func.dst     = id;

                                        m_requesting.insert(i);
                                        m_dtun.request(*id, func);
                                } else {
                                        find_node_func func;

                                        func.p_dgram = this;
                                        func.dst     = id;

                                        m_requesting.insert(i);
                                        m_dht.find_node(*id, func);
                                }
                        }
                }
        }

        dgram::dgram(const uint160_t &id, peers &p, udphandler &udp,
                     dtun &dt, dht &dh, proxy &pr, advertise &adv,
                     rdp &r) :
                m_id(id),
                m_peers(p),
                m_udp(udp),
                m_dtun(dt),
                m_dht(dh),
                m_proxy(pr),
                m_advertise(adv),
                m_rdp(r),
                m_is_callback(false)
        {

        }

        void
        dgram::send_queue(id_ptr id)
        {
                try {
                        boost::unordered_map<_id, type_queue>::iterator it;
                        cageaddr   addr;
                        send_data *data;
                        _id        i;

                        i.id = id;

                        addr = m_peers.get_addr(id); // throw std::out_of_range

                        it = m_queue.find(i);
                        if (it == m_queue.end())
                                return;

                        while (! it->second.empty()) {
                                data = it->second.front();
                                send_msg(data, addr);
                                m_data_pool[i]->destroy(data);
                                it->second.pop();
                        }
                } catch (std::out_of_range) {
                        _id i;
 
                        i.id = id;
                        m_queue.erase(i);
                        m_data_pool.erase(i);
                }
        }

        void
        dgram::push2queue(id_ptr id, const void *msg, int len,
                          const uint160_t &src)
        {
                int total = 0;
                int dmax  = PBUF_SIZE - PBUF_DEFAULT_OFFSET;

                while (len > 0) {
                        packetbuf_ptr pbuf = packetbuf::construct();
                        int   plen = (len > dmax) ? dmax : len;
                        void *p;

                        p = pbuf->append(plen);
                        memcpy(p, (char*)msg + total, plen);

                        push2queue(id, pbuf, src);

                        len   -= plen;
                        total += plen;
                }
        }

        void
        dgram::push2queue(id_ptr id, packetbuf_ptr pbuf, const uint160_t &src,
                          uint8_t type)
        {
                boost::unordered_map<_id, data_pool_ptr>::iterator it;
                send_data *data;
                _id i;

                i.id = id;

                it = m_data_pool.find(i);
                if (it == m_data_pool.end()) {
                        m_data_pool[i] = data_pool_ptr(new data_pool);
                }

                data = m_data_pool[i]->construct();

                data->pbuf = pbuf;
                data->len  = pbuf->get_len();
                data->src  = src;
                data->type = type;

                m_queue[i].push(data);
        }

        void
        dgram::send_msg(send_data *data, cageaddr &dst)
        {
                msg_dgram *dgram;
                size_t     size;

                dgram = (msg_dgram*)data->pbuf->prepend(sizeof(dgram->hdr));

                if (dgram == NULL)
                        return;

                size = data->len + sizeof(msg_hdr);

                memset(dgram, 0 , sizeof(dgram->hdr));

                dgram->hdr.magic = htons(MAGIC_NUMBER);
                dgram->hdr.ver   = CAGE_VERSION;
                dgram->hdr.type  = data->type;
                dgram->hdr.len   = htons(size);
                
                data->src.to_binary(dgram->hdr.src, sizeof(dgram->hdr.src));
                dst.id->to_binary(dgram->hdr.dst, sizeof(dgram->hdr.dst));

                if (dst.domain == domain_inet) {
                        in_ptr in;
                        in = boost::get<in_ptr>(dst.saddr);
                        m_udp.sendto(dgram, size, (sockaddr*)in.get(),
                                     sizeof(sockaddr_in));
                } else if (dst.domain == domain_inet6) {
                        in6_ptr in6;
                        in6 = boost::get<in6_ptr>(dst.saddr);
                        m_udp.sendto(dgram, size, (sockaddr*)in6.get(),
                                     sizeof(sockaddr_in6));
                }

                data->pbuf->rm_head(sizeof(dgram->hdr));
        }

        void
        dgram::set_callback(dgram::callback func)
        {
                m_is_callback = true;
                m_callback = func;
        }

        void
        dgram::recv_dgram(packetbuf_ptr pbuf, sockaddr *from)
        {
                cageaddr   addr;
                msg_dgram *dgram;
                uint160_t  dst;
                int        size;

                dgram = (msg_dgram*)pbuf->get_data();

                size = ntohs(dgram->hdr.len);

                if (size != pbuf->get_len())
                        return;

                size -= sizeof(msg_hdr);

                dst.from_binary(dgram->hdr.dst, sizeof(dgram->hdr.dst));

                if (dst != m_id) {
                        // check proxy
                        m_proxy.forward_msg(dgram, size, from);
                        return;
                }

                addr = new_cageaddr(&dgram->hdr, from);

                m_peers.add_node(addr);

                if (m_dtun.is_enabled()) {
                        // send advertise
                        if (from->sa_family == PF_INET) {
                                sockaddr_in *in = (sockaddr_in*)from;

                                m_advertise.advertise_to(*addr.id, domain_inet,
                                                         in->sin_port,
                                                         &in->sin_addr.s_addr);
                        } else if (from->sa_family == PF_INET6) {
                                sockaddr_in6 *in6 = (sockaddr_in6*)from;

                                m_advertise.advertise_to(*addr.id, domain_inet6,
                                                         in6->sin6_port,
                                                         in6->sin6_addr.s6_addr);
                        }
                }


                if (dgram->hdr.type == type_dgram && m_is_callback) {
                        m_callback(dgram->data, size, (uint8_t*)dgram->hdr.src);
                } else if (dgram->hdr.type == type_rdp) {
                        pbuf->rm_head(sizeof(dgram->hdr));
                        m_rdp.input_dgram(addr.id, pbuf);
                }
        }
}
