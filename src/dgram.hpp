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

#ifndef DGRAM_HPP
#define DGRAM_HPP

#include "common.hpp"

#include "bn.hpp"
#include "cagetypes.hpp"
#include "dht.hpp"
#include "dtun.hpp"
#include "packetbuf.hpp"
#include "peers.hpp"
#include "rdp.hpp"
#include "udphandler.hpp"

#include <queue>

#include <boost/pool/object_pool.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

namespace libcage {
        class proxy;
        class advertise;

        class dgram {
        public:
                typedef boost::function<void (void *buf, size_t len,
                                              uint8_t *addr)> callback;


                dgram(const uint160_t &id, peers &p, udphandler &udp,
                      dtun &dt, dht &dh, proxy &pr, advertise &adv,
                      rdp &r);

                void            recv_dgram(packetbuf_ptr pbuf, sockaddr *from);

                void            send_dgram(packetbuf_ptr pbuf, id_ptr id,
                                           uint8_t type = type_dgram);
                void            send_dgram(const void *msg, int len, id_ptr id);
                void            send_dgram(const void *msg, int len, id_ptr id,
                                           const uint160_t &src);
                void            set_callback(callback func);

        private:
                class request_func {
                public:
                        void operator() (bool result, cageaddr &addr);

                        dgram  *p_dgram;
                        id_ptr  dst;
                };

                class find_node_func {
                public:
                        void operator() (std::vector<cageaddr> &nodes);

                        dgram  *p_dgram;
                        id_ptr  dst;
                };

                class send_data {
                public:
                        packetbuf_ptr   pbuf;
                        uint160_t       src;
                        int             len;
                        uint8_t         type;
                };

                void            send_queue(id_ptr id);
                void            push2queue(id_ptr id, const void *msg, int len,
                                           const uint160_t &src);
                void            push2queue(id_ptr id, packetbuf_ptr pbuf,
                                           const uint160_t &src,
                                           uint8_t type = type_dgram);

                void            request(id_ptr id);

                void            send_msg(send_data *data, cageaddr &dst);


                typedef boost::object_pool<send_data>   data_pool;
                typedef boost::shared_ptr<data_pool>    data_pool_ptr;
                typedef std::queue<send_data*>          type_queue;

                boost::unordered_map<_id, data_pool_ptr>        m_data_pool;
                boost::unordered_map<_id, type_queue>   m_queue;
                boost::unordered_set<_id>               m_requesting;
                const uint160_t        &m_id;
                peers                  &m_peers;
                udphandler             &m_udp;
                dtun                   &m_dtun;
                dht                    &m_dht;
                proxy                  &m_proxy;
                advertise              &m_advertise;
                rdp                    &m_rdp;
                callback                m_callback;
                bool                    m_is_callback;
        };
}

#endif // DGRAM_HPP
