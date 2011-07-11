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

#include "advertise.hpp"
#include "bn.hpp"
#include "dgram.hpp"
#include "dtun.hpp"
#include "dht.hpp"
#include "natdetector.hpp"
#include "packetbuf.hpp"
#include "peers.hpp"
#include "proxy.hpp"
#include "rdp.hpp"
#include "timer.hpp"
#include "udphandler.hpp"

#include <openssl/evp.h>
#include <openssl/rand.h>

namespace libcage {
        class cage {
        public:
                cage();
                virtual ~cage();

                typedef boost::function<void (bool)>
                callback_join;

                bool            open(int domain, uint16_t port,
                                     bool is_dtun = true);
                void            put(const void *key, uint16_t keylen,
                                    const void *value, uint16_t valuelen,
                                    uint16_t ttl, bool is_unique = false);
                void            get(const void *key, uint16_t keylen,
                                    dht::callback_find_value func);
                void            join(std::string host, int port,
                                     callback_join func);

                // for reliable datagram transmission like TCP
                //
                // NOTICE:
                //     Currentry, this protocol doesn't support congestion
                //     control.
                int             rdp_listen(uint16_t sport,
                                           callback_rdp_event func);
                int             rdp_connect(uint16_t sport, id_ptr did,
                                            uint16_t dport,
                                            callback_rdp_event func);
                void            rdp_close(int desc);
                int             rdp_send(int desc, const void *buf, int len);
                void            rdp_receive(int desc, void *buf, int *len);
                rdp_state       rdp_get_desc_state(int desc);
                void            rdp_get_status(std::vector<rdp_status> &vec);
                void            rdp_set_max_retrans(time_t sec);
                time_t          rdp_get_max_retrans();


                // for dgram messege transmission like UDP
                //
                // NOTCE:
                //     If the length is bigger than 896 bytes, the message is
                //     automatically divided and then deliverd to the
                //     destination unlike UDP.
                //     The destination node should be receive multiple times.
                void            send_dgram(const void *buf, int len,
                                           uint8_t *dst);
                void            set_dgram_callback(dgram::callback func);
                void            unset_dgram_callback();

                std::string     get_id_str() const;
                void            get_id(void *addr) const;
                node_state      get_nat_state() const { return m_nat.get_state(); }

                void            set_global() { m_nat.set_state_global(); }

                void            set_nat() { m_nat.set_state_nat(); }
                void            set_cone_nat() { m_nat.set_state_cone_nat(); }
                void            set_symmetric_nat() { m_nat.set_state_symmetric_nat(); }
                void            set_id(const char *buf, int len);

                void            print_state() const;


        private:
                class udp_receiver : public udphandler::callback {
                public:
                        virtual void operator() (udphandler &udp,
                                                 packetbuf_ptr pbuf,
                                                 sockaddr *from, int fromlen,
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

                class rdp_output {
                public:
                        cage &m_cage;
                        
                        rdp_output(cage &c) : m_cage(c) { }

                        void operator() (id_ptr id_dst, packetbuf_ptr pbuf);
                };

                class gen_id {
                public:
                        gen_id(uint160_t &id)
                        {
                                unsigned char buf[20];

                                RAND_pseudo_bytes(buf, sizeof(buf));
                                id.from_binary(buf, sizeof(buf));

                                seed = *(uint32_t*)buf + time(NULL);
                        }

                        uint32_t seed;
                };

                gen_id          m_gen_id;
                boost::mt19937  m_gen;

                uint_dist       m_dist_int;
                rand_uint       m_rnd;

                real_dist       m_dist_real;
                rand_real       m_drnd;

                udphandler      m_udp;
                timer           m_timer;
                uint160_t       m_id;
                udp_receiver    m_receiver;
                peers           m_peers;
                natdetector     m_nat;
                dtun            m_dtun;
                rdp             m_rdp;
                dht             m_dht;
                bool            m_is_dtun;
                dgram           m_dgram;
                proxy           m_proxy;
                advertise       m_advertise;

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
