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

#ifndef DTUN_HPP
#define DTUN_HPP

#include "common.hpp"

#include "bn.hpp"
#include "natdetector.hpp"
#include "timer.hpp"
#include "udphandler.hpp"
#include "peers.hpp"
#include "rttable.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <boost/function.hpp>
#include <boost/variant.hpp>

namespace libcage {
        class proxy;

        class dtun : public rttable {
        private:
                static const int        num_find_node;
                static const int        max_query;
                static const int        query_timeout;
                static const int        request_retry;
                static const int        request_timeout;
                static const int        registered_ttl;
                static const int        timer_interval;
                static const int        maintain_interval;


                typedef boost::function<void (std::vector<cageaddr>&)>
                callback_find_node;
                typedef boost::function<void (bool, cageaddr&, cageaddr &)>
                callback_find_value;
                typedef boost::function<void (bool, cageaddr&)>
                callback_request;
                typedef boost::variant<callback_find_node,
                                       callback_find_value> callback_func;

        public:
                dtun(rand_uint &rnd, rand_real &drnd, const uint160_t &id,
                     timer &t, peers &p, const natdetector &nat,
                     udphandler &hdp, proxy &pr);
                virtual ~dtun();

                void            recv_ping(void *msg, sockaddr *from,
                                          int fromlen);
                void            recv_ping_reply(void *msg, sockaddr *from,
                                                int fromlen);
                void            recv_find_node(void *msg, sockaddr *from,
                                               int fromlen);
                void            recv_find_node_reply(void *msg, int len,
                                                     sockaddr *from);
                void            recv_find_value(void *msg, sockaddr *from,
                                                int fromlen);
                void            recv_find_value_reply(void *msg, int len,
                                                      sockaddr *from);
                void            recv_register(void *msg, sockaddr *from);
                void            recv_request(void *msg, sockaddr *from,
                                             int fromlen);
                void            recv_request_by(void *msg, int len,
                                                sockaddr *from);
                void            recv_request_reply(void *msg, sockaddr *from);


                void            find_node(const uint160_t &dst,
                                          callback_find_node func);
                void            find_node(sockaddr *saddr,
                                          callback_find_node func);
                void            find_node(std::string host, int port,
                                          callback_find_node func);

                void            find_value(const uint160_t &dst,
                                           callback_find_value func);

                void            register_node();
                void            register_node(id_ptr src, uint32_t session);

                void            request(const uint160_t &dst,
                                        callback_request func);

                void            refresh();

                void            set_enabled(bool enabled);
                bool            is_enabled() { return m_is_enabled; }

                uint32_t        get_session() { return m_register_session; }

        private:
                class timer_refresh : public timer::callback {
                public:
                        virtual void operator() ();

                        timer_refresh(dtun &d) : m_dtun(d), n(0)
                        {
                                timeval       tval;
                                tval.tv_sec  = (long)((double)dtun::timer_interval * m_dtun.m_drnd());
                                tval.tv_sec += dtun::timer_interval;
                                tval.tv_usec = 0;

                                m_dtun.m_timer.set_timer(this, &tval);
                        }

                        virtual ~timer_refresh()
                        {

                                m_dtun.m_timer.unset_timer(this);
                        }

                        dtun           &m_dtun;
                        int             n;
                };

                // for find node or value
                class timer_query : public timer::callback {
                public:
                        virtual void operator() ();

                        _id             id;
                        uint32_t        nonce;
                        dtun           *p_dtun;
                };

                typedef boost::shared_ptr<timer_query>  timer_ptr;

                class query {
                public:
                        std::vector<cageaddr>           nodes;
                        std::map<_id, timer_ptr>        timers;
                        std::set<_id>   sent;
                        uint160_t       dst;
                        uint32_t        nonce;
                        int             num_query;
                        bool            is_find_value;

                        callback_func   func;
                };

                typedef boost::shared_ptr<query> query_ptr;

                // register
                class register_callback {
                public:
                        void operator() (std::vector<cageaddr> &nodes);

                        uint32_t        session;
                        dtun   *p_dtun;
                        id_ptr  src;
                };

                class registered {
                public:
                        cageaddr        addr;
                        uint32_t        session;
                        time_t          t;

                        bool operator== (const registered &rhs) const;
                };

                // for request
                class request_find_value {
                public:
                        void operator() (bool result, cageaddr &addr,
                                         cageaddr &from);

                        uint32_t        nonce;
                        dtun           *p_dtun;
                };

                class timer_request : public timer::callback {
                public:
                        virtual void operator() ();

                        uint32_t        nonce;
                        dtun           *p_dtun;
                };

                class request_query {
                public:
                        callback_request        func;
                        timer_request   timer_req;
                        uint160_t       dst;
                        bool            finished_find_value;
                        dtun           *p_dtun;
                        int             retry;
                };

                typedef boost::shared_ptr<request_query> req_ptr;


                void            find_nv(const uint160_t &dst,
                                        callback_func func, bool is_find_value);


                virtual void    send_ping(cageaddr &dst, uint32_t nonce);
                void            send_find(query_ptr q);
                void            send_find_node(cageaddr &dst, query_ptr q);
                void            send_find_value(cageaddr &dst, query_ptr q);

                template<typename MSG>
                void            send_find_nv(uint16_t type, cageaddr &dst,
                                             query_ptr q);

                void            maintain();


                rand_uint              &m_rnd;
                rand_real              &m_drnd;

                const uint160_t        &m_id;
                timer                  &m_timer;
                peers                  &m_peers;
                const natdetector      &m_nat;
                udphandler             &m_udp;
                proxy                  &m_proxy;
                std::map<uint32_t, query_ptr>   m_query;
                bool                    m_registering;
                time_t                  m_last_registered;
                uint32_t                m_register_session;
                std::map<_id, registered>       m_registered_nodes;
                std::map<uint32_t, req_ptr>     m_request;
                timer_refresh           m_timer_refresh;
                bool                    m_is_enabled;
                int                     m_mask_bit;
                time_t                  m_last_maintain;
        };
}

#endif // DTUN_HPP
