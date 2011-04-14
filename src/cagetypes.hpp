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


#ifndef CAGETYPES_HPP
#define CAGETYPES_HPP

#include "common.hpp"

#include "bn.hpp"

#include <vector>

#include <boost/shared_ptr.hpp>
#include <boost/random.hpp>
#include <boost/variant.hpp>

#include <netinet/in.h>

namespace libcage {
        class peers;
        class udphandler;

        typedef boost::shared_ptr<sockaddr_in>  in_ptr;
        typedef boost::shared_ptr<sockaddr_in6> in6_ptr;
        typedef boost::shared_ptr<uint160_t>    id_ptr;
        typedef boost::shared_ptr<const uint160_t>  id_const_ptr;

        typedef boost::uniform_int<uint32_t>    uint_dist;
        typedef boost::uniform_real<>           real_dist;
        typedef boost::variate_generator<boost::mt19937&,
                                         boost::uniform_int<uint32_t> > rand_uint;
        typedef boost::variate_generator<boost::mt19937&,
                                         boost::uniform_real<> > rand_real;


        class _id {
        public:
                id_ptr  id;

                bool operator== (const _id &rhs) const
                {
                        return *id == *rhs.id;
                }

                bool operator< (const _id &rhs) const
                {
                        return *id < *rhs.id;
                }
        };

        size_t hash_value(const _id &i);

        struct cageaddr {
                id_ptr          id;
                uint16_t        domain;
                boost::variant<in_ptr, in6_ptr> saddr;
        };

        static const uint16_t MAGIC_NUMBER = 0xbabe;
        static const uint8_t  CAGE_VERSION = 0;

        static const uint16_t domain_loopback = 0;
        static const uint16_t domain_inet     = 1;
        static const uint16_t domain_inet6    = 2;

        static const uint16_t state_global = 1;
        static const uint16_t state_nat    = 2;

        static const uint8_t type_undefined               = 0;
        static const uint8_t type_dgram                   = 0x01;
        static const uint8_t type_advertise               = 0x02;
        static const uint8_t type_advertise_reply         = 0x03;
        static const uint8_t type_nat                     = 0x10;
        static const uint8_t type_nat_echo                = 0x11;
        static const uint8_t type_nat_echo_reply          = 0x12;
        static const uint8_t type_nat_echo_redirect       = 0x13;
        static const uint8_t type_nat_echo_redirect_reply = 0x14;
        static const uint8_t type_dtun                    = 0x20;
        static const uint8_t type_dtun_ping               = 0x21;
        static const uint8_t type_dtun_ping_reply         = 0x22;
        static const uint8_t type_dtun_find_node          = 0x23;
        static const uint8_t type_dtun_find_node_reply    = 0x24;
        static const uint8_t type_dtun_find_value         = 0x25;
        static const uint8_t type_dtun_find_value_reply   = 0x26;
        static const uint8_t type_dtun_register           = 0x27;
        static const uint8_t type_dtun_request            = 0x28;
        static const uint8_t type_dtun_request_by         = 0x29;
        static const uint8_t type_dtun_request_reply      = 0x2A;
        static const uint8_t type_dht                     = 0x40;
        static const uint8_t type_dht_ping                = 0x41;
        static const uint8_t type_dht_ping_reply          = 0x42;
        static const uint8_t type_dht_find_node           = 0x43;
        static const uint8_t type_dht_find_node_reply     = 0x44;
        static const uint8_t type_dht_find_value          = 0x45;
        static const uint8_t type_dht_find_value_reply    = 0x46;
        static const uint8_t type_dht_store               = 0x47;
        static const uint8_t type_proxy                   = 0x80;
        static const uint8_t type_proxy_register          = 0x81;
        static const uint8_t type_proxy_register_reply    = 0x82;
        static const uint8_t type_proxy_store             = 0x83;
        static const uint8_t type_proxy_get               = 0x84;
        static const uint8_t type_proxy_get_reply         = 0x85;
        static const uint8_t type_proxy_dgram             = 0x86;
        static const uint8_t type_proxy_dgram_forwarded   = 0x87;
        static const uint8_t type_proxy_rdp               = 0x88;
        static const uint8_t type_proxy_rdp_forwarded     = 0x89;
        static const uint8_t type_rdp                     = 0x90;

        static const uint8_t data_are_nodes  = 0xa0;
        static const uint8_t data_are_values = 0xa1;
        static const uint8_t data_are_nul    = 0xa2;

        static const uint8_t get_by_udp = 0xb0;
        static const uint8_t get_by_rdp = 0xb1;

        static const uint8_t dht_flag_unique = 0x01;

        static const uint8_t dht_get_next = 0xc0;

        static const uint8_t proxy_get_success = 0xd0;
        static const uint8_t proxy_get_fail    = 0xd1;
        static const uint8_t proxy_get_next    = 0xd2;


        struct msg_hdr {
                uint16_t        magic;
                uint8_t         ver;
                uint8_t         type;
                uint16_t        len;
                uint16_t        reserved;
                uint8_t         src[CAGE_ID_LEN];
                uint8_t         dst[CAGE_ID_LEN];
        };

        struct msg_nat_echo {
                msg_hdr         hdr;
                uint32_t        nonce;
        };

        struct msg_nat_echo_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint16_t        domain;
                uint16_t        port;
                uint8_t         addr[16];
        };

        struct msg_nat_echo_redirect {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint16_t        port;
                uint16_t        padding;
        };

        struct msg_nat_echo_redirect_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint16_t        domain;
                uint16_t        port;
                uint8_t         addr[16];
        };

        struct msg_dtun_ping {
                msg_hdr         hdr;
                uint32_t        nonce;
        };

        struct msg_dtun_ping_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
        };

        struct msg_dtun_find_node {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        domain;
                uint16_t        state;
        };

        struct msg_inet {
                uint16_t        port;
                uint16_t        reserved;
                uint32_t        addr;
                uint8_t         id[CAGE_ID_LEN];
        };

        struct msg_inet6 {
                uint16_t        port;
                uint16_t        reserved;
                uint32_t        addr[4];
                uint8_t         id[CAGE_ID_LEN];
        };
        
        struct msg_dtun_find_node_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        domain;
                uint8_t         num;
                uint8_t         padding;
                uint32_t        addrs[1];
        };

        struct msg_dtun_find_value {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        domain;
                uint16_t        state;
        };

        struct msg_dtun_find_value_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        domain;
                uint8_t         num;
                uint8_t         flag;
                uint32_t        addrs[1];
        };

        struct msg_dtun_register {
                msg_hdr         hdr;
                uint32_t        session;
        };

        struct msg_dtun_request {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
        };

        struct msg_dtun_request_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
        };

        struct msg_dtun_request_by {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint16_t        domain;
                uint16_t        reserved;
                uint32_t        addr[1];
        };

        struct msg_dht_ping {
                msg_hdr         hdr;
                uint32_t        nonce;
        };

        struct msg_dht_ping_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
        };

        struct msg_dht_find_node {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        domain;
                uint16_t        padding;
        };

        struct msg_dht_find_node_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        domain;
                uint8_t         num;
                uint8_t         padding;
                uint32_t        addrs[1];
        };

        struct msg_dht_find_value {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        domain;
                uint16_t        keylen;

                // use RDP when 1
                uint8_t         flag;

                uint8_t         padding[3];
                uint32_t        key[1];
        };

        struct msg_nodes {
                uint16_t        domain;
                uint8_t         num;
                uint8_t         padding;
                uint32_t        addrs[1];
        };

        struct msg_data {
                uint16_t        keylen;
                uint16_t        valuelen;
                uint32_t        data[1];
        };

        struct msg_dht_find_value_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];

                uint16_t        index; // the index of the value in data[]
                uint16_t        total; // the number of values 

                // data[] is nul when 2
                // data[] is value when 1
                // data[] is nodes when 0
                uint8_t         flag;

                uint8_t         padding[3];
                uint32_t        data[1];
        };

        struct msg_dht_store {
                msg_hdr         hdr;
                uint8_t         id[CAGE_ID_LEN];
                uint8_t         from[CAGE_ID_LEN];
                uint16_t        keylen;
                uint16_t        valuelen;
                uint16_t        ttl;
                uint8_t         flags;
                uint8_t         reserved;
                uint32_t        data[1];
        };

        struct msg_dht_rdp_store {
                uint8_t         id[CAGE_ID_LEN];
                uint8_t         from[CAGE_ID_LEN];
                uint16_t        keylen;
                uint16_t        valuelen;
                uint16_t        ttl;
                uint8_t         flags;
                uint8_t         reserved;
        };

        struct msg_dht_rdp_get {
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        keylen;
                uint16_t        reserved;
        };

        struct msg_dht_rdp_get_reply {
                uint16_t        valuelen;
                uint16_t        reserved;
        };

        struct msg_dgram {
                msg_hdr         hdr;
                uint32_t        data[1];
        };

        struct msg_proxy_register {
                msg_hdr         hdr;
                uint32_t        session;
                uint32_t        nonce;
        };

        struct msg_proxy_register_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
        };

        struct msg_proxy_store {
                msg_hdr         hdr;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        keylen;
                uint16_t        valuelen;
                uint16_t        ttl;
                uint8_t         flags;
                uint8_t         reserved;
                uint32_t        data[1];
        };

        struct msg_proxy_get {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        keylen;
                uint16_t        reserved;
                uint32_t        key[1];
        };

        struct msg_proxy_get_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];

                uint16_t        index; // the index of the value in data[]
                uint16_t        total; // the number of values

                // data[] is value when 1
                // data[] is nodes when 0
                uint8_t         flag;

                uint8_t         padding[3];
                uint32_t        data[1];
        };

        struct msg_proxy_rdp_get_reply {
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint8_t         flag;
                uint8_t         reserved[3];
        };

        struct msg_proxy_rdp_get_reply_val {
                uint16_t        valuelen;
                uint16_t        reserved;
        };

        struct msg_proxy_rdp_get {
                uint32_t        nonce;
                uint8_t         id[CAGE_ID_LEN];
                uint16_t        keylen;
                uint16_t        reserved;
        };        

        struct msg_proxy_dgram {
                msg_hdr         hdr;
                uint32_t        data[1];
        };

        struct msg_proxy_dgram_forwarded {
                msg_hdr         hdr;
                uint16_t        domain;
                uint16_t        port;
                uint32_t        addr[4];
                uint32_t        data[1];
        };

        struct msg_advertise {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint32_t        session;
        };

        struct msg_advertise_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint32_t        session;
        };

        enum node_state {
                node_undefined,
                node_nat,
                node_cone,
                node_symmetric,
                node_global
        };

        cageaddr        new_cageaddr(msg_hdr *hdr, sockaddr *saddr);
        void            write_nodes_inet(msg_inet *min,
                                         std::vector<cageaddr> &nodes);
        void            write_nodes_inet6(msg_inet6 *min6,
                                          std::vector<cageaddr> &nodes);
        void            read_nodes_inet(msg_inet *min, int num,
                                        std::vector<cageaddr> &nodes,
                                        sockaddr *from, peers &p);
        void            read_nodes_inet6(msg_inet6 *min6, int num,
                                         std::vector<cageaddr> &nodes,
                                         sockaddr *from, peers &p);
        void            send_msg(udphandler &udp, msg_hdr *hdr, uint16_t len,
                                 uint8_t type, cageaddr &dst,
                                 const uint160_t &src);
}

#endif // CAGETYPES_HPP
