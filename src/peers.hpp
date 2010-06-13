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

#ifndef PEERS_HPP
#define PEERS_HPP

#include "common.hpp"

#include "cagetypes.hpp"
#include "timer.hpp"

#include <vector>

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <boost/bimap/unordered_set_of.hpp> 
#include <boost/function.hpp>
#include <boost/random.hpp>
#include <boost/unordered_set.hpp> 

namespace libcage {
        class peers {
        private:
                static const time_t     timeout_ttl;
                static const time_t     map_ttl;
                static const time_t     timer_interval;


                typedef boost::function<void (cageaddr &addr)> callback;

                class timer_func : public timer::callback {
                public:
                        virtual void operator() ()
                        {
                                m_peers.refresh();

                                // reschedule
                                timeval tval;

                                tval.tv_sec = (long)((double)peers::timer_interval * m_peers.m_drnd());
                                tval.tv_sec += peers::timer_interval;

                                tval.tv_usec = 0;
                                m_peers.m_timer.set_timer(this, &tval);
                        }

                        timer_func(peers &p) : m_peers(p)
                        {
                                timeval tval;

                                tval.tv_sec = (long)((double)peers::timer_interval * m_peers.m_drnd());
                                tval.tv_sec += peers::timer_interval;

                                tval.tv_usec = 0;
                                m_peers.m_timer.set_timer(this, &tval);
                        }

                        ~timer_func()
                        {
                                m_peers.m_timer.unset_timer(this);
                        }

                        peers   &m_peers;
                };

                friend class    timer_func;

                class __id {
                public:
                        id_ptr          id;
                        mutable time_t  t;
                        uint32_t        session;

                        bool operator== (const __id &rhs) const
                        {
                                return *id == *rhs.id;
                        }
                };
                
                friend size_t hash_value(const __id &i);

                class _addr : private boost::totally_ordered<_addr> {
                public:
                        uint16_t        domain;
                        boost::variant<in_ptr, in6_ptr> saddr;

                        bool operator== (const _addr &rhs) const;
                        bool operator< (const _addr &rhs) const;
                };

                typedef boost::bimaps::multiset_of<_addr>       _addr_set;
                typedef boost::bimaps::unordered_set_of<__id>   __id_set;
                typedef boost::bimaps::bimap<__id_set,
                                             _addr_set>::value_type value_t;
                typedef boost::bimaps::bimap<__id_set, _addr_set>       _bimap;

        public:
                peers(rand_real &drnd, timer &t);

                // throws std::out_of_range
                cageaddr        get_addr(id_ptr id);
                cageaddr        get_first();
                cageaddr        get_next(id_ptr id);

                void            get_id(cageaddr &addr, std::vector<id_ptr> &id);

                void            remove_id(id_ptr id);
                void            remove_addr(cageaddr &addr);

                void            add_node(cageaddr &addr);
                bool            add_node(cageaddr &addr, uint32_t session);
                void            add_node_force(cageaddr &addr);

                void            add_timeout(id_ptr id);
                bool            is_timeout(id_ptr id);

                void            refresh();

                void            set_callback(callback func);

        private:
                rand_real      &m_drnd;

                _bimap          m_map;
                boost::unordered_set<__id>       m_timeout;

                timer           m_timer;
                timer_func      m_timer_func;
                bool            m_is_callback;
                callback        m_callback;

#ifdef DEBUG
        public:
                static void     test_peers();
#endif // DEBUG
        };
}

#endif // PEERS_HPP
