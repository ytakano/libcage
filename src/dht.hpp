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

#ifndef DHT_HPP
#define DHT_HPP

#include "common.hpp"

#include "bn.hpp"
#include "dtun.hpp"
#include "timer.hpp"
#include "peers.hpp"
#include "rttable.hpp"
#include "udphandler.hpp"

#include <string>
#include <vector>

#include <boost/function.hpp>
#include <boost/shared_array.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/variant.hpp>


namespace libcage {
        class dht : public rttable {
        private:
                static const int        num_find_node;
                static const int        max_query;
                static const int        query_timeout;


                typedef boost::function<void (std::vector<cageaddr>&)>
                callback_find_node;
                typedef boost::function<void (bool, void *buf, int len)>
                callback_find_value;
                typedef boost::variant<callback_find_node,
                                       callback_find_value> callback_func;

        public:
                dht(const uint160_t &id, timer &t, peers &p, udphandler &udp,
                    dtun &dt);
                virtual ~dht();

                void            recv_ping(void *msg, sockaddr *from,
                                          int fromlen);
                void            recv_ping_reply(void *msg, sockaddr *from,
                                                int fromlen);
                void            recv_find_node(void *msg, sockaddr *from);
                void            recv_find_node_reply(void *msg, int len,
                                                     sockaddr *from);
                void            recv_store(void *msg, int len, sockaddr *from);


                void            find_node(const uint160_t &dst,
                                          callback_find_node func);
                void            find_node(std::string host, int port,
                                          callback_find_node func);
                void            store(const uint160_t &id,
                                      char *key, uint16_t keylen,
                                      char *value, uint16_t valuelen,
                                      uint16_t ttl);

                void            use_dtun(bool flag);

        private:
                class _id {
                public:
                        id_ptr  id;

                        bool operator== (const _id &rhs) const
                        {
                                return *id == *rhs.id;
                        }
                };

                friend size_t hash_value(const _id &i);

                // for store
                class store_func {
                public:
                        void operator() (std::vector<cageaddr>& nodes);

                        boost::shared_array<char>       key;
                        boost::shared_array<char>       value;
                        uint16_t        keylen;
                        uint16_t        valuelen;
                        uint16_t        ttl;
                        id_ptr          id;
                        dht            *p_dht;
                };

                class id_key {
                public:
                        boost::shared_array<char>       key;
                        uint16_t        keylen;
                        id_ptr          id;

                        bool operator== (const id_key &rhs) const
                        {
                                if (keylen != keylen) {
                                        return false;
                                } else if (*id != *rhs.id) {
                                        return false;
                                } else if (memcmp(key.get(), rhs.key.get(),
                                                  keylen) != 0) {
                                        return false;
                                }

                                return true;
                        }
                };

                friend size_t hash_value(const id_key &ik);

                class stored_data {
                public:
                        boost::shared_array<char>       key;
                        boost::shared_array<char>       value;
                        boost::unordered_set<_id>       recvd;
                        uint16_t        keylen;
                        uint16_t        valuelen;
                        uint16_t        ttl;
                        time_t          stored_time;
                        id_ptr          id;
                };

                // for ping
                class ping_func {
                public:
                        void operator() (bool result, cageaddr &addr);

                        cageaddr        dst;
                        uint32_t        nonce;
                        dht            *p_dht;
                };

                // for find node or value
                class find_node_func {
                public:
                        void operator() (bool result, cageaddr &addr);

                        id_ptr          dst;
                        uint32_t        nonce;
                        dht            *p_dht;
                };

                class timer_query : public timer::callback {
                public:
                        virtual void operator() ();

                        _id             id;
                        uint32_t        nonce;
                        dht            *p_dht;
                };

                typedef boost::shared_ptr<timer_query>  timer_ptr;

                class query {
                public:
                        std::vector<cageaddr>           nodes;
                        boost::unordered_map<_id, timer_ptr>    timers;
                        boost::unordered_set<_id>       sent;
                        id_ptr          dst;
                        uint32_t        nonce;
                        int             num_query;
                        bool            is_find_value;

                        boost::shared_array<char>       key;
                        int             keylen;

                        callback_func   func;
                };

                typedef boost::shared_ptr<query> query_ptr;


                virtual void    send_ping(cageaddr &dst, uint32_t nonce);

                void            send_msg(msg_hdr *msg, uint16_t len,
                                         uint8_t type, cageaddr &dst);

                void            find_nv(const uint160_t &dst,
                                        callback_func func, bool is_find_value,
                                        char *key, int keylen);
                void            send_find(query_ptr q);
                void            send_find_node(cageaddr &dst, query_ptr q);


                const uint160_t         &m_id;
                timer          &m_timer;
                peers          &m_peers;
                udphandler     &m_udp;
                dtun           &m_dtun;
                bool            m_is_dtun;

                boost::unordered_map<uint32_t, query_ptr>       m_query;
                boost::unordered_map<id_key, stored_data>       m_stored;
        };
}

#endif // DHT_HPP
