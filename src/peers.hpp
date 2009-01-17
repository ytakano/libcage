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

#include <vector>

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>

namespace libcage {
        class peers {
        private:
                class _id : private boost::totally_ordered<_id> {
                public:
                        id_ptr id;
                        time_t t;

                        bool operator== (const _id &rhs) const
                        {
                                return *id == *rhs.id;
                        }

                        bool operator< (const _id &rhs) const
                        {
                                return *id < *rhs.id;
                        }
                };

                class _addr : private boost::totally_ordered<_addr> {
                public:
                        sa_family_t     domain;
                        boost::variant<in_ptr, in6_ptr> saddr;

                        bool operator== (const _addr &rhs) const;
                        bool operator< (const _addr &rhs) const;
                };

                typedef boost::bimaps::multiset_of<_addr> _addr_set;
                typedef boost::bimaps::bimap<_id,
                                             _addr_set>::value_type value_t;
                typedef boost::bimaps::bimap<_id, _addr_set> _bimap;

        public:
                peers();

                // throws std::out_of_range
                cageaddr        get_addr(id_ptr id);
                void            get_id(cageaddr &addr, std::vector<id_ptr> &id);

                void            remove_id(id_ptr id);
                void            remove_addr(cageaddr &addr);

                void            add_node(cageaddr &addr);
                void            add_node_force(cageaddr &addr);

        private:
                _bimap          m_map;


#ifdef DEBUG
        public:
                static void     test_peers();
#endif // DEBUG
        };
}

#endif // PEERS_HPP
