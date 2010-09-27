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

#ifndef NATDETECTOR_HPP
#define NATDETECTOR_HPP

#include "common.hpp"
#include "bn.hpp"
#include "cagetypes.hpp"
#include "timer.hpp"
#include "peers.hpp"
#include "udphandler.hpp"

#include <map>
#include <string>

#include <boost/random.hpp>

namespace libcage {
        class proxy;
        class dht;

        class natdetector {
        private:
                static const time_t     timer_interval;

        public:
                natdetector(rand_uint &rnd, udphandler &udp, timer &t,
                            const uint160_t &id, dht &d, peers &p, proxy &pr);
                virtual ~natdetector();

                void            detect(std::string host, int port);

                void            detect_nat(std::string host, int port);
                void            detect_nat(sockaddr *saddr, int slen);
                void            detect_nat_type(sockaddr *saddr1,
                                                sockaddr *saddr2,
                                                int slen);
                void            recv_echo(void *msg, sockaddr *from,
                                          int fromlen);
                void            recv_echo_reply(void *msg, sockaddr *from,
                                                int fromlen);
                void            recv_echo_redirect(void *msg, sockaddr *from,
                                                   int fromlen);
                void            recv_echo_redirect_reply(void *msg);


                node_state      get_state() const;
                void            set_state_global() { m_state = global; }

        private:
                enum state {
                        undefined,
                        echo_wait1,
                        echo_redirect_wait,
                        global,
                        nat,
                        echo_wait2,
                        cone_nat,
                        symmetric_nat
                };

                class timer_nat : public timer::callback {
                public:
                        virtual void operator() ();

                        timer_nat(natdetector &nat) : m_nat(nat) { }
                        virtual ~timer_nat();

                        natdetector    &m_nat;
                };

                class timer_echo_wait1 : public timer::callback {
                public:
                        virtual void operator() ();

                        uint32_t        m_nonce;
                        natdetector    *m_nat;
                };

                class timer_echo_wait2 : public timer::callback {
                public:
                        virtual void operator() ();

                        uint32_t        m_nonce;
                        natdetector    *m_nat;
                };

                class udp_receiver : public udphandler::callback {
                public:
                        virtual void operator() (udphandler &udp, 
                                                 packetbuf_ptr pbuf,
                                                 sockaddr *from, int fromlen,
                                                 bool is_timeout);

                        uint32_t        m_nonce;
                        natdetector    *m_nat;
                        udphandler      m_udp;

#ifdef DEBUG_NAT
                        ~udp_receiver() { printf("close socket\n"); }
#endif // DEBUG_NAT
                };

                typedef boost::shared_ptr<timer::callback> callback_ptr;
                typedef boost::shared_ptr<udphandler::callback> udp_ptr;

                static const time_t     echo_timeout;

                rand_uint              &m_rnd;
                state                   m_state;
                timer                  &m_timer;
                udphandler             &m_udp;
                udphandler             *m_udp_tmp;
                const uint160_t        &m_id;
                dht                    &m_dht;
                peers                  &m_peers;
                proxy                  &m_proxy;
                uint16_t                m_global_port;
                uint8_t                 m_global_addr[16];
                timer_nat               m_timer_nat;

                std::map<uint32_t, callback_ptr>        m_timers;
                std::map<uint32_t, udp_ptr>             m_udps;
                std::map<uint32_t, uint16_t>            m_reply;

                void            recv_echo_reply_wait1(void *msg, sockaddr *from,
                                                      int fromlen);
                void            recv_echo_reply_wait2(void *msg);

                void            get_echo(msg_nat_echo &echo);

                void            join_dht();

        public:
                void            set_state_nat() { m_state = nat; }
                void            set_state_cone_nat() { m_state = cone_nat; }
                void            set_state_symmetric_nat() { m_state = symmetric_nat; }
        };
}


#endif // NATDETECTOR_HPP
