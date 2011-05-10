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

#include "proxy.hpp"

#include "advertise.hpp"

#include <openssl/rand.h>

#include <boost/foreach.hpp>

namespace libcage {
        const time_t    proxy::register_timeout     = 2;
        const time_t    proxy::register_ttl         = 300;
        const time_t    proxy::get_timeout          = 10;
        const time_t    proxy::timer_interval       = 30;
        const time_t    proxy::rdp_timeout          = 30;
        const uint16_t  proxy::proxy_store_port     = 200;
        const uint16_t  proxy::proxy_get_port       = 201;
        const uint16_t  proxy::proxy_get_reply_port = 202;

        static void
        no_action(void *buf, size_t len, uint8_t *addr)
        {

        }

        proxy::proxy(rand_uint &rnd, rand_real &drnd, const uint160_t &id,
                     udphandler &udp, timer &t, natdetector &nat, peers &p,
                     dtun &dt, dht &dh, dgram &dg, advertise &adv, rdp &r) :
                m_rnd(rnd),
                m_drnd(drnd),
                m_id(id),
                m_udp(udp),
                m_timer(t),
                m_nat(nat),
                m_peers(p),
                m_dtun(dt),
                m_dht(dh),
                m_dgram(dg),
                m_advertise(adv),
                m_rdp(r),
                m_is_registered(false),
                m_is_registering(false),
                m_timer_register(*this),
                m_timer_proxy(*this),
                m_dgram_func(&no_action)
        {
                rdp_recv_store_func func1(*this);
                m_rdp_store_desc = m_rdp.listen(proxy_store_port, func1);

                rdp_recv_get_func func2(*this);
                m_rdp_get_desc = m_rdp.listen(proxy_get_port, func2);

                rdp_recv_get_reply_func func3(*this);
                m_rdp_get_reply_desc = m_rdp.listen(proxy_get_reply_port,
                                                    func3);
        }

        proxy::~proxy()
        {
                m_rdp.close(m_rdp_store_desc);
                m_rdp.close(m_rdp_get_desc);
                m_rdp.close(m_rdp_get_reply_desc);

                std::map<uint32_t, gd_ptr>::iterator it1;
                for (it1 = m_getdata.begin(); it1 != m_getdata.end(); ++it1)
                        m_timer.unset_timer(&it1->second->timeout);

                std::map<int, time_t>::iterator it2;
                for (it2 = m_rdp_store.begin(); it2 != m_rdp_store.end(); ++it2)
                        m_rdp.close(it2->first);

                std::map<int, rdp_recv_store_ptr>::iterator it3;
                for (it3 = m_rdp_recv_store.begin();
                     it3 != m_rdp_recv_store.end(); ++it3)
                        m_rdp.close(it3->first);

                std::map<int, rdp_get_ptr>::iterator it4;
                for (it4 = m_rdp_get.begin(); it4 != m_rdp_get.end(); ++it4)
                        m_rdp.close(it4->first);

                std::map<int, rdp_recv_get_ptr>::iterator it5;
                for (it5 = m_rdp_recv_get.begin(); it5 != m_rdp_recv_get.end();
                     ++it5)
                        m_rdp.close(it5->first);

                std::map<int, rdp_recv_get_reply_ptr>::iterator it6;
                for (it6 = m_rdp_recv_get_reply.begin();
                     it6 != m_rdp_recv_get_reply.end(); ++it6)
                        m_rdp.close(it6->first);

                std::map<int, time_t>::iterator it7;
                for (it7 = m_rdp_get_reply.begin();
                     it7 != m_rdp_get_reply.end(); ++it7)
                        m_rdp.close(it7->first);
        }

        void
        proxy::sweep_rdp()
        {
                time_t now = time(NULL);
                time_t diff;

                std::map<int, time_t>::iterator it1;
                for (it1 = m_rdp_store.begin(); it1 != m_rdp_store.end(); ) {
                        diff = now - it1->second;
                        if (diff > rdp_timeout) {
                                m_rdp.close(it1->first);
                                m_rdp_store.erase(it1++);
                                continue;
                        }
                        ++it1;
                }

                std::map<int, rdp_recv_store_ptr>::iterator it2;
                for (it2 = m_rdp_recv_store.begin();
                     it2 != m_rdp_recv_store.end(); ) {
                        diff = now - it2->second->m_time;
                        if (diff > rdp_timeout) {
                                m_rdp.close(it2->first);
                                m_rdp_recv_store.erase(it2++);
                                continue;
                        }
                        ++it2;
                }

                std::map<int, rdp_recv_get_ptr>::iterator it3;
                for (it3 = m_rdp_recv_get.begin();
                     it3 != m_rdp_recv_get.end(); ) {
                        diff = now - it3->second->m_time;
                        if (diff > rdp_timeout) {
                                m_rdp.close(it3->first);
                                m_rdp_recv_get.erase(it3++);
                                continue;
                        }
                        ++it3;
                }

                std::map<int, time_t>::iterator it4;
                for (it4 = m_rdp_get_reply.begin();
                     it4 != m_rdp_get_reply.end(); ) {
                        diff = now - it4->second;
                        if (diff > rdp_timeout) {
                                m_rdp.close(it4->first);
                                m_rdp_get_reply.erase(it4++);
                                continue;
                        }
                        ++it4;
                }

                std::map<int, rdp_recv_get_reply_ptr>::iterator it5;
                for (it5 = m_rdp_recv_get_reply.begin();
                     it5 != m_rdp_recv_get_reply.end(); ) {
                        diff = it5->second->m_time;
                        if (diff > rdp_timeout) {
                                m_rdp.close(it5->first);

                                switch (it5->second->m_state) {
                                case rdp_recv_get_reply::RGR_VAL_HDR:
                                case rdp_recv_get_reply::RGR_VAL:
                                {
                                        std::map<uint32_t, gd_ptr>::iterator it;

                                        it = m_getdata.find(it5->second->m_nonce);
                                        if (it == m_getdata.end()) {
                                                if (it->second->vset->size() > 0)
                                                        it->second->func(true, it->second->vset);
                                                else
                                                        it->second->func(false, it->second->vset);

                                                m_timer.unset_timer(&it->second->timeout);
                                                m_getdata.erase(it);
                                        }
                                }
                                default:
                                        m_rdp_recv_get_reply.erase(it5++);
                                }

                                continue;
                        }
                        ++it5;
                }
        }

        void
        proxy::timer_register::operator() ()
        {
                m_proxy.m_is_registering  = false;
        }

        void
        proxy::register_func::operator() (std::vector<cageaddr> &nodes)
        {
                BOOST_FOREACH(cageaddr &addr, nodes) {
                        if (p_proxy->m_id == *addr.id)
                                continue;

                        msg_proxy_register reg;

                        memset(&reg, 0, sizeof(reg));

                        p_proxy->m_nonce = p_proxy->m_rnd();

                        reg.session = htonl(p_proxy->m_dtun.get_session());
                        reg.nonce   = htonl(p_proxy->m_nonce);


                        // start timer
                        timeval tval;

                        tval.tv_sec  = proxy::register_timeout;
                        tval.tv_usec = 0;

                        p_proxy->m_timer.set_timer(&p_proxy->m_timer_register,
                                                   &tval);

                        send_msg(p_proxy->m_udp, &reg.hdr, sizeof(reg),
                                 type_proxy_register, addr, p_proxy->m_id);

                        break;
                }
        }

        void
        proxy::recv_dgram(packetbuf_ptr pbuf)
        {
                msg_proxy_dgram *data;
                id_ptr src(new uint160_t);
                id_ptr dst(new uint160_t);
                int    size;

                data = (msg_proxy_dgram*)pbuf->get_data();

                size = sizeof(data->hdr);

                if (size <= 0)
                        return;

                src->from_binary(data->hdr.src, sizeof(data->hdr.src));

                _id i;

                i.id = src;

                if (m_registered.find(i) == m_registered.end())
                        return;

                dst->from_binary(data->hdr.dst, sizeof(data->hdr.dst));

                m_dgram.send_dgram(pbuf, dst, data->hdr.type);
        }

        void
        proxy::recv_register_reply(void *msg, sockaddr *from)
        {
                msg_proxy_register_reply *reply;
                cageaddr  addr;
                uint160_t dst;
                uint32_t  nonce;

                if (! m_is_registering)
                        return;

                reply = (msg_proxy_register_reply*)msg;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));

                if (dst != m_id)
                        return;

                nonce = ntohl(reply->nonce);

                if (m_nonce != nonce)
                        return;

                addr = new_cageaddr(&reply->hdr, from);
                m_server = addr;

                m_is_registered = true;
                m_is_registering = false;
                m_timer.unset_timer(&m_timer_register);
        }

        void
        proxy::recv_register(void *msg, sockaddr *from)
        {
                msg_proxy_register *reg;
                cageaddr  addr;
                uint160_t dst;

                reg = (msg_proxy_register*)msg;

                dst.from_binary(reg->hdr.dst, sizeof(reg->hdr.dst));

                if (dst != m_id)
                        return;

                addr = new_cageaddr(&reg->hdr, from);


                std::map<_id, _addr>::iterator it;
                _id i;

                i.id = addr.id;
                it = m_registered.find(i);

                if (it == m_registered.end()) {
                        _addr a;

                        a.session   = ntohl(reg->session);
                        a.addr      = addr;
                        a.recv_time = time(NULL);

                        m_registered[i] = a;

                        m_dtun.register_node(addr.id, a.session);
                } else {
                        uint32_t session = ntohl(reg->session);

                        if (it->second.session == session) {
                                it->second.addr      = addr;
                                it->second.recv_time = time(NULL);
                        } else {
                                return;
                        }
                }

                msg_proxy_register_reply  reply;

                memset(&reply, 0, sizeof(reply));

                reply.nonce = reg->nonce;

                send_msg(m_udp, &reply.hdr, sizeof(reply),
                         type_proxy_register_reply, addr, m_id);
        }

        void
        proxy::store_by_rdp(const uint160_t &id, const void *key,
                            uint16_t keylen, const void *value,
                            uint16_t valuelen, uint16_t ttl, bool is_unique)
        {
                rdp_store_func func(*this);
                int            desc;

                create_store_func(func, id, key, keylen, value, valuelen, ttl,
                                  is_unique);

                desc = m_rdp.connect(0, m_server.id, proxy_store_port, func);

                m_rdp_store[desc] = time(NULL);
        }

        void
        proxy::create_store_func(rdp_store_func &func,
                                 const uint160_t &id, const void *key,
                                 uint16_t keylen, const void *value,
                                 uint16_t valuelen, uint16_t ttl,
                                 bool is_unique)
        {
                boost::shared_array<char> k(new char[keylen]);
                boost::shared_array<char> v(new char[valuelen]);
                id_ptr p_id(new uint160_t);

                memcpy(k.get(), key, keylen);
                memcpy(v.get(), value, valuelen);

                *p_id = id;

                func.m_key       = k;
                func.m_val       = v;
                func.m_keylen    = keylen;
                func.m_valuelen  = valuelen;
                func.m_ttl       = ttl;
                func.m_is_unique = is_unique;
                func.m_id        = p_id;
        }

        void
        proxy::store(const uint160_t &id, const void *key, uint16_t keylen,
                     const void *value, uint16_t valuelen, uint16_t ttl,
                     bool is_unique)
        {
                if (! m_is_registered) {
                        if (m_nat.get_state() == node_symmetric) {
                                register_node();
                                if (m_dht.is_use_rdp()) {
                                        rdp_store_func_ptr func(new rdp_store_func(*this));
                                        create_store_func(*func, id, key,
                                                          keylen, value,
                                                          valuelen, ttl,
                                                          is_unique);

                                        m_store_data.push_back(func);
                                }
                        }
                        return;
                }

                if (m_dht.is_use_rdp()) {
                        store_by_rdp(id, key, keylen, value, valuelen, ttl,
                                     is_unique);
                        return;
                }

                msg_proxy_store *store;
                char *p_value;
                char buf[1024 * 4];
                int  size;

                size = sizeof(*store) - sizeof(store->data) + keylen + valuelen;

                if (size > (int)sizeof(buf))
                        return;

                store = (msg_proxy_store*)buf;

                memset(store, 0, size);

                id.to_binary(store->id, sizeof(store->id));

                store->keylen   = htons(keylen);
                store->valuelen = htons(valuelen);
                store->ttl      = htons(ttl);

                if (is_unique)
                        store->flags = dht_flag_unique;

                p_value = (char*)store->data;
                p_value += keylen;

                memcpy(store->data, key, keylen);
                memcpy(p_value, value, valuelen);

                send_msg(m_udp, &store->hdr, size, type_proxy_store,
                         m_server, m_id);
        }

        void
        proxy::recv_store(void *msg, int len, sockaddr *from)
        {
                msg_proxy_store *store;
                uint160_t dst;
                uint16_t  keylen;
                uint16_t  valuelen;
                bool      is_unique = false;
                int       size;

                store = (msg_proxy_store*)msg;

                keylen   = ntohs(store->keylen);
                valuelen = ntohs(store->valuelen);

                size = sizeof(*store) - sizeof(store->data)
                        + keylen + valuelen;

                if (size != len)
                        return;

                dst.from_binary(store->hdr.dst, sizeof(store->hdr.dst));
                if (dst != m_id)
                        return;


                boost::shared_array<char> key(new char[keylen]);
                boost::shared_array<char> value(new char[valuelen]);
                id_ptr    id(new uint160_t);
                id_ptr    src(new uint160_t);
                char     *p_value;

                src->from_binary(store->hdr.src, sizeof(store->hdr.src));
                if (! is_registered(src))
                        return;


                id->from_binary(store->id, sizeof(store->id));

                if (store->flags & dht_flag_unique)
                        is_unique = true;

                p_value  = (char*)store->data;
                p_value += keylen;

                memcpy(key.get(), store->data, keylen);
                memcpy(value.get(), &store->data[keylen], valuelen);

                m_dht.store(id, key, keylen, value, valuelen,
                            ntohs(store->ttl), src, is_unique);
        }


        void
        proxy::rdp_recv_get_reply_func::close_rdp(int desc,
                                                  rdp_recv_get_reply_ptr ptr)
        {
                std::map<uint32_t, gd_ptr>::iterator it;

                it = m_proxy.m_getdata.find(ptr->m_nonce);
                if (it != m_proxy.m_getdata.end()) {
                        if (it->second->vset->size() > 0)
                                it->second->func(true, it->second->vset);
                        else
                                it->second->func(false, it->second->vset);

                        m_proxy.m_timer.unset_timer(&it->second->timeout);
                        m_proxy.m_getdata.erase(ptr->m_nonce);
                }

                m_proxy.m_rdp.close(desc);
                m_proxy.m_rdp_recv_get_reply.erase(desc);
        }

        bool
        proxy::rdp_recv_get_reply_func::read_hdr(int desc,
                                                 rdp_recv_get_reply_ptr ptr)
        {
                msg_proxy_rdp_get_reply msg;
                int size = sizeof(msg);

                m_proxy.m_rdp.receive(desc, &msg, &size);

                if (size == 0)
                        return false;


                std::map<uint32_t, gd_ptr>::iterator it;
                uint32_t nonce = ntohl(msg.nonce);

                it = m_proxy.m_getdata.find(nonce);
                if (it == m_proxy.m_getdata.end()) {
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_recv_get_reply.erase(desc);
                        return false;
                }

                ptr->m_nonce = nonce;
                ptr->m_time  = time(NULL);
                ptr->m_state = rdp_recv_get_reply::RGR_VAL_HDR;

                if (msg.flag == proxy_get_fail) {
                        close_rdp(desc, ptr);
                        return false;
                }

                uint8_t op = proxy_get_next;
                m_proxy.m_rdp.send(desc, &op, sizeof(op));

                return true;
        }

        bool
        proxy::rdp_recv_get_reply_func::read_val_hdr(int desc,
                                                     rdp_recv_get_reply_ptr ptr)
        {
                std::map<uint32_t, gd_ptr>::iterator it;

                it = m_proxy.m_getdata.find(ptr->m_nonce);
                if (it == m_proxy.m_getdata.end()) {
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_recv_get_reply.erase(desc);
                        return false;
                }


                msg_proxy_rdp_get_reply_val msg;
                int size = sizeof(msg);

                m_proxy.m_rdp.receive(desc, &msg, &size);

                if (size == 0)
                        return false;

                ptr->m_valuelen = ntohs(msg.valuelen);
                ptr->m_val_read = 0;
                ptr->m_state    = rdp_recv_get_reply::RGR_VAL;
                ptr->m_time     = time(NULL);

                boost::shared_array<char> val(new char[ptr->m_valuelen]);
                ptr->m_val = val;

                return true;
        }

        bool
        proxy::rdp_recv_get_reply_func::read_val(int desc,
                                                 rdp_recv_get_reply_ptr ptr)
        {
                std::map<uint32_t, gd_ptr>::iterator it;

                it = m_proxy.m_getdata.find(ptr->m_nonce);
                if (it == m_proxy.m_getdata.end()) {
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_recv_get_reply.erase(desc);
                        return false;
                }


                for (;;) {
                        int   size = ptr->m_valuelen - ptr->m_val_read;
                        char *buf  = &ptr->m_val[ptr->m_val_read];

                        m_proxy.m_rdp.receive(desc, buf, &size);

                        if (size == 0)
                                return false;

                        ptr->m_val_read += size;
                        ptr->m_time      = time(NULL);

                        if (ptr->m_val_read += ptr->m_valuelen) {
                                dht::value_t v;

                                v.value = ptr->m_val;
                                v.len   = ptr->m_valuelen;

                                it->second->vset->insert(v);


                                ptr->m_val_read = 0;
                                ptr->m_val.reset();
                                ptr->m_state = rdp_recv_get_reply::RGR_VAL_HDR;


                                uint8_t op = proxy_get_next;
                                m_proxy.m_rdp.send(desc, &op, sizeof(op));

                                return true;
                        }
                }

                return true;
        }

        void
        proxy::rdp_recv_get_reply_func::operator() (int desc, rdp_addr addr,
                                                    rdp_event event)
        {
                switch (event) {
                case ACCEPTED:
                {
                        rdp_recv_get_reply_ptr ptr(new rdp_recv_get_reply);

                        m_proxy.m_rdp_recv_get_reply[desc] = ptr;

                        break;
                }
                case READY2READ:
                {
                        std::map<int, rdp_recv_get_reply_ptr>::iterator it;

                        it = m_proxy.m_rdp_recv_get_reply.find(desc);
                        if (it == m_proxy.m_rdp_recv_get_reply.end()) {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }

                        for (;;) {
                                switch (it->second->m_state) {
                                case rdp_recv_get_reply::RGR_HDR:
                                        if (! read_hdr(desc, it->second))
                                                return;
                                        break;
                                case rdp_recv_get_reply::RGR_VAL_HDR:
                                        if (! read_val_hdr(desc, it->second))
                                                return;
                                        break;
                                case rdp_recv_get_reply::RGR_VAL:
                                        if (! read_val(desc, it->second))
                                                return;
                                        break;
                                }
                        }

                        break;
                }
                default:
                        std::map<int, rdp_recv_get_reply_ptr>::iterator it;

                        it = m_proxy.m_rdp_recv_get_reply.find(desc);
                        if (it == m_proxy.m_rdp_recv_get_reply.end()) {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }

                        close_rdp(desc, it->second);
                }
        }

        void
        proxy::rdp_get_reply_func::read_op(int desc)
        {
                for (;;) {
                        if (m_vset->size() == 0) {
                                m_proxy.m_rdp.close(desc);
                                m_proxy.m_rdp_get_reply.erase(desc);
                                return;
                        }

                        uint8_t op;
                        int     size = sizeof(op);

                        m_proxy.m_rdp.receive(desc, &op, &size);

                        if (size == 0)
                                return;

                        if (op != proxy_get_next) {
                                m_proxy.m_rdp.close(desc);
                                m_proxy.m_rdp_get_reply.erase(desc);
                                return;
                        }

                        msg_proxy_rdp_get_reply_val msg;
                        dht::value_set::iterator it;

                        it = m_vset->begin();

                        memset(&msg, 0, sizeof(msg));

                        msg.valuelen = htons(it->len);

                        m_proxy.m_rdp.send(desc, &msg, sizeof(msg));
                        m_proxy.m_rdp.send(desc, it->value.get(), it->len);

                        m_vset->erase(it);
                }
        }

        void
        proxy::rdp_get_reply_func::operator() (int desc, rdp_addr addr,
                                               rdp_event event)
        {
                switch (event) {
                case CONNECTED:
                {
                        msg_proxy_rdp_get_reply msg;

                        memset(&msg, 0, sizeof(msg));

                        msg.nonce = htonl(m_nonce);

                        if (m_result)
                                msg.flag = proxy_get_success;
                        else
                                msg.flag = proxy_get_fail;

                        m_id->to_binary(msg.id, sizeof(msg.id));

                        m_proxy.m_rdp.send(desc, &msg, sizeof(msg));

                        m_proxy.m_rdp_get_reply[desc] = time(NULL);

                        break;
                }
                case READY2READ:
                {
                        if (! m_result) {
                                m_proxy.m_rdp.close(desc);
                                m_proxy.m_rdp_get_reply.erase(desc);
                                return;
                        }

                        read_op(desc);

                        break;
                }
                default:
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_get_reply.erase(desc);
                }
        }

        void
        proxy::get_reply_func::send_reply_by_rdp(bool result,
                                                 dht::value_set_ptr vset)
        {
                rdp_get_reply_func func(*p_proxy);
                int desc;

                func.m_result = result;
                func.m_vset   = vset;
                func.m_nonce  = nonce;
                func.m_id     = id;

                desc = p_proxy->m_rdp.connect(0, src, proxy_get_reply_port,
                                              func);

                p_proxy->m_rdp_get_reply[desc] = time(NULL);
        }

        void
        proxy::get_reply_func::send_reply(bool result, dht::value_set_ptr vset)
        {
                msg_proxy_get_reply *reply;
                int  size;
                char tmp[1024 * 4];

                reply = (msg_proxy_get_reply*)tmp;

                size = sizeof(*reply) - sizeof(reply->data);

                if (result) {
                        std::map<_id, _addr>::iterator it;
                        _id i;

                        i.id = src;

                        it = p_proxy->m_registered.find(i);
                        if (it == p_proxy->m_registered.end()) {
                                return;
                        }

                        memset(reply, 0, size);
                        reply->nonce = nonce;
                        reply->flag  = 1;
                        reply->total = htons((uint16_t)vset->size());

                        id->to_binary(reply->id, sizeof(reply->id));


                        dht::value_set::iterator it_v;
                        uint16_t index = 1;

                        for (it_v = vset->begin(); it_v != vset->end();
                             ++it_v) {
                                int size2 = size + it_v->len;

                                reply->index = htons(index);

                                memcpy(reply->data, it_v->value.get(),
                                       it_v->len);
                                
                                send_msg(p_proxy->m_udp, &reply->hdr, size2,
                                         type_proxy_get_reply,
                                         it->second.addr, p_proxy->m_id);

                                index++;
                        }
                } else {
                        memset(reply, 0, size);

                        reply->nonce = nonce;
                        reply->flag  = 0;

                        id->to_binary(reply->id, sizeof(reply->id));


                        std::map<_id, _addr>::iterator it;
                        _id i;

                        i.id = src;

                        it = p_proxy->m_registered.find(i);
                        if (it != p_proxy->m_registered.end()) {
                                send_msg(p_proxy->m_udp, &reply->hdr, size,
                                         type_proxy_get_reply,
                                         it->second.addr, p_proxy->m_id);
                        }
                }
        }

        void
        proxy::get_reply_func::operator() (bool result, dht::value_set_ptr vset)
        {
                if (is_rdp) {
                        send_reply_by_rdp(result, vset);
                } else {
                        send_reply(result, vset);
                }
        }

        void
        proxy::recv_get(void *msg, int len)
        {
                msg_proxy_get *getmsg;
                uint16_t keylen;
                int      size;
                id_ptr   src(new uint160_t);
                id_ptr   id(new uint160_t);
                _id      i;

                getmsg = (msg_proxy_get*)msg;


                src->from_binary(getmsg->hdr.src, sizeof(getmsg->hdr.src));
                i.id = src;
                if (m_registered.find(i) == m_registered.end())
                        return;

                keylen = ntohs(getmsg->keylen);

                size = sizeof(*getmsg) - sizeof(getmsg->key) +
                        keylen;

                if (size != len)
                        return;


                get_reply_func func;

                func.id      = id;
                func.src     = src;
                func.nonce   = getmsg->nonce;
                func.p_proxy = this;
                func.is_rdp  = false;

                id->from_binary(getmsg->id, sizeof(getmsg->id));

                m_dht.find_value(*id, getmsg->key, keylen, func);
        }

        void
        proxy::timer_get::operator() ()
        {
                std::map<uint32_t, gd_ptr>::iterator it;

                it = p_proxy->m_getdata.find(nonce);
                if (it == p_proxy->m_getdata.end()) {
                        dht::value_set_ptr p;
                        func(false, p);
                }

                if (it->second->vset->size() > 0) {
                        it->second->func(true, it->second->vset);
                } else {
                        dht::value_set_ptr p;
                        func(false, p);
                }

                if (it->second->is_rdp && it->second->desc != 0) {
                        p_proxy->m_rdp.close(it->second->desc);
                        p_proxy->m_rdp_get.erase(it->second->desc);
                }

                p_proxy->m_getdata.erase(nonce);
        }

        void
        proxy::retry_storing()
        {
                if (! m_is_registered && m_nat.get_state() != node_symmetric)
                        return;


                BOOST_FOREACH(rdp_store_func_ptr func, m_store_data) {
                        int desc;

                        desc = m_rdp.connect(0, m_server.id, proxy_store_port,
                                             *func);
                        m_rdp_store[desc] = time(NULL);
                }

                m_store_data.clear();
        }

        void
        proxy::get_by_rdp(const uint160_t &id, const void *key, uint16_t keylen,
                          dht::callback_find_value func)
        {
                boost::shared_array<char> p_key(new char[keylen]);
                proxy::gd_ptr gdp(new proxy::getdata);
                rdp_get_ptr   p_get(new rdp_get);
                rdp_get_func  getfunc(*this);
                id_ptr        p_id(new uint160_t);
                int           desc;
                uint32_t      nonce;

                for (;;) {
                        nonce = m_rnd();
                        if (m_getdata.find(nonce) == m_getdata.end())
                                break;
                }

                memcpy(p_key.get(), key, keylen);

                *p_id = id;

                p_get->m_id     = p_id;
                p_get->m_key    = p_key;
                p_get->m_keylen = keylen;
                p_get->m_func   = func;
                p_get->m_time   = time(NULL);
                p_get->m_nonce  = nonce;
                p_get->m_data   = gdp;


                desc = m_rdp.connect(0, m_server.id, proxy_get_port, getfunc);


                gdp->key    = p_key;
                gdp->keylen = keylen;
                gdp->func   = func;
                gdp->is_rdp = true;
                gdp->desc   = desc;

                gdp->timeout.p_proxy = this;
                gdp->timeout.nonce   = nonce;
                gdp->timeout.func    = func;


                // start timer
                timeval tval;

                tval.tv_sec  = get_timeout;
                tval.tv_usec = 0;

                m_timer.set_timer(&gdp->timeout, &tval);


                m_getdata[nonce] = gdp;
                m_rdp_get[desc]  = p_get;
        }

        void
        proxy::get(const uint160_t &id, const void *key, uint16_t keylen,
                   dht::callback_find_value func)
        {
                if (! m_is_registered) {
                        if (m_nat.get_state() == node_symmetric)
                                register_node();

                        dht::value_set_ptr p;
                        func(false, p);
                        return;
                }

                if (m_dht.is_use_rdp()) {
                        get_by_rdp(id, key, keylen, func);
                        return;
                }

                boost::shared_array<char> p_key;
                msg_proxy_get *msg;
                gd_ptr   data(new getdata);
                uint32_t nonce;
                int      size;
                char     buf[1024 * 4];

                msg = (msg_proxy_get*)buf;

                size = sizeof(*msg) - sizeof(msg->key) + keylen;
                if (size > (int)sizeof(buf)) {
                        dht::value_set_ptr p;
                        func(false, p);
                        return;
                }

                for (;;) {
                        nonce = m_rnd();
                        if (m_getdata.find(nonce) == m_getdata.end())
                                break;
                }

                p_key = boost::shared_array<char>(new char[keylen]);
                memcpy(p_key.get(), key, keylen);

                data->keylen = keylen;
                data->func   = func;
                data->key    = p_key;

                data->timeout.p_proxy = this;
                data->timeout.nonce   = nonce;
                data->timeout.func    = func;

                m_getdata[nonce] = data;

                // start timer
                timeval tval;

                tval.tv_sec  = get_timeout;
                tval.tv_usec = 0;

                m_timer.set_timer(&data->timeout, &tval);

                // send get
                memset(msg, 0, size);

                msg->nonce  = htonl(nonce);
                msg->keylen = htons(keylen);

                id.to_binary(msg->id, sizeof(msg->id));
                memcpy(msg->key, key, keylen);

                send_msg(m_udp, &msg->hdr, size, type_proxy_get, m_server,
                         m_id);
        }

        void
        proxy::recv_get_reply(void *msg, int len)
        {
                msg_proxy_get_reply *reply;
                uint160_t dst;
                uint32_t  nonce;
                int       valuelen;

                reply = (msg_proxy_get_reply*)msg;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (m_id != dst)
                        return;

                valuelen = len - (sizeof(*reply) - sizeof(reply->data));

                if (valuelen < 0)
                        return;

                nonce = ntohl(reply->nonce);

                
                std::map<uint32_t, gd_ptr>::iterator it;

                it = m_getdata.find(nonce);
                if (it == m_getdata.end())
                        return;

                if (reply->flag > 0) {
                        dht::value_t value;

                        value.value = boost::shared_array<char>(new char[valuelen]);
                        value.len   = valuelen;

                        memcpy(value.value.get(), reply->data, valuelen);

                        it->second->vset->insert(value);
                        it->second->indeces.insert((int)ntohs(reply->index));

                        if (it->second->total == 0) {
                                it->second->total = ntohs(reply->total);
                        }

                        if (it->second->total <= (int)it->second->vset->size()) {
                                it->second->func(true, it->second->vset);

                                m_timer.unset_timer(&it->second->timeout);
                                m_getdata.erase(nonce);
                        }
                } else {
                        dht::value_set_ptr p;
                        it->second->func(false, p);

                        m_timer.unset_timer(&it->second->timeout);
                        m_getdata.erase(nonce);
                }
        }

        void
        proxy::register_node()
        {
                if (m_is_registering)
                        return;

                m_is_registering = true;

                register_func func;

                func.p_proxy = this;

                m_dtun.find_node(m_id, func);
        }

        void
        proxy::send_dgram(const void *msg, int len, id_ptr id)
        {
                cageaddr addr;
                try {
                        addr = m_peers.get_addr(id);
                } catch (std::out_of_range) {
                        if (! m_is_registered) {
                                if (m_nat.get_state() == node_symmetric)
                                        register_node();

                                return;
                        }

                        addr = m_server;
                }

                msg_proxy_dgram *dgram;
                char buf[1024];
                int  size;

                size = sizeof(*dgram) - sizeof(dgram->data) + len;

                if (size > (int)sizeof(buf))
                        return;

                dgram = (msg_proxy_dgram*)buf;

                memcpy(dgram->data, msg, len);

                addr.id = id;

                send_msg(m_udp, &dgram->hdr, size, type_proxy_dgram,
                         addr, m_id);
        }

        void
        proxy::send_dgram(packetbuf_ptr pbuf, id_ptr id, uint8_t type)
        {
                cageaddr addr;
                try {
                        addr = m_peers.get_addr(id);
                } catch (std::out_of_range) {
                        if (! m_is_registered) {
                                if (m_nat.get_state() == node_symmetric)
                                        register_node();

                                return;
                        }

                        addr = m_server;
                }

                msg_hdr *hdr;

                hdr = (msg_hdr*)pbuf->prepend(sizeof(*hdr));
                if (hdr == NULL)
                        return;

                addr.id = id;

                send_msg(m_udp, hdr, pbuf->get_len(), type, addr, m_id);

                pbuf->rm_head(sizeof(*hdr));
        }

        void
        proxy::forward_msg(msg_dgram *data, int size, sockaddr *from)
        {
                std::map<_id, _addr>::iterator it;
                msg_proxy_dgram_forwarded *forwarded;
                uint160_t src;
                char      buf[1024 * 4];
                id_ptr    dst(new uint160_t);
                int       len;
                _id       i;

                dst->from_binary(data->hdr.dst, sizeof(data->hdr.dst));

                i.id = dst;

                it = m_registered.find(i);

                if (it == m_registered.end())
                        return;

                len = size + sizeof(*forwarded) - sizeof(forwarded->data);

                if (len > (int)sizeof(buf))
                        return;

                forwarded = (msg_proxy_dgram_forwarded*)buf;
                memset(forwarded, 0, len);

                if (from->sa_family == PF_INET) {
                        sockaddr_in *in = (sockaddr_in*)from;

                        forwarded->domain  = htons(domain_inet);
                        forwarded->port    = in->sin_port;
                        forwarded->addr[0] = in->sin_addr.s_addr;
                } else if (from->sa_family == PF_INET6) {
                        sockaddr_in6 *in6 = (sockaddr_in6*)from;

                        forwarded->domain  = htons(domain_inet6);
                        forwarded->port    = in6->sin6_port;

                        memcpy(forwarded->addr, in6->sin6_addr.s6_addr,
                               sizeof(in6_addr));
                } else {
                        return;
                }

                memcpy(forwarded->data, data->data, size);

                src.from_binary(data->hdr.src, sizeof(data->hdr.src));

                if (data->hdr.type == type_dgram) {
                        send_msg(m_udp, &forwarded->hdr, len,
                                 type_proxy_dgram_forwarded,
                                 it->second.addr, src);
                } else if (data->hdr.type == type_rdp) {
                        send_msg(m_udp, &forwarded->hdr, len,
                                 type_proxy_rdp_forwarded,
                                 it->second.addr, src);
                }
        }

        void
        proxy::recv_forwarded(packetbuf_ptr pbuf)
        {
                msg_proxy_dgram_forwarded *forwarded;
                id_ptr src(new uint160_t);
                int    size;

                forwarded = (msg_proxy_dgram_forwarded*)pbuf->get_data();

                size = pbuf->get_len() - (sizeof(*forwarded) -
                                          sizeof(forwarded->data));

                if (size <= 0)
                        return;

                src->from_binary(forwarded->hdr.src,
                                 sizeof(forwarded->hdr.src));

                if (forwarded->hdr.type == type_proxy_dgram_forwarded)
                        m_dgram_func(forwarded->data, size, forwarded->hdr.src);
                else if (forwarded->hdr.type == type_proxy_rdp_forwarded) {
                        m_rdp.input_dgram(src, pbuf);
                }

                // send advertise
                uint16_t  domain;
                
                domain = ntohs(forwarded->domain);

                m_advertise.advertise_to(*src, domain, forwarded->port,
                                         forwarded->addr);
        }

        void
        proxy::rdp_get_func::operator() (int desc, rdp_addr addr,
                                         rdp_event event)
        {
                switch (event) {
                case CONNECTED:
                {
                        std::map<int, rdp_get_ptr>::iterator it;

                        it = m_proxy.m_rdp_get.find(desc);
                        if (it == m_proxy.m_rdp_get.end())  {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }


                        msg_proxy_rdp_get msg;

                        memset(&msg, 0, sizeof(msg));

                        it->second->m_id->to_binary(msg.id, sizeof(msg.id));

                        msg.keylen = htons(it->second->m_keylen);
                        msg.nonce  = htonl(it->second->m_nonce);

                        m_proxy.m_rdp.send(desc, &msg, sizeof(msg));
                        m_proxy.m_rdp.send(desc, it->second->m_key.get(),
                                           it->second->m_keylen);

                        break;
                }
                case RESET:
                {
                        std::map<int, rdp_get_ptr>::iterator it;

                        it = m_proxy.m_rdp_get.find(desc);
                        if (it == m_proxy.m_rdp_get.end())  {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }

                        it->second->m_data->desc = 0;

                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_get.erase(desc);
                        break;
                }
                default:
                {
                        m_proxy.m_rdp.close(desc);

                        std::map<int, rdp_get_ptr>::iterator it1;
                        it1 = m_proxy.m_rdp_get.find(desc);
                        if (it1 == m_proxy.m_rdp_get.end()) {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }

                        std::map<uint32_t, gd_ptr>::iterator it2;
                        it2 = m_proxy.m_getdata.find(it1->second->m_nonce);
                        if (it2 != m_proxy.m_getdata.end()) {
                                m_proxy.m_timer.unset_timer(&it2->second->timeout);
                                m_proxy.m_getdata.erase(it2);
                        }


                        dht::value_set_ptr p;
                        it1->second->m_func(false, p);

                        m_proxy.m_rdp_get.erase(it1);

                        break;
                }
                }
        }

        bool
        proxy::rdp_recv_get_func::read_hdr(int desc, rdp_recv_get_ptr ptr)
        {
                msg_proxy_rdp_get msg;
                int size = sizeof(msg);

                m_proxy.m_rdp.receive(desc, &msg, &size);

                if (size != sizeof(msg)) {
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_recv_get.erase(desc);
                        return false;
                }

                ptr->m_keylen = ntohs(msg.keylen);
                ptr->m_nonce  = ntohl(msg.nonce);
                ptr->m_time   = time(NULL);
                ptr->m_state  = rdp_recv_get::RG_KEY;

                boost::shared_array<char> key(new char[ptr->m_keylen]);
                ptr->m_key = key;

                id_ptr id(new uint160_t);
                id->from_binary(msg.id, sizeof(msg.id));

                ptr->m_id = id;

                return true;
        }

        void
        proxy::rdp_recv_get_func::read_key(int desc, rdp_recv_get_ptr ptr)
        {
                for (;;) {
                        int   size = ptr->m_keylen - ptr->m_key_read;
                        char *buf  = &ptr->m_key[ptr->m_key_read];

                        m_proxy.m_rdp.receive(desc, buf, &size);

                        if (size == 0)
                                return;

                        ptr->m_key_read += size;
                        if (ptr->m_key_read == ptr->m_keylen) {
                                get_reply_func func;

                                func.id      = ptr->m_id;
                                func.src     = ptr->m_src;
                                func.nonce   = ptr->m_nonce;
                                func.p_proxy = &m_proxy;
                                func.is_rdp  = true;

                                m_proxy.m_dht.find_value(*ptr->m_id,
                                                         ptr->m_key.get(),
                                                         ptr->m_keylen, func);

                                m_proxy.m_rdp.close(desc);
                                m_proxy.m_rdp_recv_get.erase(desc);
                                return;
                        }
                }
        }

        void
        proxy::rdp_recv_get_func::operator() (int desc, rdp_addr addr,
                                              rdp_event event)
        {
                switch (event) {
                case ACCEPTED:
                {
                        if (! m_proxy.is_registered(addr.did)) {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }

                        rdp_recv_get_ptr ptr(new rdp_recv_get(m_proxy));

                        ptr->m_src = addr.did;

                        m_proxy.m_rdp_recv_get[desc] = ptr;

                        break;
                }
                case READY2READ:
                {
                        std::map<int, rdp_recv_get_ptr>::iterator it;

                        it = m_proxy.m_rdp_recv_get.find(desc);
                        if (it == m_proxy.m_rdp_recv_get.end()) {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }

                        for (;;) {
                                switch (it->second->m_state) {
                                case rdp_recv_get::RG_HDR:
                                        if (! read_hdr(desc, it->second))
                                                return;
                                        break;
                                case rdp_recv_get::RG_KEY:
                                        read_key(desc, it->second);
                                        return;
                                }
                        }

                        break;
                }
                default:
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_recv_get.erase(desc);
                        break;
                }
        }

        void
        proxy::rdp_store_func::operator() (int desc, rdp_addr addr,
                                           rdp_event event)
        {
                switch (event) {
                case CONNECTED:
                {
                        msg_dht_rdp_store msg;

                        memset(&msg, 0, sizeof(msg));

                        m_id->to_binary(msg.id, sizeof(msg.id));

                        msg.keylen   = htons(m_keylen);
                        msg.valuelen = htons(m_valuelen);
                        msg.ttl      = htons(m_ttl);

                        if (m_is_unique)
                                msg.flags = dht_flag_unique;

                        m_proxy.m_rdp.send(desc, &msg, sizeof(msg));
                        m_proxy.m_rdp.send(desc, m_key.get(), m_keylen);
                        m_proxy.m_rdp.send(desc, m_val.get(), m_valuelen);

                        break;
                }
                default:
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_store.erase(desc);
                }
        }

        bool
        proxy::rdp_recv_store_func::read_hdr(int desc, rdp_recv_store_ptr ptr)
        {
                msg_dht_rdp_store msg;
                int size = sizeof(msg);

                m_proxy.m_rdp.receive(desc, &msg, &size);

                if (size != sizeof(msg)) {
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_recv_store.erase(desc);
                        return false;
                }


                id_ptr id(new uint160_t);

                id->from_binary(msg.id, sizeof(msg.id));

                ptr->m_keylen   = ntohs(msg.keylen);
                ptr->m_valuelen = ntohs(msg.valuelen);
                ptr->m_ttl      = ntohs(msg.ttl);
                ptr->m_id       = id;
                ptr->m_time     = time(NULL);
                ptr->m_state    = rdp_recv_store::RS_KEY;

                if (msg.flags & dht_flag_unique)
                        ptr->m_is_unique = true;


                boost::shared_array<char> key(new char[ptr->m_keylen]);
                boost::shared_array<char> val(new char[ptr->m_valuelen]);

                ptr->m_key = key;
                ptr->m_val = val;

                return true;
        }

        bool
        proxy::rdp_recv_store_func::read_key(int desc, rdp_recv_store_ptr ptr)
        {
                for (;;) {
                        int   size = ptr->m_keylen - ptr->m_key_read;
                        char *buf  = &ptr->m_key[ptr->m_key_read];

                        m_proxy.m_rdp.receive(desc, buf, &size);

                        if (size == 0)
                                return false;

                        ptr->m_key_read += size;
                        ptr->m_time      = time(NULL);

                        if (ptr->m_keylen == ptr->m_key_read) {
                                ptr->m_state = rdp_recv_store::RS_VAL;
                                return true;
                        }
                }

                return true;
        }

        void
        proxy::rdp_recv_store_func::read_val(int desc, rdp_recv_store_ptr ptr)
        {
                for (;;) {
                        int   size = ptr->m_valuelen - ptr->m_val_read;
                        char *buf  = &ptr->m_val[ptr->m_val_read];

                        m_proxy.m_rdp.receive(desc, buf, &size);

                        if (size == 0)
                                return;

                        ptr->m_val_read += size;
                        ptr->m_time      = time(NULL);

                        if (ptr->m_valuelen == ptr->m_val_read) {
                                m_proxy.m_rdp.close(desc);
                                m_proxy.m_dht.store(ptr->m_id,
                                                    ptr->m_key,
                                                    ptr->m_keylen,
                                                    ptr->m_val,
                                                    ptr->m_valuelen,
                                                    ptr->m_ttl,
                                                    ptr->m_src,
                                                    ptr->m_is_unique);
                                m_proxy.m_rdp_recv_store.erase(desc);
                                return;
                        }
                }
        }

        void
        proxy::rdp_recv_store_func::operator() (int desc, rdp_addr addr,
                                                rdp_event event)
        {
                switch (event) {
                case ACCEPTED:
                {
                        if (! m_proxy.is_registered(addr.did)) {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }

                        rdp_recv_store_ptr ptr(new rdp_recv_store);

                        ptr->m_time = time(NULL);
                        ptr->m_src  = addr.did;

                        m_proxy.m_rdp_recv_store[desc] = ptr;

                        break;
                }
                case READY2READ:
                {
                        std::map<int, rdp_recv_store_ptr>::iterator it;

                        it = m_proxy.m_rdp_recv_store.find(desc);
                        if (it == m_proxy.m_rdp_recv_store.end()) {
                                m_proxy.m_rdp.close(desc);
                                return;
                        }

                        for (;;) {
                                switch (it->second->m_state) {
                                case rdp_recv_store::RS_HDR:
                                        if (! read_hdr(desc, it->second))
                                                return;
                                        break;
                                case rdp_recv_store::RS_KEY:
                                        if (! read_key(desc, it->second))
                                                return;
                                        break;
                                case rdp_recv_store::RS_VAL:
                                        read_val(desc, it->second);
                                        return;
                                }
                        }

                        break;
                }
                default:
                        m_proxy.m_rdp.close(desc);
                        m_proxy.m_rdp_recv_store.erase(desc);
                }
        }

        void
        proxy::set_callback(dgram::callback func)
        {
                m_dgram_func = func;
        }

        void
        proxy::refresh()
        {
                std::map<_id, _addr>::iterator it;
                time_t now;

                now = time(NULL);

                for (it = m_registered.begin(); it != m_registered.end();) {
                        time_t diff = now - it->second.recv_time;

                        if (diff > register_ttl) {
                                m_registered.erase(it++);
                        } else {
                                ++it;
                        }
                }
        }

        bool
        proxy::is_registered(id_ptr id)
        {
                _id i;

                i.id = id;

                if (m_registered.find(i) == m_registered.end())
                        return false;
                else
                        return true;
        }
}
