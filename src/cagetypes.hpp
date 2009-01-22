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
#include <boost/variant.hpp>


namespace libcage {
        class peers;

        typedef boost::shared_ptr<sockaddr_in>  in_ptr;
        typedef boost::shared_ptr<sockaddr_in6> in6_ptr;
        typedef boost::shared_ptr<uint160_t>    id_ptr;

        struct cageaddr {
                id_ptr          id;
                sa_family_t     domain;
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
        static const uint8_t type_nat_echo                = 1;
        static const uint8_t type_nat_echo_reply          = 2;
        static const uint8_t type_nat_echo_redirect       = 3;
        static const uint8_t type_nat_echo_redirect_reply = 4;
        static const uint8_t type_dtun_ping               = 5;
        static const uint8_t type_dtun_ping_reply         = 6;
        static const uint8_t type_dtun_find_node          = 7;
        static const uint8_t type_dtun_find_node_reply    = 8;
        static const uint8_t type_dtun_find_value         = 9;
        static const uint8_t type_dtun_find_value_reply   = 10;
        static const uint8_t type_dtun_register           = 11;
        static const uint8_t type_dtun_request            = 12;
        static const uint8_t type_dtun_request_by         = 13;
        static const uint8_t type_dtun_request_reply      = 14;
        static const uint8_t type_dht_ping                = 15;
        static const uint8_t type_dht_ping_reply          = 16;
        static const uint8_t type_dht_find_node           = 17;
        static const uint8_t type_dht_find_node_reply     = 18;
        static const uint8_t type_dht_find_value          = 19;
        static const uint8_t type_dht_find_value_reply    = 20;
        static const uint8_t type_dht_store               = 21;
        

        struct msg_hdr {
                uint16_t        magic;
                uint8_t         ver;
                uint8_t         type;
                uint16_t        len;
                uint16_t        reserved;
                uint8_t         src[20];
                uint8_t         dst[20];
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
                uint32_t        id[5];
                uint16_t        domain;
                uint16_t        state;
        };

        struct msg_inet {
                uint16_t        port;
                uint16_t        reserved;
                uint32_t        addr;
                uint32_t        id[5];
        };

        struct msg_inet6 {
                uint16_t        port;
                uint16_t        reserved;
                uint32_t        addr[4];
                uint32_t        id[5];
        };
        
        struct msg_dtun_find_node_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint32_t        id[5];
                uint16_t        domain;
                uint8_t         num;
                uint8_t         padding;
                uint32_t        addrs[1];
        };

        struct msg_dtun_find_value {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint32_t        id[5];
                uint16_t        domain;
                uint16_t        state;
        };

        struct msg_dtun_find_value_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint32_t        id[5];
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
                uint32_t        id[5];
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
                uint32_t        id[5];
                uint16_t        domain;
                uint16_t        padding;
        };

        struct msg_dht_find_node_reply {
                msg_hdr         hdr;
                uint32_t        nonce;
                uint32_t        id[5];
                uint16_t        domain;
                uint8_t         num;
                uint8_t         padding;
                uint32_t        addrs[1];
        };

        struct msg_dht_store {
                msg_hdr         hdr;
                uint32_t        id[5];
                uint16_t        keylen;
                uint16_t        valuelen;
                uint16_t        ttl;
                uint16_t        reserved;
                uint32_t        data[1];
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
}

#endif // CAGETYPES_HPP
