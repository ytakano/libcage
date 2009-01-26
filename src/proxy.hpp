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

#ifndef PROXY_HPP
#define PROXY_HPP

#include "common.hpp"

#include "bn.hpp"
#include "dht.hpp"
#include "dtun.hpp"
#include "peers.hpp"
#include "timer.hpp"

#include <vector>

#include <boost/unordered_map.hpp>

namespace libcage {
        class proxy {
        private:
                static const time_t     register_timeout;
                static const time_t     get_timeout;

        public:
                typedef boost::function<void (bool, void *buf, int len)>
                callback_get;

                proxy(const uint160_t &id, udphandler &udp, timer &t,
                      peers &p, dtun &dt, dht &dh);
                virtual ~proxy();

                void            recv_register(void *msg, sockaddr *from);
                void            recv_register_reply(void *msg, sockaddr *from);
                void            recv_store(void *msg, int len, sockaddr *from);
                void            recv_get(void *msg, int len);
                void            recv_get_reply(void *msg, int len);

                void            register_node();
                void            store(const uint160_t &id,
                                      void *key, uint16_t keylen,
                                      void *value, uint16_t valuelen,
                                      uint16_t ttl);
                void            get(const uint160_t &id,
                                    void *key, uint16_t keylen,
                                    callback_get func);
                
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

                class _addr {
                public:
                        uint32_t        session;
                        cageaddr        addr;
                        time_t          recv_time;
                        time_t          last_registered;
                };

                class register_func {
                public:
                        void operator() (std::vector<cageaddr> &nodes);

                        proxy  *p_proxy;
                };

                class timer_register : public timer::callback {
                public:
                        virtual void operator() ();

                        timer_register(proxy &p) : m_proxy(p) { }

                        virtual ~timer_register()
                        {
                                m_proxy.m_timer.unset_timer(this);
                        }

                        proxy  &m_proxy;
                };

                class timer_get : public timer::callback {
                public:
                        virtual void operator() ();

                        proxy          *p_proxy;
                        uint32_t        nonce;
                        callback_get    func;
                };

                class getdata {
                public:
                        timer_get       timeout;
                        boost::shared_array<char>       key;
                        int             keylen;
                        callback_get    func;
                };

                typedef boost::shared_ptr<getdata> gd_ptr;

                class get_reply_func {
                public:
                        void operator() (bool result, void *buf, int len);

                        id_ptr          id;
                        id_ptr          src;
                        uint32_t        nonce;
                        proxy          *p_proxy;
                };

                const uint160_t        &m_id;
                udphandler     &m_udp;
                timer          &m_timer;
                peers          &m_peers;
                dtun           &m_dtun;
                dht            &m_dht;
                cageaddr        m_server;
                uint32_t        m_register_session;
                bool            m_is_registered;
                bool            m_is_registering;
                uint32_t        m_nonce;
                timer_register  m_timer_register;
                boost::unordered_map<_id, _addr>        m_registered;
                boost::unordered_map<uint32_t, gd_ptr>  m_getdata;
        };
}

#endif // PROXY_HPP
