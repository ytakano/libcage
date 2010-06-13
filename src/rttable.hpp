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

#ifndef RTTABLE_HPP
#define RTTABLE_HPP

#include "common.hpp"

#include <time.h>

#include <list>
#include <map>
#include <set>
#include <vector>

#include "bn.hpp"
#include "cagetypes.hpp"
#include "peers.hpp"
#include "timer.hpp"


namespace libcage {
        class rttable {
        public:
                rttable(rand_uint &rnd, const uint160_t &id, timer &t,
                        peers &p);
                virtual ~rttable();

                void            add(const cageaddr &addr);
                void            remove(const uint160_t &id);
                void            lookup(const uint160_t &id, int num, 
                                       std::vector<cageaddr> &ret);

                void            recv_ping_reply(cageaddr &src, uint32_t nonce);

                void            print_table() const;
                bool            is_zero();
                int             get_size();
                bool            has_id(uint160_t &id);

        protected:
                virtual void    send_ping(cageaddr &dst, uint32_t nonce);
                void            merge_nodes(const uint160_t &id,
                                            std::vector<cageaddr> &dst,
                                            const std::vector<cageaddr> &v1,
                                            const std::vector<cageaddr> &v2,
                                            int max);

        public:
                class compare {
                public:
                        const uint160_t        *m_id;

                        bool operator() (const cageaddr &lhs,
                                         const cageaddr &rhs) const
                        {
                                return (*m_id ^ *lhs.id) < (*m_id ^ *rhs.id);
                        }
                };


        private:
                static const int        max_entry;
                static const int        ping_timeout;


                class timer_ping : public timer::callback {
                public:
                        virtual void operator() ();

                        rttable        *m_rttable;
                        cageaddr        m_addr_old;
                        cageaddr        m_addr_new;
                        uint32_t        m_nonce;
                        int             m_i;
                };

                friend class    timer_ping;

                typedef boost::shared_ptr<timer_ping>   timer_ptr;

                std::map<int, std::list<cageaddr> >  m_table;
                std::map<uint32_t, timer_ptr>        m_ping_wait;
                std::set<int>           m_ping_send;
                std::set<uint160_t>     m_nodes;

                rand_uint              &m_rnd;
                const uint160_t        &m_id;
                timer                  &m_timer;
                peers                  &m_peers;

                int             id2i(const uint160_t &id);
                int             id2i4lookup(const uint160_t &id, int max,
                                            std::set<int> &ret);
                int             id2i4lookupR(const uint160_t &id, int max,
                                             std::set<int> &ret);

#ifdef DEBUG
        public:
                static void     test_rttable();
#endif // DEBUG
        };
}

#endif // RTTABLE_HPP
