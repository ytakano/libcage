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

#include "cage.hpp"

#include <boost/foreach.hpp>

#include "cagetypes.hpp"

namespace libcage {
        extern void no_action(std::vector<cageaddr> &nodes);

        void
        cage::udp_receiver::operator() (udphandler &udp, packetbuf_ptr pbuf,
                                        sockaddr *from, int fromlen,
                                        bool is_timeout)
        {
                int len = pbuf->get_len();
                
                if (len < (int)sizeof(msg_hdr)) {
                        return;
                }

                void    *buf = pbuf->get_data();
                msg_hdr *hdr = (msg_hdr*)buf;

                if (ntohs(hdr->magic) != MAGIC_NUMBER ||
                    hdr->ver != CAGE_VERSION) {
                        return;
                }

                switch (hdr->type) {
                case type_nat_echo:
                        if (len == (int)sizeof(msg_nat_echo)) {
#ifdef DEBUG_NAT
                                printf("recv nat_echo\n");
#endif // DEBUG_NAT
                                m_cage.m_nat.recv_echo(buf, from, fromlen);
                        }
                        break;
                case type_nat_echo_reply:
                        if (len == (int)sizeof(msg_nat_echo_reply)) {
#ifdef DEBUG_NAT
                                printf("recv nat_echo_reply\n");
#endif // DEBUG_NAT
                                m_cage.m_nat.recv_echo_reply(buf, from,
                                                             fromlen);
                        }
                        break;
                case type_nat_echo_redirect:
                        if (len == (int)sizeof(msg_nat_echo_redirect)) {
#ifdef DEBUG_NAT
                                printf("recv nat_echo_redirect\n");
#endif // DEBUG_NAT
                                m_cage.m_nat.recv_echo_redirect(buf, from,
                                                                fromlen);
                        }
                        break;
                case type_dtun_ping:
                        if (len == (int)sizeof(msg_dtun_ping)) {
                                m_cage.m_dtun.recv_ping(buf, from, fromlen);
                        }
                        break;
                case type_dtun_ping_reply:
                        if (len == (int)sizeof(msg_dtun_ping_reply)) {
                                m_cage.m_dtun.recv_ping_reply(buf, from,
                                                              fromlen);
                        }
                        break;
                case type_dtun_find_node:
                        if (len == (int)sizeof(msg_dtun_find_node)) {
                                m_cage.m_dtun.recv_find_node(buf, from,
                                                             fromlen);
                        }
                        break;
                case type_dtun_find_node_reply:
                        if (len >= (int)(sizeof(msg_dtun_find_node_reply) -
                                         sizeof(uint32_t))) {
                                m_cage.m_dtun.recv_find_node_reply(buf, len,
                                                                   from);
                        }
                        break;
                case type_dtun_register:
                        if (len == (int)sizeof(msg_dtun_register)) {
                                m_cage.m_dtun.recv_register(buf, from);
                        }
                        break;
                case type_dtun_find_value:
                        if (len == (int)sizeof(msg_dtun_find_value)) {
                                m_cage.m_dtun.recv_find_value(buf, from,
                                                              fromlen);
                        }
                        break;
                case type_dtun_find_value_reply:
                        if (len >= (int)(sizeof(msg_dtun_find_value_reply) -
                                         sizeof(uint32_t))) {
                                m_cage.m_dtun.recv_find_value_reply(buf, len,
                                                                    from);
                        }
                        break;
                case type_dtun_request:
                        if (len == (int)sizeof(msg_dtun_request)) {
                                m_cage.m_dtun.recv_request(buf, from, fromlen);
                        }
                        break;
                case type_dtun_request_by:
                        if (len >= (int)(sizeof(msg_dtun_request) -
                                         sizeof(uint32_t))) {
                                m_cage.m_dtun.recv_request_by(buf, len, from);
                        }
                        break;
                case type_dtun_request_reply:
                        if (len == (int)sizeof(msg_dtun_request_reply)) {
                                m_cage.m_dtun.recv_request_reply(buf, from);
                        }
                        break;
                case type_dht_find_node:
                        if (len == (int)sizeof(msg_dht_find_node)) {
                                m_cage.m_dht.recv_find_node(buf, from);
                        }
                        break;
                case type_dht_find_node_reply:
                        if (len >= (int)(sizeof(msg_dht_find_node_reply) -
                                         sizeof(uint32_t))) {
                                m_cage.m_dht.recv_find_node_reply(buf, len,
                                                                  from);
                        }
                        break;
                case type_dht_store:
                        if (len >= (int)(sizeof(msg_dht_store) -
                                         sizeof(uint32_t))) {
                                m_cage.m_dht.recv_store(buf, len, from);
                        }
                        break;
                case type_dht_find_value:
                        if (len >= (int)(sizeof(msg_dht_find_value) - 
                                         sizeof(uint32_t))) {
                                m_cage.m_dht.recv_find_value(buf, len, from);
                        }
                        break;
                case type_dht_find_value_reply:
                        if (len >= (int)(sizeof(msg_dht_find_value_reply) - 
                                         sizeof(uint32_t))) {
                                m_cage.m_dht.recv_find_value_reply(buf, len,
                                                                   from);
                        }
                        break;
                case type_rdp:
                case type_dgram:
                        if (len >= (int)sizeof(msg_hdr)) {
                                m_cage.m_dgram.recv_dgram(pbuf, from);
                        }
                        break;
                case type_proxy_register:
                        if (len == (int)sizeof(msg_proxy_register)) {
                                m_cage.m_proxy.recv_register(buf, from);
                        }
                        break;
                case type_proxy_register_reply:
                        if (len == (int)sizeof(msg_proxy_register_reply)) {
                                m_cage.m_proxy.recv_register_reply(buf, from);
                        }
                        break;
                case type_proxy_store:
                        if (len >= (int)(sizeof(msg_proxy_store) -
                                         sizeof(uint32_t))) {
                                m_cage.m_proxy.recv_store(buf, len, from);
                        }
                        break;
                case type_proxy_get:
                        if (len >= (int)(sizeof(msg_proxy_get) - 
                                         sizeof(uint32_t))) {
                                m_cage.m_proxy.recv_get(buf, len);
                        }
                        break;
                case type_proxy_get_reply:
                        if (len >= (int)(sizeof(msg_proxy_get) -
                                         sizeof(uint32_t))) {
                                m_cage.m_proxy.recv_get_reply(buf, len);
                        }
                        break;
                case type_proxy_rdp:
                case type_proxy_dgram:
                        if (len >= (int)(sizeof(msg_proxy_dgram) -
                                         sizeof(uint32_t))) {
                                m_cage.m_proxy.recv_dgram(pbuf);
                        }
                        break;
                case type_proxy_rdp_forwarded:
                case type_proxy_dgram_forwarded:
                        if (len >= (int)(sizeof(msg_proxy_get) -
                                         sizeof(uint32_t))) {
                                m_cage.m_proxy.recv_forwarded(pbuf);
                        }
                        break;
                case type_advertise:
                        if (len == (int)sizeof(msg_advertise)) {
                                m_cage.m_advertise.recv_advertise(buf, from);
                        }
                        break;
                case type_advertise_reply:
                        if (len == (int)sizeof(msg_advertise_reply)) {
                                m_cage.m_advertise.recv_advertise_reply(buf,
                                                                        from);
                        }
                        break;
                }
        }

        cage::cage() : m_gen_id(m_id),
                       m_gen(m_gen_id.seed),
                       m_dist_int(0, ~0),
                       m_rnd(m_gen, m_dist_int),
                       m_dist_real(0, 1),
                       m_drnd(m_gen, m_dist_real),
                       m_receiver(*this),
                       m_peers(m_drnd, m_timer),
                       m_nat(m_rnd, m_udp, m_timer, m_id, m_peers, m_proxy),
                       m_dtun(m_rnd, m_drnd, m_id, m_timer, m_peers, m_nat,
                              m_udp, m_proxy),
                       m_rdp(m_rnd, m_timer),
                       m_dht(m_rnd, m_drnd, m_id, m_timer, m_peers, m_nat,
                             m_udp, m_dtun, m_rdp),
                       m_dgram(m_id, m_peers, m_udp, m_dtun, m_dht, m_proxy,
                               m_advertise, m_rdp),
                       m_proxy(m_rnd, m_drnd, m_id, m_udp, m_timer, m_nat,
                               m_peers, m_dtun, m_dht, m_dgram, m_advertise,
                               m_rdp),
                       m_advertise(m_rnd, m_drnd, m_id, m_timer, m_udp,
                                   m_peers, m_dtun)
        {
                m_rdp.set_callback_dgram_out(rdp_output(*this));
        }

        cage::~cage()
        {

        }

        void
        cage::set_id(char *buf, int len)
        {
                EVP_MD_CTX md_ctx;
                unsigned int md_len;
                unsigned char md_value[EVP_MAX_MD_SIZE];

                EVP_MD_CTX_init(&md_ctx);
                EVP_DigestInit_ex(&md_ctx, EVP_sha1(), NULL);
                EVP_DigestUpdate(&md_ctx, buf, len);
                EVP_DigestFinal_ex(&md_ctx, md_value, &md_len);
                EVP_MD_CTX_cleanup(&md_ctx);

                m_id.from_binary(md_value, md_len);
        }

        int
        cage::rdp_listen(uint16_t sport, callback_rdp_event func)
        {
                return m_rdp.listen(sport, func);
        }

        int
        cage::rdp_connect(uint16_t sport, id_ptr did, uint16_t dport,
                          callback_rdp_event func)
        {
                return m_rdp.connect(sport, did, dport, func);
        }

        void
        cage::rdp_close(int desc)
        {
                m_rdp.close(desc);
        }

        int
        cage::rdp_send(int desc, const void *buf, int len)
        {
                return m_rdp.send(desc, buf, len);
        }
        
        void
        cage::rdp_receive(int desc, void *buf, int *len)
        {
                m_rdp.receive(desc, buf, len);
        }

        rdp_state
        cage::rdp_status(int desc)
        {
                return m_rdp.status(desc);
        }

        void
        cage::send_dgram(const void *buf, int len, uint8_t *dst)
        {
                boost::shared_ptr<uint160_t> id(new uint160_t);

                id->from_binary(dst, CAGE_ID_LEN);

                if (m_nat.get_state() == node_symmetric) {
                        m_proxy.send_dgram(buf, len, id);
                } else {
                        m_dgram.send_dgram(buf, len, id);
                }
        }

        void
        cage::print_state() const
        {
                std::string str;

                str = m_id.to_string();
                printf("MyID = %s\n\n", str.c_str());

                printf("Node State:\n");

                switch (m_nat.get_state()) {
                case node_undefined:
                        printf("  undefined\n");
                        break;
                case node_global:
                        printf("  Global\n");
                        break;
                case node_nat:
                        printf("  NAT\n");
                        break;
                case node_cone:
                        printf("  Cone NAT\n");
                        break;
                case node_symmetric:
                        printf("  Symmetric NAT\n");
                        break;
                }

                printf("DTUN Table:\n");

                if (m_is_dtun)
                        m_dtun.print_table();
                else
                        printf("  disabled\n");

                printf("\n");

                printf("DHT Table:\n");
                m_dht.print_table();
        }

        bool
        cage::open(int domain, uint16_t port, bool is_dtun)
        {
                if (!m_udp.open(domain, port))
                        return false;

                if (domain == PF_INET6) {
                        m_dtun.set_enabled(false);
                        m_dht.set_enabled_dtun(false);
                        m_nat.set_state_global();
                        m_is_dtun = false;
                } else {
                        m_dtun.set_enabled(is_dtun);
                        m_dht.set_enabled_dtun(is_dtun);
                        m_is_dtun = is_dtun;

                        if (! m_is_dtun)
                                m_nat.set_state_global();
                }

                m_udp.set_callback(&m_receiver);

                return true;
        }

        void
        cage::set_dgram_callback(dgram::callback func)
        {
                m_dgram.set_callback(func);
                m_proxy.set_callback(func);
        }

        static void
        no_action_dgram(void *buf, size_t len, uint8_t *addr)
        {

        }

        void
        cage::unset_dgram_callback()
        {
                m_dgram.set_callback(&no_action_dgram);
                m_proxy.set_callback(&no_action_dgram);
        }

        void
        cage::put(const void *key, uint16_t keylen,
                  const void *value, uint16_t valuelen, uint16_t ttl,
                  bool is_unique)
        {
                EVP_MD_CTX      ctx;
                uint160_t       id;
                uint32_t        len;
                uint8_t         buf[20];

                EVP_MD_CTX_init(&ctx);
                EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);
                EVP_DigestUpdate(&ctx, key, keylen);
                EVP_DigestFinal_ex(&ctx, buf, &len);
                EVP_MD_CTX_cleanup(&ctx);

                id.from_binary(buf, sizeof(buf));

                if (m_nat.get_state() == node_symmetric) {
                        m_proxy.store(id, key, keylen, value, valuelen, ttl,
                                      is_unique);
                } else {
                        m_dht.store(id, key, keylen, value, valuelen, ttl,
                                    is_unique);
                }
        }

        void
        cage::get(const void *key, uint16_t keylen,
                  dht::callback_find_value func)
        {
                EVP_MD_CTX      ctx;
                uint160_t       id;
                uint32_t        len;
                uint8_t         buf[20];

                EVP_MD_CTX_init(&ctx);
                EVP_DigestInit_ex(&ctx, EVP_sha1(), NULL);
                EVP_DigestUpdate(&ctx, key, keylen);
                EVP_DigestFinal_ex(&ctx, buf, &len);
                EVP_MD_CTX_cleanup(&ctx);

                id.from_binary(buf, sizeof(buf));

                if (m_nat.get_state() == node_symmetric) {
                        m_proxy.get(id, key, keylen, func);
                } else {
                        m_dht.find_value(id, key, keylen, func);
                }
        }

        void
        cage::join_func::operator() (std::vector<cageaddr> &nodes)
        {
                if (nodes.size() > 0) {
                        p_cage->m_dtun.register_node();
                        func(true);
                } else {
                        func(false);
                }
        }

        void
        cage::join(std::string host, int port, callback_join func)
        {
                join_func f;

                f.func   = func;
                f.p_cage = this;

                if (m_is_dtun) {
                        m_nat.detect(host, port);
                        m_dtun.find_node(host, port, f);
                        m_dht.find_node(host, port, &no_action);
                } else {
                        m_dht.find_node(host, port, f);
                }
        }

        void
        cage::get_id(void *addr) const
        {
                m_id.to_binary(addr, CAGE_ID_LEN);
        }

        void
        cage::rdp_output::operator() (id_ptr id_dst, packetbuf_ptr pbuf)
        {
                if (m_cage.m_nat.get_state() == node_symmetric) {
                        m_cage.m_proxy.send_dgram(pbuf, id_dst, type_rdp);
                } else {
                        m_cage.m_dgram.send_dgram(pbuf, id_dst, type_rdp);
                }
        }

#ifdef DEBUG_NAT
        void
        cage::test_natdetect()
        {
                cage *c1, *c2;
                c1 = new cage();
                c2 = new cage();

                c1->open(PF_INET, 0);
                c2->open(PF_INET, 0);

                c1->m_nat.detect_nat("localhost", ntohs(c2->m_udp.get_port()));
        }

        void
        cage::test_nattypedetect()
        {
                sockaddr_in saddr1, saddr2;
                cage *c1, *c2, *c3;
                c1 = new cage();
                c2 = new cage();
                c3 = new cage();

                c1->open(PF_INET, 0);
                c2->open(PF_INET, 0);
                c3->open(PF_INET, 0);

                c1->m_nat.set_state_nat();

                memset(&saddr1, 0, sizeof(saddr1));
                memset(&saddr2, 0, sizeof(saddr2));

                saddr1.sin_family = PF_INET;
                saddr1.sin_port   = c2->m_udp.get_port();
                saddr1.sin_addr.s_addr = htonl(127 << 24 + 1);

                saddr2.sin_family = PF_INET;
                saddr2.sin_port   = c3->m_udp.get_port();
                saddr2.sin_addr.s_addr = htonl(127 << 24 + 1);

                c1->m_nat.detect_nat_type((sockaddr*)&saddr1,
                                          (sockaddr*)&saddr2,
                                          sizeof(saddr1));

        }
#endif // DEBUG_NAT

#ifdef DEBUG
        #define NUM_NODES 100

        void
        cage::dtun_find_node_callback::operator() (std::vector<cageaddr> &addrs)
        {
                printf("dtun: recv find node reply\n");

                BOOST_FOREACH(cageaddr &addr, addrs) {
                        in_ptr in = boost::get<in_ptr>(addr.saddr);
                        printf("  port = %d, ", ntohs(in->sin_port));
                        printf("id = %s\n", addr.id->to_string().c_str());
                }

                p_cage[n].m_dtun.register_node();
                p_cage[n].m_dtun.print_table();

                n++;

                if (n < NUM_NODES) {
                        p_cage[n].open(PF_INET, 11000 + n);
                        p_cage[n].m_nat.set_state_global();
                        p_cage[n].m_dtun.find_node("localhost", 10000,
                                                    *this);


                        dht_find_node_callback  func;
                        func.n = n;
                        func.p_cage = p_cage;

                        p_cage[n].m_dht.find_node("localhost", 10000, func);

                } else {
                        // test find value
                        dtun_find_value_callback func1;
                        p_cage[0].m_dtun.find_value(p_cage[NUM_NODES - 2].m_id,
                                                    func1);

                        // test request
                        dtun_request_callback func2;
                        p_cage[1].m_dtun.request(p_cage[NUM_NODES - 3].m_id,
                                                 func2);
                }
        }

        void
        cage::dtun_request_callback::operator() (bool result, cageaddr &addr)
        {
                printf("dtun: recv request reply\n");

                if (result)
                        printf("  true\n");
                else
                        printf("  false\n");
        }

        void
        cage::dtun_find_value_callback::operator() (bool result, cageaddr &addr,
                                                    cageaddr &from)
        {
                printf("dtun: recv find value reply\n");

                if (result)
                        printf("  true\n");
                else
                        printf("  false\n");
        }

        void
        cage::dht_find_node_callback::operator() (std::vector<cageaddr> &addrs)
        {
                printf("dht: recv find node reply\n");

                BOOST_FOREACH(cageaddr &addr, addrs) {
                        in_ptr in = boost::get<in_ptr>(addr.saddr);
                        printf("  port = %d, ", ntohs(in->sin_port));
                        printf("id = %s\n", addr.id->to_string().c_str());
                }

                dht_get_callback func;
                int k = 50;

                p_cage[n].m_dht.print_table();

                printf("put: n = %d\n", n);
                p_cage[n].put((void*)&n, sizeof(n), (void*)&n, sizeof(n), 300);
                p_cage[n].get((void*)&k, sizeof(k), func);
        }

        void
        cage::dht_get_callback::operator() (bool result, void *buf, int len)
        {
                printf("dht: get\n");

                if (result)
                        printf("  true\n");
                else
                        printf("  false\n");
        }

        void
        cage::test_dtun()
        {
                dtun_find_node_callback func;
                dht_find_node_callback  func2;
                cage *c;

                // open bootstrap node
                c = new cage;
                c->open(PF_INET, 10000);
                c->m_nat.set_state_global();

                // connect to bootstrap
                func.n = 0;
                func.p_cage = new cage[NUM_NODES];

                func2.n = 0;
                func2.p_cage = func.p_cage;

                func.p_cage[0].open(PF_INET, 11000);
                func.p_cage[0].m_nat.set_state_global();
                func.p_cage[0].m_dtun.find_node("localhost", 10000, func);
                func.p_cage[0].m_dht.find_node("localhost", 10000, func2);
        }
#endif // DEBUG
}
