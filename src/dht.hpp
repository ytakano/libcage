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
#include "rdp.hpp"
#include "udphandler.hpp"

#include <set>
#include <string>
#include <vector>

#include <boost/function.hpp>
#include <boost/shared_array.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>
#include <boost/variant.hpp>


namespace libcage {
        class proxy;

        class dht : public rttable {
        private:
                static const int        num_find_node;
                static const int        max_query;
                static const int        query_timeout;
                static const int        restore_interval;
                static const int        timer_interval;
                static const int        original_put_num;
                static const int        recvd_value_timeout;
                static const uint16_t   rdp_store_port;

        public:
                class value_t {
                public:
                        boost::shared_array<char> value;
                        int len;

                public:
                        bool operator== (const value_t &rhs) const {
                                if (len != rhs.len) {
                                        return false;
                                } else if (memcmp(value.get(), rhs.value.get(),
                                                  len) != 0) {
                                        return false;
                                }

                                return true;
                        }
                };

                friend size_t hash_value(const value_t &value);

                typedef boost::unordered_set<value_t> value_set;
                typedef boost::shared_ptr<value_set>  value_set_ptr;


                typedef boost::function<void (std::vector<cageaddr>&)>
                callback_find_node;
                typedef boost::function<void (bool, value_set_ptr)> callback_find_value;
                typedef boost::variant<callback_find_node,
                                       callback_find_value> callback_func;

                dht(const uint160_t &id, timer &t, peers &p,
                    const natdetector &nat, udphandler &udp, dtun &dt,
                    rdp &r);
                virtual ~dht();

                void            recv_ping(void *msg, sockaddr *from,
                                          int fromlen);
                void            recv_ping_reply(void *msg, sockaddr *from,
                                                int fromlen);
                void            recv_find_node(void *msg, sockaddr *from);
                void            recv_find_node_reply(void *msg, int len,
                                                     sockaddr *from);
                void            recv_find_value(void *msg, int len,
                                                sockaddr *from);
                void            recv_find_value_reply(void *msg, int len,
                                                      sockaddr *from);
                void            recv_store(void *msg, int len, sockaddr *from);


                void            find_node(const uint160_t &dst,
                                          callback_find_node func);
                void            find_node(std::string host, int port,
                                          callback_find_node func);
                void            find_node(sockaddr *saddr,
                                          callback_find_node func);
                void            find_value(const uint160_t &dst,
                                           const void *key, uint16_t keylen,
                                           callback_find_value func);
                void            store(const uint160_t &id,
                                      const void *key, uint16_t keylen,
                                      const void *value, uint16_t valuelen,
                                      uint16_t ttl);

                void            set_enabled_dtun(bool flag);
                void            set_enabled_rdp(bool flag);

        private:
                class rdp_recv_store {
                public:
                        boost::shared_array<char>       key;
                        boost::shared_array<char>       value;
                        uint16_t        keylen;
                        uint16_t        valuelen;
                        uint16_t        key_read;
                        uint16_t        val_read;
                        uint16_t        ttl;
                        id_ptr          id;
                        id_ptr          src;
                        time_t          last_time;
                        dht            *p_dht;
                        bool            is_hdr_read;

                        rdp_recv_store(dht *d, id_ptr from) :
                                keylen(0), valuelen(0), key_read(0),
                                val_read(0), src(from), last_time(time(NULL)),
                                p_dht(d), is_hdr_read(false) { }

                        void store2local();
                };

                typedef boost::shared_ptr<rdp_recv_store> rdp_recv_store_ptr;

                class rdp_recv_store_func {
                public:
                        typedef boost::unordered_map<int, rdp_recv_store_ptr>::iterator it_rcvs;
                        dht &m_dht;

                        rdp_recv_store_func(dht &d) : m_dht(d) { }

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);
                        
                        bool read_hdr(int desc, it_rcvs it);
                        bool read_body(int desc, it_rcvs it);
                };

                class rdp_store_func {
                public:
                        boost::shared_array<char>       key;
                        boost::shared_array<char>       value;
                        uint16_t        keylen;
                        uint16_t        valuelen;
                        uint16_t        ttl;
                        id_ptr          id;
                        dht            *p_dht;

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);
                };

                class rdp_recv_get_func {
                public:
                        dht            *p_dht;

                        rdp_recv_get_func(dht *d) : p_dht(d) { }

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);
                };

                class sync_node {
                public:
                        sync_node(dht &d) : m_dht(d)
                        {
                                m_dht.m_peers.set_callback(*this);
                        }

                        void operator() (cageaddr &addr);

                        dht    &m_dht;
                };

                // for store
                class store_func {
                public:
                        void operator() (std::vector<cageaddr>& nodes);
                        bool store_by_udp(std::vector<cageaddr>& nodes);
                        bool store_by_rdp(std::vector<cageaddr>& nodes);

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
                                if (keylen != rhs.keylen) {
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
                        uint16_t        keylen;
                        uint16_t        valuelen;
                        id_ptr          id;

                        mutable boost::unordered_set<_id>       recvd;
                        mutable time_t          stored_time;
                        mutable int             original;
                        mutable uint16_t        ttl;


                        bool operator== (const stored_data &rhs) const
                        {
                                if (valuelen != rhs.valuelen) {
                                        return false;
                                } else if (memcmp(value.get(), rhs.value.get(),
                                                  valuelen) != 0) {
                                        return false;
                                }

                                return true;
                        }
                };

                friend size_t hash_value(const stored_data &sdata);

                typedef boost::unordered_set<stored_data> sdata_set;

                // for ping
                class ping_func {
                public:
                        void operator() (bool result, cageaddr &addr);

                        cageaddr        dst;
                        uint32_t        nonce;
                        dht            *p_dht;
                };

                // for find node or value
                class find_value_func {
                public:
                        void operator() (bool result, cageaddr &addr);

                        boost::shared_array<char>       key;
                        uint16_t        keylen;
                        id_ptr          dst;
                        uint32_t        nonce;
                        dht            *p_dht;
                };

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

                typedef boost::shared_ptr<timer_query>  timer_query_ptr;


                class timer_recvd_value;
                typedef boost::shared_ptr<timer_recvd_value> timer_recvd_ptr;

                class query {
                public:
                        std::vector<cageaddr>           nodes;
                        boost::unordered_map<_id, timer_query_ptr>      timers;
                        boost::unordered_set<_id>       sent;
                        id_ptr          dst;
                        uint32_t        nonce;
                        int             num_query;
                        bool            is_find_value;

                        boost::shared_array<char>       key;
                        int             keylen;

                        boost::unordered_map<_id, value_set> values;
                        boost::unordered_map<_id, int>       num_value;
                        boost::unordered_map<_id, std::set<int> >       indeces;
                        value_set_ptr   vset;

                        timer_recvd_ptr       timer_recvd;
                        bool                  is_timer_recvd_started;

                        callback_func   func;

                        query() : vset(new value_set),
                                  is_timer_recvd_started(false){ }
                };

                typedef boost::shared_ptr<query> query_ptr;


                class timer_recvd_value : public timer::callback {
                public:
                        dht            &m_dht;
                        query_ptr       m_query;

                        timer_recvd_value(dht &d, query_ptr q)
                                : m_dht(d), m_query(q) { }
                        virtual ~timer_recvd_value() { }

                        virtual void operator() ()
                        {
                                m_dht.recvd_value(m_query);
                        }
                };

                // for restore
                class restore_func {
                public:
                        void operator() (std::vector<cageaddr> &n);

                        bool restore_by_udp(std::vector<cageaddr> &nodes,
                                            sdata_set::iterator &it);
                        bool restore_by_rdp(std::vector<cageaddr> &nodes,
                                            sdata_set::iterator &it);

                        dht    *p_dht;
                };

                class timer_dht : public timer::callback {
                public:
                        virtual void operator() ()
                        {
                                m_dht.refresh();
                                m_dht.restore();

                                // reschedule
                                timeval tval;

                                tval.tv_sec  = (long)((double)dht::timer_interval * drand48());
                                tval.tv_sec += dht::restore_interval;

                                tval.tv_usec = 0;

                                m_dht.m_timer.set_timer(this, &tval);
                        }

                        timer_dht(dht &d) : m_dht(d)
                        {
                                timeval tval;

                                tval.tv_sec  = (long)((double)dht::timer_interval * drand48());
                                tval.tv_sec += dht::restore_interval;

                                tval.tv_usec = 0;

                                m_dht.m_timer.set_timer(this, &tval);
                        }

                        virtual ~timer_dht()
                        {
                                m_dht.m_timer.unset_timer(this);
                        }

                        dht    &m_dht;
                };

                // for join
                class dht_join : public timer::callback {
                public:
                        virtual void operator() ();

                        dht    &m_dht;
                        time_t  m_interval;

                        dht_join(dht &d) : m_dht(d), m_interval(3)
                        {
                                timeval tval;

                                tval.tv_sec  = m_interval;
                                tval.tv_usec = 0;

                                m_dht.m_timer.set_timer(this, &tval);
                        }

                        virtual ~dht_join()
                        {
                                m_dht.m_timer.unset_timer(this);
                        }
                };


                virtual void    send_ping(cageaddr &dst, uint32_t nonce);

                void            find_nv(const uint160_t &dst,
                                        callback_func func, bool is_find_value,
                                        const void *key, int keylen);
                void            send_find(query_ptr q);
                void            send_find_node(cageaddr &dst, query_ptr q);
                void            send_find_value(cageaddr &dst, query_ptr q);

                void            refresh();
                void            restore();

                void            recvd_value(query_ptr q);


                const uint160_t         &m_id;
                timer                   &m_timer;
                peers                   &m_peers;
                const natdetector       &m_nat;
                udphandler              &m_udp;
                dtun                    &m_dtun;
                rdp                     &m_rdp;
                bool                     m_is_dtun;
                time_t                   m_last_restore;
                timer_dht                m_timer_dht;
                dht_join                 m_join;
                sync_node                m_sync;
                int                      m_rdp_listen;
                bool                     m_is_use_rdp;

                boost::unordered_map<uint32_t, query_ptr>     m_query;
                boost::unordered_map<id_key, sdata_set>       m_stored;
                boost::unordered_map<int, rdp_recv_store_ptr> m_rdp_recv_store;
                boost::unordered_map<int, time_t>             m_rdp_store;
        };
}

#endif // DHT_HPP
