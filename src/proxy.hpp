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
                static const time_t     rdp_timeout;
                static const uint16_t   proxy_store_port;
                static const uint16_t   proxy_get_port;
                static const uint16_t   proxy_get_reply_port;

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
                                      uint16_t ttl, bool is_unique);
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
                class rdp_store_func {
                public:
                        proxy  &m_proxy;
                        boost::shared_array<char>       m_key;
                        boost::shared_array<char>       m_val;
                        uint16_t        m_keylen;
                        uint16_t        m_valuelen;
                        uint16_t        m_ttl;
                        bool            m_is_unique;
                        id_ptr          m_id;

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);

                        rdp_store_func(proxy &p) : m_proxy(p) { }
                };

                typedef boost::shared_ptr<rdp_store_func> rdp_store_func_ptr;

                class rdp_recv_store {
                public:
                        enum recv_store_state {
                                RS_HDR,
                                RS_KEY,
                                RS_VAL,
                        };

                        recv_store_state                m_state;
                        boost::shared_array<char>       m_key;
                        boost::shared_array<char>       m_val;
                        uint16_t        m_keylen;
                        uint16_t        m_key_read;
                        uint16_t        m_valuelen;
                        uint16_t        m_val_read;
                        uint16_t        m_ttl;
                        bool            m_is_unique;
                        id_ptr          m_id;
                        id_ptr          m_src;
                        time_t          m_time;

                        rdp_recv_store() : m_state(RS_HDR), m_key_read(0),
                                           m_val_read(0) { }
                };

                typedef boost::shared_ptr<rdp_recv_store> rdp_recv_store_ptr;

                class rdp_recv_store_func {
                public:
                        proxy  &m_proxy;

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);

                        rdp_recv_store_func(proxy &p) : m_proxy(p) { }

                        bool read_hdr(int desc, rdp_recv_store_ptr ptr);
                        bool read_key(int desc, rdp_recv_store_ptr ptr);
                        void read_val(int desc, rdp_recv_store_ptr ptr);
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

                        // for rdp
                        int     desc;
                        bool    is_rdp;

                        getdata() : vset(new dht::value_set), total(0),
                                    is_rdp(false) { }
                };

                typedef boost::shared_ptr<getdata> gd_ptr;

                class rdp_get {
                public:
                        id_ptr  m_id;
                        dht::callback_find_value        m_func;
                        boost::shared_array<char>       m_key;
                        uint16_t        m_keylen;
                        time_t          m_time;
                        gd_ptr          m_data;
                        uint32_t        m_nonce;
                };

                typedef boost::shared_ptr<rdp_get> rdp_get_ptr;

                class rdp_get_func {
                public:
                        proxy  &m_proxy;

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);

                        rdp_get_func(proxy &p) : m_proxy(p) { }
                };

                class rdp_recv_get {
                public:
                        enum recv_get_state {
                                RG_HDR,
                                RG_KEY,
                        };

                        proxy  &m_proxy;
                        id_ptr  m_id;
                        id_ptr  m_src;
                        boost::shared_array<char>       m_key;
                        uint16_t        m_keylen;
                        uint16_t        m_key_read;
                        uint32_t        m_nonce;
                        time_t          m_time;
                        recv_get_state  m_state;

                        rdp_recv_get(proxy &p) : m_proxy(p), m_key_read(0),
                                                 m_time(time(NULL)),
                                                 m_state(RG_HDR) { }
                };

                typedef boost::shared_ptr<rdp_recv_get> rdp_recv_get_ptr;

                class rdp_recv_get_func {
                public:
                        proxy  &m_proxy;

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);

                        rdp_recv_get_func(proxy &p) : m_proxy(p) { }

                        bool read_hdr(int desc, rdp_recv_get_ptr ptr);
                        void read_key(int desc, rdp_recv_get_ptr ptr);
                };

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

                class rdp_get_reply_func {
                public:
                        proxy  &m_proxy;
                        bool    m_result;
                        dht::value_set_ptr      m_vset;
                        uint32_t        m_nonce;
                        id_ptr          m_id;

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);

                        rdp_get_reply_func(proxy &p) : m_proxy(p) { }

                        void read_op(int desc);
                };

                class get_reply_func {
                public:
                        void operator() (bool result, dht::value_set_ptr vset);

                        void send_reply_by_rdp(bool result,
                                               dht::value_set_ptr vset);
                        void send_reply(bool result, dht::value_set_ptr vset);

                        id_ptr          id;
                        id_ptr          src;
                        uint32_t        nonce;
                        proxy          *p_proxy;
                        bool            is_rdp;
                };

                class rdp_recv_get_reply {
                public:
                        enum recv_get_reply_state {
                                RGR_HDR,
                                RGR_VAL_HDR,
                                RGR_VAL,
                        };

                        boost::shared_array<char>       m_val;
                        uint16_t        m_valuelen;
                        uint16_t        m_val_read;

                        recv_get_reply_state    m_state;
                        uint32_t        m_nonce;
                        time_t          m_time;

                        rdp_recv_get_reply() : m_state(RGR_HDR),
                                               m_time(time(NULL)) { }
                };

                typedef boost::shared_ptr<rdp_recv_get_reply> rdp_recv_get_reply_ptr;

                class rdp_recv_get_reply_func {
                public:
                        proxy  &m_proxy;

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);

                        rdp_recv_get_reply_func(proxy &p) : m_proxy(p) { }

                        bool read_hdr(int desc, rdp_recv_get_reply_ptr ptr);
                        bool read_val_hdr(int desc, rdp_recv_get_reply_ptr ptr);
                        bool read_val(int desc, rdp_recv_get_reply_ptr ptr);

                        void close_rdp(int desc, rdp_recv_get_reply_ptr ptr);
                };

                class timer_proxy : public timer::callback {
                public:
                        virtual void operator() ()
                        {
                                m_proxy.refresh();

                                if (m_proxy.m_nat.get_state() ==
                                    node_symmetric) {
                                        m_proxy.register_node();
                                        m_proxy.sweep_rdp();
                                        m_proxy.retry_storing();
                                }

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

                void            store_by_rdp(const uint160_t &id,
                                             const void *key, uint16_t keylen,
                                             const void *value,
                                             uint16_t valuelen,
                                             uint16_t ttl,
                                             bool is_unique);
                void            get_by_rdp(const uint160_t &id,
                                           const void *key, uint16_t keylen,
                                           dht::callback_find_value func);
                void            sweep_rdp();
                void            create_store_func(rdp_store_func &func,
                                                  const uint160_t &id,
                                                  const void *key,
                                                  uint16_t keylen,
                                                  const void *value,
                                                  uint16_t valuelen,
                                                  uint16_t ttl,
                                                  bool is_unique);
                void            retry_storing();

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
                std::vector<rdp_store_func_ptr> m_store_data;

                int             m_rdp_store_desc;
                int             m_rdp_get_desc;
                int             m_rdp_get_reply_desc;
                std::map<int, time_t>           m_rdp_store;
                std::map<int, rdp_get_ptr>      m_rdp_get;
                std::map<int, rdp_recv_store_ptr>       m_rdp_recv_store;
                std::map<int, rdp_recv_get_ptr>         m_rdp_recv_get;
                std::map<int, time_t>           m_rdp_get_reply;
                std::map<int, rdp_recv_get_reply_ptr>   m_rdp_recv_get_reply;
        };
}

#endif // PROXY_HPP
