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

#ifndef CAGE_HPP
#define CAGE_HPP

#include "common.hpp"

#include "bn.hpp"
#include "dgram.hpp"
#include "dtun.hpp"
#include "dht.hpp"
#include "natdetector.hpp"
#include "peers.hpp"
#include "proxy.hpp"
#include "timer.hpp"
#include "udphandler.hpp"

namespace libcage {
        class cage {
        public:
                cage();
                virtual ~cage();

                typedef boost::function<void (bool, void *buf, int len)>
                callback_get;
                typedef boost::function<void (bool)>
                callback_join;

                bool            open(int domain, uint16_t port,
                                     bool is_dtun = true);
                void            put(void *key, uint16_t keylen,
                                    void *value, uint16_t valuelen,
                                    uint16_t ttl);
                void            get(void *key, uint16_t keylen,
                                    callback_get func);
                void            join(std::string host, int port,
                                     callback_join func);

                void            send_dgram(const void *buf, int len,
                                           uint8_t *dst);
                void            set_dgram_callback(dgram::callback func);
                void            unset_dgram_callback();

                void            set_global() { m_nat.set_state_global(); }

                void            print_state();


        private:
                class udp_receiver : public udphandler::callback {
                public:
                        virtual void operator() (udphandler &udp, void *buf,
                                                 int len, sockaddr *from,
                                                 int fromlen,
                                                 bool is_timeout);

                        udp_receiver(cage &c) : m_cage(c) {}

                private:
                        cage   &m_cage;
                };

                class join_func {
                public:
                        void operator() (std::vector<cageaddr> &nodes);

                        callback_join   func;
                        cage   *p_cage;
                };

                udphandler      m_udp;
                timer           m_timer;
                uint160_t       m_id;
                udp_receiver    m_receiver;
                peers           m_peers;
                natdetector     m_nat;
                dtun            m_dtun;
                dht             m_dht;
                bool            m_is_dtun;
                dgram           m_dgram;
                proxy           m_proxy;

#ifdef DEBUG_NAT
        public:
                static void     test_natdetect();
                static void     test_nattypedetect();
#endif // DEBUG_NAT


#ifdef DEBUG
                class dtun_find_node_callback {
                public:
                        void operator() (std::vector<cageaddr> &addrs);

                        int     n;
                        cage   *p_cage;
                };

                class dtun_find_value_callback {
                public:
                        void operator() (bool result, cageaddr &addr,
                                         cageaddr &from);

                        int     n;
                        cage   *p_cage;
                };

                class dht_find_node_callback {
                public:
                        void operator() (std::vector<cageaddr> &addrs);

                        int     n;
                        cage   *p_cage;
                };

                class dtun_request_callback {
                public:
                        void operator() (bool result, cageaddr &addr);

                        int     n;
                        cage   *p_cage;
                };

                class dht_get_callback {
                public:
                        void operator() (bool result, void *buf, int len);
                };

        public:
                static void     test_dtun();
#endif // DEBUG
        };
}

#endif // CAGE_HPP
