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
#include "dgram.hpp"
#include "dht.hpp"
#include "dtun.hpp"
#include "natdetector.hpp"
#include "peers.hpp"
#include "timer.hpp"

#include <map>
#include <set>
#include <vector>

namespace libcage {
        class advertise;

        class proxy {
        private:
                static const time_t     register_timeout;
                static const time_t     register_ttl;
                static const time_t     get_timeout;
                static const time_t     timer_interval;

        public:
                typedef boost::function<void (bool, void *buf, int len)>
                callback_get;
                
                proxy(rand_uint &rnd, rand_real &drnd, const uint160_t &id,
                      udphandler &udp, timer &t, natdetector &nat, peers &p,
                      dtun &dt, dht &dh, dgram &dg, advertise &adv, rdp &r);
                virtual ~proxy();

                void            recv_register(void *msg, sockaddr *from);
                void            recv_register_reply(void *msg, sockaddr *from);
                void            recv_store(void *msg, int len,
                                           sockaddr *from);
                void            recv_get(void *msg, int len);
                void            recv_get_reply(void *msg, int len);
                void            recv_dgram(packetbuf_ptr pbuf);
                void            recv_forwarded(packetbuf_ptr pbuf);

                void            register_node();

                void            store(const uint160_t &id,
                                      const void *key, uint16_t keylen,
                                      const void *value, uint16_t valuelen,
                                      uint16_t ttl);
                void            get(const uint160_t &id,
                                    const void *key, uint16_t keylen,
                                    dht::callback_find_value func);

                void            send_dgram(const void *msg, int len, id_ptr id);
                void            send_dgram(packetbuf_ptr pbuf, id_ptr id,
                                           uint8_t type = type_proxy_dgram);

                void            forward_msg(msg_dgram *data, int size,
                                            sockaddr *from);

                bool            is_registered(id_ptr id);

                void            set_callback(dgram::callback func);

                void            refresh();
                
        private:
                class _addr {
                public:
                        uint32_t        session;
                        cageaddr        addr;
                        time_t          recv_time;
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
                        dht::callback_find_value    func;
                };

                class getdata {
                public:
                        timer_get       timeout;
                        boost::shared_array<char>       key;
                        int             keylen;
                        dht::callback_find_value    func;

                        dht::value_set_ptr  vset;
                        std::set<int>       indeces;
                        int                 total;

                        getdata() : vset(new dht::value_set), total(0) { }
                };

                typedef boost::shared_ptr<getdata> gd_ptr;

                class get_reply_func {
                public:
                        void operator() (bool result, dht::value_set_ptr vset);

                        id_ptr          id;
                        id_ptr          src;
                        uint32_t        nonce;
                        proxy          *p_proxy;
                };

                class timer_proxy : public timer::callback {
                public:
                        virtual void operator() ()
                        {
                                m_proxy.refresh();

                                if (m_proxy.m_nat.get_state() ==
                                    node_symmetric)
                                        m_proxy.register_node();

                                timeval tval;
                                time_t  t;

                                t  = (time_t)((double)proxy::timer_interval *
                                              m_proxy.m_drnd());
                                t += proxy::timer_interval;

                                tval.tv_sec  = proxy::timer_interval;
                                tval.tv_usec = 0;

                                m_proxy.m_timer.set_timer(this, &tval);
                        }

                        timer_proxy(proxy &pr) : m_proxy(pr)
                        {
                                timeval tval;
                                time_t  t;

                                t  = (time_t)((double)proxy::timer_interval *
                                              m_proxy.m_drnd());
                                t += proxy::timer_interval;

                                tval.tv_sec  = t;
                                tval.tv_usec = 0;

                                m_proxy.m_timer.set_timer(this, &tval);
                        }

                        virtual ~timer_proxy()
                        {
                                m_proxy.m_timer.unset_timer(this);
                        }

                        proxy  &m_proxy;
                };

                rand_uint      &m_rnd;
                rand_real      &m_drnd;

                const uint160_t        &m_id;
                udphandler     &m_udp;
                timer          &m_timer;
                natdetector    &m_nat;
                peers          &m_peers;
                dtun           &m_dtun;
                dht            &m_dht;
                dgram          &m_dgram;
                advertise      &m_advertise;
                rdp            &m_rdp;
                cageaddr        m_server;
                bool            m_is_registered;
                bool            m_is_registering;
                uint32_t        m_nonce;
                timer_register  m_timer_register;
                timer_proxy     m_timer_proxy;
                dgram::callback m_dgram_func;
                std::map<_id, _addr>            m_registered;
                std::map<uint32_t, gd_ptr>      m_getdata;
        };
}

#endif // PROXY_HPP
