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

#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/function.hpp>
#include <boost/random.hpp>
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
                static const int        slow_timer_interval;
                static const int        fast_timer_interval;
                static const int        original_put_num;
                static const int        recvd_value_timeout;
                static const uint16_t   rdp_store_port;
                static const uint16_t   rdp_get_port;
                static const time_t     rdp_timeout;

        public:
                class value_t {
                public:
                        boost::shared_array<char> value;
                        int len;

                public:
                        bool operator< (const value_t &rhs) const {
                                int n = (len < rhs.len) ? len : rhs.len;
                                int val = memcmp(value.get(),
                                                 rhs.value.get(), n);

                                if (val < 0)
                                        return true;
                                else if (val == 0 && len < rhs.len)
                                        return true;

                                return false;
                        }
                };

                typedef std::set<value_t>             value_set;
                typedef boost::shared_ptr<value_set>  value_set_ptr;


                typedef boost::function<void (std::vector<cageaddr>&)>
                callback_find_node;
                typedef boost::function<void (bool, value_set_ptr)> callback_find_value;
                typedef boost::variant<callback_find_node,
                                       callback_find_value> callback_func;

                dht(rand_uint &rnd, rand_real &drnd, const uint160_t &id,
                    timer &t, peers &p, const natdetector &nat, udphandler &udp,
                    dtun &dt, rdp &r);
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
                                      uint16_t ttl, bool is_unique);
                void            store(id_ptr id, boost::shared_array<char> key,
                                      uint16_t keylen,
                                      boost::shared_array<char> value,
                                      uint16_t valuelen, uint16_t ttl,
                                      id_ptr from, bool is_unique);


                void            set_enabled_dtun(bool flag);
                void            set_enabled_rdp(bool flag);
                bool            is_use_rdp() { return m_is_use_rdp; }

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
                        bool            is_unique;

                        rdp_recv_store(dht *d, id_ptr from) :
                                keylen(0), valuelen(0), key_read(0),
                                val_read(0), src(from), last_time(time(NULL)),
                                p_dht(d), is_hdr_read(false) { }

                        void store2local();
                };

                typedef boost::shared_ptr<rdp_recv_store> rdp_recv_store_ptr;

                class rdp_recv_store_func {
                public:
                        typedef std::map<int, rdp_recv_store_ptr>::iterator it_rcvs;
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
                        id_ptr          from;
                        bool            is_unique;
                        dht            *p_dht;

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
                        id_ptr          from;
                        bool            is_unique;
                        dht            *p_dht;
                };

                class _key {
                public:
                        boost::shared_array<char>       key;
                        uint16_t        keylen;

                        bool operator== (const _key &rhs) const
                        {
                                if (keylen != rhs.keylen) {
                                        return false;
                                } else if (memcmp(key.get(), rhs.key.get(),
                                                  keylen) != 0) {
                                        return false;
                                }

                                return true;
                        }
                };

                friend size_t hash_value(const _key &k);

                class stored_data {
                public:
                        boost::shared_array<char>       key;
                        boost::shared_array<char>       value;
                        uint16_t        keylen;
                        uint16_t        valuelen;
                        id_ptr          id;
                        id_ptr          src;
                        bool            is_unique;

                        mutable std::set<_id>   recvd;
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
                typedef boost::unordered_map<_key, sdata_set> sdata_map;

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
                        std::map<_id, timer_query_ptr>  timers;
                        std::set<_id>   sent;
                        id_ptr          dst;
                        uint32_t        nonce;
                        int             num_query;
                        bool            is_find_value;

                        boost::shared_array<char>       key;
                        int             keylen;
                        
                        struct val_info {
                                std::set<int> indeces;
                                value_set     values;
                                uint16_t      num_value;
                        };

                        std::map<_id, val_info>  valinfo;
                        value_set_ptr   vset;

                        // for RDP
                        enum query_state {
                                QUERY_HDR,
                                QUERY_VAL,
                        };
                        std::queue<id_ptr>      ids;
                        bool            is_rdp_con;
                        int             rdp_desc;
                        time_t          rdp_time;
                        query_state     rdp_state;
                        uint16_t        vallen;
                        uint16_t        val_read;
                        boost::shared_array<char>       val;

                        timer_recvd_ptr       timer_recvd;
                        bool                  is_timer_recvd_started;

                        callback_func   func;

                        query() : vset(new value_set),
                                  is_rdp_con(false),
                                  is_timer_recvd_started(false) { } 
                };

                typedef boost::shared_ptr<query> query_ptr;

                class timer_recvd_value : public timer::callback {
                public:
                        dht            &m_dht;
                        query_ptr       m_query;

                        timer_recvd_value(dht &d, query_ptr q)
                                : m_dht(d), m_query(q) { }

                        virtual void operator() ()
                        {
                                m_dht.recvd_value(m_query);
                        }
                };

                class rdp_get_func {
                public:
                        dht            &m_dht;
                        query_ptr       m_query;

                        rdp_get_func(dht &d, query_ptr q) : m_dht(d),
                                                            m_query(q) { }

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);

                        bool read_hdr(int desc);
                        bool read_val(int desc);
                        void close_rdp(int desc);
                };

                class rdp_recv_get {
                public:
                        enum rget_state {
                                RGET_HDR,
                                RGET_KEY,
                                RGET_VAL,
                                RGET_END,
                        };
                        dht            &m_dht;
                        time_t          m_time;
                        rget_state      m_state;
                        id_ptr          m_id;
                        uint16_t        m_keylen;
                        uint16_t        m_key_read;
                        boost::shared_array<char>      m_key;
                        std::queue<stored_data>        m_data;

                        rdp_recv_get(dht &d) : m_dht(d), m_time(time(NULL)),
                                               m_state(RGET_HDR),
                                               m_key_read(0) { }
                };

                typedef boost::shared_ptr<rdp_recv_get> rdp_recv_get_ptr;

                class rdp_recv_get_func {
                public:
                        dht            &m_dht;

                        rdp_recv_get_func(dht &d) : m_dht(d) { }

                        void operator() (int desc, rdp_addr addr,
                                         rdp_event event);
                        bool read_hdr(int desc, rdp_recv_get_ptr rget);
                        bool read_key(int desc, rdp_recv_get_ptr rget);
                        void read_val(rdp_recv_get_ptr rget);
                        void read_op(int desc, rdp_recv_get_ptr rget);

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
                        void
                        reschedule()
                        {
                                timeval tval;

                                tval.tv_sec  = (time_t)((double)m_sec *
                                                        m_dht.m_drnd());
                                tval.tv_sec += m_sec;
                                tval.tv_usec = 0;

                                m_dht.m_timer.set_timer(this, &tval);

                        }

                        timer_dht(dht &d, time_t sec) : m_dht(d), m_sec(sec)
                        {
                                reschedule();
                        }

                        virtual ~timer_dht()
                        {
                                m_dht.m_timer.unset_timer(this);
                        }

                        dht    &m_dht;
                        time_t  m_sec;
                };

                class fast_timer_dht : public timer_dht {
                public:
                        virtual void operator() ()
                        {
                                m_dht.refresh();
                                m_dht.sweep_rdp();

                                reschedule();
                        }

                        fast_timer_dht(dht &d) :
                                timer_dht(d, dht::fast_timer_interval),
                                m_dht(d) { }

                        dht    &m_dht;
                };

                class slow_timer_dht : public timer_dht {
                public:
                        virtual void operator() ()
                        {
                                m_dht.restore();
                                m_dht.maintain();

                                reschedule();
                        }

                        slow_timer_dht(dht &d) :
                                timer_dht(d, dht::slow_timer_interval),
                                m_dht(d) { }

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
                void            sweep_rdp();
                void            maintain();

                void            recvd_value(query_ptr q);
                void            remove_query(query_ptr q);

                void            add_sdata(stored_data &sdata, bool is_origin);
                void            erase_sdata(stored_data &sdata);
                void            insert2recvd_sdata(stored_data &sdata,
                                                   id_ptr id);
                int             dec_origin_sdata(stored_data &sdata);


                rand_uint               &m_rnd;
                rand_real               &m_drnd;
                const uint160_t         &m_id;
                timer                   &m_timer;
                peers                   &m_peers;
                const natdetector       &m_nat;
                udphandler              &m_udp;
                dtun                    &m_dtun;
                rdp                     &m_rdp;
                bool                     m_is_dtun;
                time_t                   m_last_restore;
                slow_timer_dht           m_slow_timer_dht;
                fast_timer_dht           m_fast_timer_dht;
                dht_join                 m_join;
                sync_node                m_sync;
                int                      m_rdp_recv_listen;
                int                      m_rdp_get_listen;
                bool                     m_is_use_rdp;
                int                      m_mask_bit;

                boost::unordered_map<_id, sdata_map>    m_stored;
                std::map<uint32_t, query_ptr>           m_query;
                std::map<int, rdp_recv_store_ptr>       m_rdp_recv_store;
                std::map<int, time_t>                   m_rdp_store;
                std::map<int, rdp_recv_get_ptr>         m_rdp_recv_get;
        };
}

#endif // DHT_HPP
