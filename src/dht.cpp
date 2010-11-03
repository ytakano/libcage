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

#include "dht.hpp"

#include "ping.hpp"

#include <boost/foreach.hpp>

namespace libcage {
        const int       dht::num_find_node       = 10;
        const int       dht::max_query           = 6;
        const int       dht::query_timeout       = 3;
        const int       dht::restore_interval    = 120;
        const int       dht::slow_timer_interval = 600;
        const int       dht::fast_timer_interval = 60;
        const int       dht::original_put_num    = 3;
        const int       dht::recvd_value_timeout = 3;
        const uint16_t  dht::rdp_store_port      = 100;
        const uint16_t  dht::rdp_get_port        = 101;
        const time_t    dht::rdp_timeout         = 30;

        size_t
        hash_value(const dht::_key &k)
        {
                size_t    h   = 0;
                uint32_t *p   = (uint32_t*)k.key.get();
                int       len = k.keylen / 4;

                for (int i = 0; i < len; i++)
                        boost::hash_combine(h, p[i]);

                for (int j = len * 4; j < k.keylen; j++)
                        boost::hash_combine(h, k.key[j]);

                return h;
        }

        size_t
        hash_value(const dht::stored_data &sdata)
        {
                size_t    h   = 0;
                uint32_t *p   = (uint32_t*)sdata.value.get();
                int       len = sdata.valuelen / 4;

                for (int i = 0; i < len; i++)
                        boost::hash_combine(h, p[i]);

                for (int j = len * 4; j < sdata.valuelen; j++)
                        boost::hash_combine(h, sdata.value[j]);

                return h;
        }

        dht::dht(rand_uint &rnd, rand_real &drnd, const uint160_t &id, timer &t,
                 peers &p, const natdetector &nat, udphandler &udp, dtun &dt,
                 rdp &r) :
                rttable(rnd, id, t, p),
                m_rnd(rnd),
                m_drnd(drnd),
                m_id(id),
                m_timer(t),
                m_peers(p),
                m_nat(nat),
                m_udp(udp),
                m_dtun(dt),
                m_rdp(r),
                m_is_dtun(true),
                m_last_restore(0),
                m_slow_timer_dht(*this),
                m_fast_timer_dht(*this),
                m_join(*this),
                m_sync(*this),
                m_is_use_rdp(true)
        {
                rdp_recv_store_func func_recv(*this);
                rdp_recv_get_func   func_get(*this);


                m_rdp_recv_listen = m_rdp.listen(rdp_store_port, func_recv);
                m_rdp_get_listen  = m_rdp.listen(rdp_get_port, func_get);


                m_mask_bit = 1;
        }

        dht::~dht()
        {
                std::map<int, rdp_recv_store_ptr>::iterator it1;
                std::map<int, time_t>::iterator it2;

                for (it1 = m_rdp_recv_store.begin();
                     it1 != m_rdp_recv_store.end(); ++it1) {
                        m_rdp.close(it1->first);
                }

                for (it2 = m_rdp_store.begin(); it2 != m_rdp_store.end();
                     ++it2) {
                        m_rdp.close(it2->first);
                }


                m_rdp.close(m_rdp_recv_listen);
                m_rdp.close(m_rdp_get_listen);
        }

        void
        no_action(std::vector<cageaddr>& nodes)
        {

        }

        void
        dht::add_sdata(dht::stored_data &sdata, bool is_origin)
        {
                _id  i;
                _key k;

                i.id     = sdata.id;
                k.key    = sdata.key;
                k.keylen = sdata.keylen;


                boost::unordered_map<_id, sdata_map>::iterator it1;

                it1 = m_stored.find(i);
                if (it1 == m_stored.end()) {
                        m_stored[i][k].insert(sdata);
                        return;
                }


                sdata_map::iterator it2;

                it2 = it1->second.find(k);
                if (it2 == it1->second.end()) {
                        m_stored[i][k].insert(sdata);
                        return;
                }


                sdata_set::iterator it3;

                if (sdata.is_unique) {
                        if (it2->second.size() == 0) {
                                it2->second.insert(sdata);
                                return;
                        }

                        if (it2->second.size() > 1)
                                return;

                        it3 = it2->second.begin();
                        if (*it3->src == *sdata.src && it3->is_unique) {
                                if (*it3 == sdata) {
                                        it3->ttl         = sdata.ttl;
                                        it3->original    = sdata.original;
                                        it3->stored_time = sdata.stored_time;
                                } else {
                                        it2->second.erase(it3);
                                        it2->second.insert(sdata);
                                }
                        }

                        return;
                }

                it3 = it2->second.begin();
                if (it2->second.size() == 1 && it3->is_unique) {
                        if (*it3->src != *sdata.src || ! sdata.is_unique)
                                return;

                        if (*it3 == sdata) {
                                it3->ttl         = sdata.ttl;
                                it3->original    = sdata.original;
                                it3->stored_time = sdata.stored_time;
                        } else {
                                it2->second.erase(it3);
                                it2->second.insert(sdata);
                        }

                        return;
                }

                it3 = it2->second.find(sdata);
                if (it3 == it2->second.end()) {
                        it2->second.insert(sdata);
                        return;
                }

                if (*it3->src != *sdata.src)
                        return;

                it3->ttl         = sdata.ttl;
                it3->stored_time = sdata.stored_time;

                if (is_origin)
                        it3->original = sdata.original;
        }

        void
        dht::erase_sdata(dht::stored_data &sdata)
        {
                _id  i;
                _key k;

                i.id     = sdata.id;
                k.key    = sdata.key;
                k.keylen = sdata.keylen;


                boost::unordered_map<_id, sdata_map>::iterator it1;

                it1 = m_stored.find(i);
                if (it1 == m_stored.end())
                        return;


                sdata_map::iterator it2;

                it2 = it1->second.find(k);
                if (it2 == it1->second.end())
                        return;


                sdata_set::iterator it3;

                it3 = it2->second.find(sdata);
                if (*it3->src != *sdata.src)
                        return;

                it2->second.erase(sdata);

                if (it2->second.size() == 0)
                        it1->second.erase(it2);

                if (it1->second.size() == 0)
                        m_stored.erase(it1);
        }

        void
        dht::insert2recvd_sdata(stored_data &sdata, id_ptr id)
        {
                _id  i;
                _key k;

                i.id     = sdata.id;
                k.key    = sdata.key;
                k.keylen = sdata.keylen;


                boost::unordered_map<_id, sdata_map>::iterator it1;

                it1 = m_stored.find(i);
                if (it1 == m_stored.end())
                        return;


                sdata_map::iterator it2;

                it2 = it1->second.find(k);
                if (it2 == it1->second.end())
                        return;


                sdata_set::iterator it3;

                it3 = it2->second.find(sdata);
                if (it3 == it2->second.end())
                        return;


                _id src;

                src.id = id;

                it3->recvd.insert(src);
        }

        int
        dht::dec_origin_sdata(dht::stored_data &sdata)
        {
                _id  i;
                _key k;

                i.id     = sdata.id;
                k.key    = sdata.key;
                k.keylen = sdata.keylen;


                boost::unordered_map<_id, sdata_map>::iterator it1;

                it1 = m_stored.find(i);
                if (it1 == m_stored.end()) {
                        m_stored[i][k].insert(sdata);
                        return -1;
                }


                sdata_map::iterator it2;

                it2 = it1->second.find(k);
                if (it2 == it1->second.end()) {
                        m_stored[i][k].insert(sdata);
                        return -1;
                }


                sdata_set::iterator it3;

                it3 = it2->second.find(sdata);
                if (it3 == it2->second.end()) {
                        it2->second.insert(sdata);
                        return -1;
                }

                if (it3->original > 0)
                        it3->original--;

                return it3->original;
        }

        void
        dht::maintain()
        {
                uint160_t bit = 1;
                uint160_t mask1, mask2;

                mask1 = ~(bit << (160 - m_mask_bit));
                mask2 = ~(bit << (160 - m_mask_bit - 1));

                find_node(m_id & mask1, no_action);
                find_node(m_id & mask2, no_action);

                m_mask_bit += 2;
                if (m_mask_bit > 20)
                        m_mask_bit = 1;
        }

        bool
        dht::rdp_get_func::read_hdr(int desc)
        {
                msg_dht_rdp_get_reply msg;
                int size = sizeof(msg);

                m_dht.m_rdp.receive(desc, &msg, &size);

                if (size == 0)
                        return false;

                if (size != sizeof(msg)) {
                        close_rdp(desc);
                        return false;
                }

                m_query->rdp_state = query::QUERY_VAL;
                m_query->rdp_time  = time(NULL);
                m_query->vallen    = ntohs(msg.valuelen);
                m_query->val_read  = 0;

                if (m_query->vallen == 0) {
                        close_rdp(desc);
                        return false;
                }

                boost::shared_array<char> val(new char[m_query->vallen]);
                m_query->val = val;

                return true;
        }

        bool
        dht::rdp_get_func::read_val(int desc)
        {
                int   size = m_query->vallen - m_query->val_read;
                char *buf  = &m_query->val[m_query->val_read];

                m_dht.m_rdp.receive(desc, buf, &size);

                if (size == 0)
                        return false;

                m_query->val_read += size;

                if (m_query->vallen == m_query->val_read) {
                        value_t val;

                        val.value = m_query->val;
                        val.len   = m_query->vallen;

                        m_query->vset->insert(val);

                        m_query->rdp_state = query::QUERY_HDR;

                        uint8_t op = dht_get_next;
                        m_dht.m_rdp.send(desc, &op, sizeof(op));
                }

                m_query->rdp_time = time(NULL);

                return true;
        }

        void
        dht::rdp_get_func::close_rdp(int desc)
        {
                m_dht.m_rdp.close(desc);

                if (m_query->vset->size() > 0) {
                        m_dht.recvd_value(m_query);
                        return;
                }

                if (m_query->ids.size() > 0) {
                        id_ptr id = m_query->ids.front();

                        m_query->ids.pop();
                        m_query->rdp_time = time(NULL);
                        m_query->rdp_desc = m_dht.m_rdp.connect(rdp_get_port,
                                                                id, 0, *this);
                } else {
                        m_query->is_rdp_con = false;
                        m_dht.send_find(m_query);
                }
        }

        void
        dht::rdp_get_func::operator() (int desc, rdp_addr addr,
                                       rdp_event event)
        {
                switch (event) {
                case CONNECTED:
                {
                        m_query->is_rdp_con = true;
                        m_query->rdp_desc   = desc;
                        m_query->rdp_time   = time(NULL);
                        m_query->rdp_state  = query::QUERY_HDR;

                        msg_dht_rdp_get get;

                        memset(&get, 0, sizeof(get));
                        m_query->dst->to_binary(get.id, sizeof(get.id));
                        get.keylen = htons(m_query->keylen);

                        m_dht.m_rdp.send(desc, &get, sizeof(get));
                        m_dht.m_rdp.send(desc, m_query->key.get(),
                                         m_query->keylen);

                        uint8_t op = dht_get_next;
                        m_dht.m_rdp.send(desc, &op, sizeof(op));

                        break;
                }
                case READY2READ:
                {
                        for (;;) {
                                switch (m_query->rdp_state) {
                                case query::QUERY_HDR:
                                        if (! read_hdr(desc))
                                                return;
                                        break;
                                case query::QUERY_VAL:
                                        if (! read_val(desc))
                                                return;
                                        break;
                                }
                        }

                        break;
                }
                default:
                        close_rdp(desc);
                }
        }

        void
        dht::rdp_recv_get_func::operator() (int desc, rdp_addr addr,
                                            rdp_event event)
        {
                switch (event) {
                case ACCEPTED:
                {
                        rdp_recv_get_ptr rget(new rdp_recv_get(m_dht));

                        m_dht.m_rdp_recv_get[desc] = rget;

                        break;
                }
                case READY2READ:
                {
                        std::map<int, rdp_recv_get_ptr>::iterator it;

                        it = m_dht.m_rdp_recv_get.find(desc);
                        if (it == m_dht.m_rdp_recv_get.end()) {
                                m_dht.m_rdp.close(desc);
                        }

                        for (;;) {
                                switch (it->second->m_state) {
                                case rdp_recv_get::RGET_HDR:
                                        if (! read_hdr(desc, it->second))
                                                return;
                                        break;
                                case rdp_recv_get::RGET_KEY:
                                        if (! read_key(desc, it->second))
                                                return;
                                        break;
                                case rdp_recv_get::RGET_VAL:
                                        read_op(desc, it->second);
                                        return;
                                case rdp_recv_get::RGET_END:
                                        m_dht.m_rdp.close(desc);
                                        m_dht.m_rdp_recv_get.erase(desc);
                                        return;
                                }
                        }

                        break;
                }
                default:
                        m_dht.m_rdp.close(desc);
                        m_dht.m_rdp_recv_get.erase(desc);
                }
        }

        void
        dht::rdp_recv_get_func::read_op(int desc, rdp_recv_get_ptr rget)
        {
                for (;;) {
                        uint8_t op;
                        int     size = sizeof(op);

                        m_dht.m_rdp.receive(desc, &op, &size);

                        if (size == 0)
                                return;

                        if (op != dht_get_next) {
                                m_dht.m_rdp.close(desc);
                                m_dht.m_rdp_recv_get.erase(desc);
                                return;
                        }

                        rget->m_time = time(NULL);


                        msg_dht_rdp_get_reply msg;
                        stored_data data;

                        memset(&msg, 0, sizeof(msg));

                        if (rget->m_data.size() == 0) {
                                m_dht.m_rdp.send(desc, &msg, sizeof(msg));
                                rget->m_state = rdp_recv_get::RGET_END;
                        } else {
                                data = rget->m_data.front();
                                rget->m_data.pop();

                                msg.valuelen = htons(data.valuelen);
                                m_dht.m_rdp.send(desc, &msg, sizeof(msg));
                                m_dht.m_rdp.send(desc, data.value.get(),
                                                 data.valuelen);
                        }

                }
        }

        void
        dht::rdp_recv_get_func::read_val(rdp_recv_get_ptr rget)
        {
                boost::unordered_map<_id, sdata_map>::iterator it1;
                _id i;

                i.id = rget->m_id;

                it1 = m_dht.m_stored.find(i);
                if (it1 == m_dht.m_stored.end())
                        return;

                sdata_map::iterator it2;
                _key k;

                k.key    = rget->m_key;
                k.keylen = rget->m_keylen;
                
                it2 = it1->second.find(k);
                if (it2 == it1->second.end())
                        return;


                sdata_set::iterator it3;
                time_t now = time(NULL);
                for (it3 = it2->second.begin(); it3 != it2->second.end(); ) {
                        time_t diff = now - it3->stored_time;
                        if (diff > it3->ttl) {
                                it2->second.erase(it3++);
                                continue;
                        }

                        rget->m_data.push(*it3);
                        ++it3;
                }

                if (it2->second.size() == 0)
                        it1->second.erase(it2);

                if (it1->second.size() == 0)
                        m_dht.m_stored.erase(it1);
        }

        bool
        dht::rdp_recv_get_func::read_key(int desc, rdp_recv_get_ptr rget)
        {
                for (;;) {
                        int   size = rget->m_keylen - rget->m_key_read;
                        char *buf  = &rget->m_key[rget->m_key_read];
                        m_dht.m_rdp.receive(desc, buf, &size);

                        if (size == 0)
                                return false;

                        rget->m_key_read += size;
                        if (rget->m_keylen == rget->m_key_read) {
                                rget->m_state = rdp_recv_get::RGET_VAL;
                                read_val(rget);
                                return true;
                        }
                }

                return true;
        }

        bool
        dht::rdp_recv_get_func::read_hdr(int desc, rdp_recv_get_ptr rget)
        {
                msg_dht_rdp_get msg;
                int size = sizeof(msg);

                m_dht.m_rdp.receive(desc, &msg, &size);

                if (size != sizeof(msg)) {
                        m_dht.m_rdp.close(desc);
                        m_dht.m_rdp_recv_get.erase(desc);
                        return false;
                }

                id_ptr id(new uint160_t);

                id->from_binary(msg.id, sizeof(msg.id));

                rget->m_time   = time(NULL);
                rget->m_state  = rdp_recv_get::RGET_KEY;
                rget->m_id     = id;
                rget->m_keylen = ntohs(msg.keylen);

                boost::shared_array<char> key(new char[rget->m_keylen]);
                rget->m_key = key;

                return true;
        }

        bool
        dht::rdp_recv_store_func::read_hdr(int desc,
                                           dht::rdp_recv_store_func::it_rcvs it)
        {
                // read header
                msg_dht_rdp_store msg;
                int size = sizeof(msg);

                m_dht.m_rdp.receive(desc, &msg, &size);
                if (size != (int)sizeof(msg)) {
                        m_dht.m_rdp_recv_store.erase(desc);
                        m_dht.m_rdp.close(desc);
                        return -1;
                }

                it->second->keylen   = ntohs(msg.keylen);
                it->second->valuelen = ntohs(msg.valuelen);
                if (it->second->keylen == 0 ||
                    it->second->valuelen == 0) {
                        m_dht.m_rdp_recv_store.erase(desc);
                        m_dht.m_rdp.close(desc);
                        return false;
                }

                id_ptr id(new uint160_t);
                id_ptr src(new uint160_t);

                id->from_binary(msg.id, sizeof(msg.id));
                src->from_binary(msg.from, sizeof(msg.from));

                it->second->ttl         = ntohs(msg.ttl);
                it->second->id          = id;
                it->second->src         = src;
                it->second->last_time   = time(NULL);
                it->second->is_hdr_read = true;

                boost::shared_array<char> key(new char[it->second->keylen]);
                boost::shared_array<char> val(new char[it->second->valuelen]);
                it->second->key   = key;
                it->second->value = val;

                if (msg.flags & dht_flag_unique)
                        it->second->is_unique = true;

                return true;
        }

        bool
        dht::rdp_recv_store_func::read_body(int desc,
                                            dht::rdp_recv_store_func::it_rcvs it)
        {
                if (it->second->key_read < it->second->keylen) {
                        int size  = it->second->keylen - it->second->key_read;
                        char *buf = &it->second->key[it->second->key_read];
                        m_dht.m_rdp.receive(desc, buf, &size);

                        if (size == 0)
                                return false;

                        it->second->key_read  += size;
                        it->second->last_time  = time(NULL);
                } else {
                        int size  = it->second->valuelen - it->second->val_read;
                        char *buf = &it->second->value[it->second->val_read];
                        m_dht.m_rdp.receive(desc, buf, &size);

                        if (size == 0)
                                return false;

                        it->second->val_read  += size;
                        it->second->last_time  = time(NULL);

                        if (it->second->valuelen == it->second->val_read) {
                                it->second->store2local();

                                m_dht.m_rdp_recv_store.erase(desc);
                                m_dht.m_rdp.close(desc);

                                return false;
                        }
                }

                return true;
        }

        void
        dht::rdp_recv_store_func::operator() (int desc, rdp_addr addr,
                                              rdp_event event)
        {
                switch (event) {
                case ACCEPTED:
                {
                        rdp_recv_store_ptr rs(new rdp_recv_store(&m_dht,
                                                                 addr.did));

                        m_dht.m_rdp_recv_store[desc] = rs;

                        break;
                }
                case READY2READ:
                {
                        it_rcvs it;
                        it = m_dht.m_rdp_recv_store.find(desc);

                        if (it == m_dht.m_rdp_recv_store.end())
                                return;

                        for (;;) {
                                if (it->second->is_hdr_read) {
                                        if (! read_body(desc, it))
                                                return;
                                } else {
                                        if (! read_hdr(desc, it))
                                                return;
                                }
                        }

                        break;
                }
                default:
                        m_dht.m_rdp_recv_store.erase(desc);
                        m_dht.m_rdp.close(desc);
                        break;
                }
        }

        void
        dht::rdp_recv_store::store2local()
        {
                stored_data data;

                data.value       = value;
                data.valuelen    = valuelen;
                data.key         = key;
                data.keylen      = keylen;
                data.ttl         = ttl;
                data.stored_time = time(NULL);
                data.id          = id;
                data.src         = src;
                data.original    = 0;
                data.is_unique   = is_unique;

                if (ttl == 0) {
                        p_dht->erase_sdata(data);
                        return;
                }

                p_dht->add_sdata(data, false);
                p_dht->insert2recvd_sdata(data, src);
        }

        void
        dht::rdp_store_func::operator() (int desc, rdp_addr addr,
                                         rdp_event event)
        {
                switch (event) {
                case CONNECTED:
                {
                        msg_dht_rdp_store msg;

                        memset(&msg, 0, sizeof(msg));

                        id->to_binary(&msg.id, sizeof(msg.id));
                        from->to_binary(&msg.from, sizeof(msg.from));

                        msg.keylen   = ntohs(keylen);
                        msg.valuelen = ntohs(valuelen);
                        msg.ttl      = ntohs(ttl);

                        if (is_unique)
                                msg.flags |= dht_flag_unique;

                        p_dht->m_rdp.send(desc, &msg, sizeof(msg));
                        p_dht->m_rdp.send(desc, key.get(), keylen);
                        p_dht->m_rdp.send(desc, value.get(), valuelen);

                        uint160_t dist;
                        dist = *id ^ *addr.did;
                        break;
                }
                case RESET:
                {
                        stored_data sdata;

                        sdata.value    = value;
                        sdata.valuelen = valuelen;
                        sdata.key      = key;
                        sdata.keylen   = keylen;
                        sdata.id       = id;

                        p_dht->insert2recvd_sdata(sdata, addr.did);
                        p_dht->m_rdp_store.erase(desc);
                        p_dht->m_rdp.close(desc);
                        break;
                }
                default:
                        p_dht->m_rdp_store.erase(desc);
                        p_dht->m_rdp.close(desc);
                        break;
                }
        }

        void
        dht::ping_func::operator() (bool result, cageaddr &addr)
        {
                if (result) {
                        send_ping_tmpl<msg_dht_ping>(addr, nonce, type_dht_ping,
                                                     p_dht->m_id,
                                                     p_dht->m_udp);
                }
        }

        void
        dht::send_ping(cageaddr &dst, uint32_t nonce)
        {
                try {
                        if (m_is_dtun) {
                                node_state state = m_nat.get_state();
                                if (state == node_symmetric ||
                                    state == node_undefined ||
                                    state == node_nat)
                                        return;

                                m_peers.get_addr(dst.id);
                        }

                        send_ping_tmpl<msg_dht_ping>(dst, nonce, type_dht_ping,
                                                     m_id, m_udp);
                } catch (std::out_of_range) {
                        ping_func func;

                        func.dst   = dst;
                        func.nonce = nonce;
                        func.p_dht = this;

                        m_dtun.request(*dst.id, func);
                }
        }

        void
        dht::recv_ping(void *msg, sockaddr *from, int fromlen)
        {
                recv_ping_tmpl<msg_dht_ping,
                        msg_dht_ping_reply>(msg, from, fromlen,
                                            type_dht_ping_reply,
                                            m_id, m_udp, m_peers);
        }

        void
        dht::recv_ping_reply(void *msg, sockaddr *from, int fromlen)
        {
                recv_ping_reply_tmpl<msg_dht_ping_reply>(msg, from, fromlen,
                                                         m_id, m_peers, this);
        }

        void
        dht::find_nv(const uint160_t &dst, callback_func func,
                     bool is_find_value, const void *key = NULL, int keylen = 0)
        {
                query_ptr q(new query);

                lookup(dst, num_find_node, q->nodes);

                if (q->nodes.size() == 0) {
                        if (is_find_value) {
                                callback_find_value f;
                                value_set_ptr p;
                                f = boost::get<callback_find_value>(func);
                                f(false, p);
                        } else {
                                callback_find_node f;
                                f = boost::get<callback_find_node>(func);
                                f(q->nodes);
                        }
                        return;
                }

                id_ptr p_dst(new uint160_t);
                *p_dst = dst;

                q->dst           = p_dst;
                q->num_query     = 0;
                q->is_find_value = is_find_value;
                q->func          = func;

                if (is_find_value) {
                        q->key = boost::shared_array<char>(new char[keylen]);
                        q->keylen = keylen;

                        memcpy(q->key.get(), key, keylen);
                }

                if (is_find_value) {
                        q->key    = boost::shared_array<char>(new char[keylen]);
                        q->keylen = keylen;
                        memcpy(q->key.get(), key, keylen);
                }

                // add my id
                _id i;
                i.id  = id_ptr(new uint160_t(m_id));

                q->sent.insert(i);


                uint32_t nonce;
                do {
                        nonce = m_rnd();
                } while (m_query.find(nonce) != m_query.end());

                q->nonce = nonce;
                m_query[nonce] = q;

                send_find(q);
        }

        void
        dht::find_node(const uint160_t &dst, callback_find_node func)
        {
                node_state state = m_nat.get_state();
                if (state == node_symmetric || state == node_undefined ||
                    state == node_nat) {
                        std::vector<cageaddr> nodes;
                        func(nodes);
                        return;
                }

                find_nv(dst, func, false);
        }

        void
        dht::send_find(query_ptr q)
        {
                if (m_is_use_rdp && q->is_find_value && q->is_rdp_con)
                        return;

                BOOST_FOREACH(cageaddr &addr, q->nodes) {
                        if (q->num_query >= max_query) {
                                break;
                        }

                        _id i;
                        i.id = addr.id;

                        if (q->sent.find(i) != q->sent.end()) {
                                continue;
                        }

                        // start timer
                        timer_query_ptr t(new timer_query);
                        timeval         tval;

                        t->nonce = q->nonce;
                        t->id    = i;
                        t->p_dht = this;

                        tval.tv_sec  = query_timeout;
                        tval.tv_usec = 0;

                        q->timers[i] = t;
                        q->sent.insert(i);

                        m_timer.set_timer(t.get(), &tval);


                        // send find node
                        if (q->is_find_value) {
                                send_find_value(addr, q);
                        } else {
                                send_find_node(addr, q);
                        }

                        q->num_query++;
                }

                if (q->num_query == 0) {
                        // call callback functions
                        if (q->is_find_value) {
                                cageaddr addr1, addr2;
                                callback_find_value func;
                                value_set_ptr p;
                                func = boost::get<callback_find_value>(q->func);
                                func(false, p);
                        } else {
                                callback_find_node func;
                                func = boost::get<callback_find_node>(q->func);
                                func(q->nodes);
                        }

                        remove_query(q);
                }
        }

        void
        dht::remove_query(query_ptr q)
        {
                // stop all timers
                std::map<_id, timer_query_ptr>::iterator it;
                for (it = q->timers.begin(); it != q->timers.end(); ++it) 
                        m_timer.unset_timer(it->second.get());

                if (q->is_timer_recvd_started)
                        m_timer.unset_timer(q->timer_recvd.get());

                // remove query
                m_query.erase(q->nonce);
        }

        void
        dht::find_node_func::operator() (bool result, cageaddr &addr)
        {
                if (! result) {
                        return;
                }

                msg_dht_find_node msg;

                memset(&msg, 0, sizeof(msg));

                msg.nonce  = htonl(nonce);
                msg.domain = htons(addr.domain);

                dst->to_binary(msg.id, sizeof(msg.id));

                send_msg(p_dht->m_udp, &msg.hdr, sizeof(msg),
                         type_dht_find_node, addr, p_dht->m_id);
        }

        void
        dht::send_find_node(cageaddr &dst, query_ptr q)
        {
                try {
                        if (m_is_dtun && ! dst.id->is_zero())
                                m_peers.get_addr(dst.id);

                        msg_dht_find_node msg;

                        memset(&msg, 0, sizeof(msg));

                        msg.nonce  = htonl(q->nonce);
                        msg.domain = htons(dst.domain);

                        q->dst->to_binary(msg.id, sizeof(msg.id));

                        send_msg(m_udp, &msg.hdr, sizeof(msg),
                                 type_dht_find_node, dst, m_id);
                } catch (std::out_of_range) {
                        find_node_func func;

                        func.dst   = q->dst;
                        func.nonce = q->nonce;
                        func.p_dht = this;

                        m_dtun.request(*dst.id, func);
                }
        }

        void
        dht::timer_query::operator() ()
        {
                query_ptr       q = p_dht->m_query[nonce];
                timer_query_ptr t = q->timers[id];
                uint160_t zero;

                q->sent.insert(id);
                q->num_query--;
                q->timers.erase(id);

                zero.fill_zero();

                if (*id.id != zero) {
                        std::vector<cageaddr> tmp;
                        
                        tmp = q->nodes;
                        q->nodes.clear();
                        BOOST_FOREACH(cageaddr &addr, tmp) {
                                if (*addr.id != *id.id)
                                        q->nodes.push_back(addr);
                        }

                        p_dht->m_peers.add_timeout(id.id);
                        p_dht->remove(*id.id);
                }

                // send find node
                p_dht->send_find(q);
        }

        void
        dht::recv_find_node(void *msg, sockaddr *from)
        {
                msg_dht_find_node_reply *reply;
                msg_dht_find_node       *req;
                std::vector<cageaddr>    nodes;
                uint160_t dst;
                uint160_t zero;
                uint160_t id;
                uint16_t  domain;
                cageaddr  addr;
                char      buf[1024 * 2];
                int       len = 0;

                req = (msg_dht_find_node*)msg;

                zero.fill_zero();
                dst.from_binary(req->hdr.dst, sizeof(req->hdr.dst));
                if (dst != m_id && dst != zero)
                        return;

                domain = ntohs(req->domain);
                if (domain != m_udp.get_domain())
                        return;

                addr = new_cageaddr(&req->hdr, from);

                // add to rttable and request cache
                m_peers.add_node(addr);
                add(addr);

                // lookup
                id.from_binary(req->id, sizeof(req->id));
                lookup(id, num_find_node, nodes);

                // send reply
                memset(buf, 0, sizeof(buf));
                reply = (msg_dht_find_node_reply*)buf;

                reply->nonce  = req->nonce;
                reply->domain = req->domain;
                reply->num    = nodes.size();

                memcpy(reply->id, req->id, sizeof(reply->id));

                if (domain == domain_inet) {
                        msg_inet *min;

                        len = sizeof(msg_inet) * nodes.size() +
                                sizeof(*reply) - sizeof(reply->addrs);

                        min   = (msg_inet*)reply->addrs;
                        write_nodes_inet(min, nodes);
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;

                        len = sizeof(msg_inet6) * nodes.size() +
                                sizeof(*reply) - sizeof(reply->addrs);

                        min6  = (msg_inet6*)reply->addrs;
                        write_nodes_inet6(min6, nodes);
                } else {
                        return;
                }

                send_msg(m_udp, &reply->hdr, len, type_dht_find_node_reply,
                         addr, m_id);
        }

        void
        dht::recv_find_node_reply(void *msg, int len, sockaddr *from)
        {
                msg_dht_find_node_reply *reply;
                std::vector<cageaddr>    nodes;
                query_ptr q;
                uint160_t dst, id;
                cageaddr  addr;
                uint32_t  nonce;
                uint16_t  domain;
                int       size;
                _id       c_id;


                reply = (msg_dht_find_node_reply*)msg;

                nonce = ntohl(reply->nonce);
                if (m_query.find(nonce) == m_query.end())
                        return;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (dst != m_id)
                        return;

                id.from_binary(reply->id, sizeof(reply->id));
                q = m_query[nonce];

                if (*q->dst != id)
                        return;

                if (q->is_find_value)
                        return;

                addr = new_cageaddr(&reply->hdr, from);

                // add rttable and request cache
                m_peers.add_node(addr);
                add(addr);

                // stop timer
                c_id.id = addr.id;
                if (q->timers.find(c_id) == q->timers.end()) {
                        timer_query_ptr t;
                        id_ptr          zero(new uint160_t);

                        zero->fill_zero();
                        c_id.id = zero;
                        if (q->timers.find(c_id) == q->timers.end())
                                return;
                        
                        t = q->timers[c_id];
                        m_timer.unset_timer(t.get());
                        q->timers.erase(c_id);
                } else {
                        timer_query_ptr t;
                        t = q->timers[c_id];
                        m_timer.unset_timer(t.get());
                        q->timers.erase(c_id);
                }

                // read nodes
                domain = ntohs(reply->domain);
                if (domain == domain_inet) {
                        msg_inet *min;
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                sizeof(*min) * reply->num;

                        if (size != len)
                                return;

                        min = (msg_inet*)reply->addrs;

                        read_nodes_inet(min, reply->num, nodes, from, m_peers);
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;
                        size = sizeof(*reply) - sizeof(reply->addrs) +
                                sizeof(*min6) * reply->num;

                        if (size != len)
                                return;

                        min6 = (msg_inet6*)reply->addrs;

                        read_nodes_inet6(min6, reply->num, nodes, from,
                                         m_peers);
                } else {
                        return;
                }

                _id i;

                i.id = addr.id;

                q->sent.insert(i);
                q->num_query--;

                // sort
                compare cmp;
                cmp.m_id = &id;
                std::sort(nodes.begin(), nodes.end(), cmp);

                // merge
                std::vector<cageaddr> tmp;

                tmp = q->nodes;
                q->nodes.clear();

                merge_nodes(*q->dst, q->nodes, tmp, nodes, num_find_node);

                // send
                send_find(q);
        }

        void
        dht::find_node(std::string host, int port, callback_find_node func)
        {
                node_state state = m_nat.get_state();
                if (state == node_symmetric || state == node_undefined ||
                    state == node_nat)
                        return;


                sockaddr_storage saddr;

                if (! m_udp.get_sockaddr(&saddr, host, port))
                        return;

                find_node((sockaddr*)&saddr, func);
        }

        void
        dht::find_node(sockaddr *saddr, callback_find_node func)
        {
                node_state state = m_nat.get_state();
                if (state == node_symmetric || state == node_undefined ||
                    state == node_nat)
                        return;


                // initialize query
                query_ptr q(new query);
                id_ptr    dst(new uint160_t);

                *dst = m_id;

                q->dst           = dst;
                q->num_query     = 1;
                q->is_find_value = false;
                q->func          = func;

                // add my id
                _id i;
                i.id = id_ptr(new uint160_t);
                *i.id = m_id;

                q->sent.insert(i);

                uint32_t nonce;
                do {
                        nonce = m_rnd();
                } while (m_query.find(nonce) != m_query.end());

                q->nonce = nonce;
                m_query[nonce] = q;


                // start timer
                timer_query_ptr t(new timer_query);
                timeval   tval;
                id_ptr    zero(new uint160_t);
                _id       zero_id;

                zero->fill_zero();
                zero_id.id = zero;

                t->nonce = q->nonce;
                t->id    = zero_id;
                t->p_dht = this;

                tval.tv_sec  = query_timeout;
                tval.tv_usec = 0;

                q->timers[zero_id] = t;

                m_timer.set_timer(t.get(), &tval);


                // send find node
                cageaddr addr;
                addr.id = zero;

                addr.domain = m_udp.get_domain();
                if (addr.domain == domain_inet) {
                        in_ptr in(new sockaddr_in);
                        memcpy(in.get(), saddr, sizeof(sockaddr_in));
                        addr.saddr = in;
                } else if (addr.domain == domain_inet6) {
                        in6_ptr in6(new sockaddr_in6);
                        memcpy(in6.get(), saddr, sizeof(sockaddr_in6));
                        addr.saddr = in6;
                }

                send_find_node(addr, q);
        }

        void
        dht::set_enabled_dtun(bool flag)
        {
                m_is_dtun = flag;
        }

        void
        dht::set_enabled_rdp(bool flag)
        {
                m_is_use_rdp = flag;
        }

        void
        dht::store(id_ptr id, boost::shared_array<char> key, uint16_t keylen,
                   boost::shared_array<char> value, uint16_t valuelen,
                   uint16_t ttl, id_ptr from, bool is_unique)
        {
                // store to dht network
                store_func func;

                func.key       = key;
                func.value     = value;
                func.id        = id;
                func.keylen    = keylen;
                func.valuelen  = valuelen;
                func.ttl       = ttl;
                func.is_unique = is_unique;
                func.p_dht     = this;
                func.from      = from;

                find_node(*id, func);


                // store to local
                stored_data data;

                data.key         = func.key;
                data.value       = func.value;
                data.keylen      = keylen;
                data.valuelen    = valuelen;
                data.ttl         = ttl;
                data.stored_time = time(NULL);
                data.id          = func.id;
                data.original    = original_put_num;
                data.src         = from;
                data.is_unique   = is_unique;

                if (ttl == 0) {
                        erase_sdata(data);
                } else {
                        add_sdata(data, true);
                }
        }

        void
        dht::store(const uint160_t &id, const void *key, uint16_t keylen,
                   const void *value, uint16_t valuelen, uint16_t ttl,
                   bool is_unique)
        {
                id_ptr     p_id(new uint160_t);
                id_ptr     p_from(new uint160_t(m_id));
                boost::shared_array<char> p_key(new char[keylen]);
                boost::shared_array<char> p_val(new char[valuelen]);

                *p_id = id;

                memcpy(p_key.get(), key, keylen);
                memcpy(p_val.get(), value, valuelen);

                store(p_id, p_key, keylen, p_val, valuelen, ttl, p_from,
                      is_unique);
        }

        bool
        dht::store_func::store_by_udp(std::vector<cageaddr>& nodes)
        {
                msg_dht_store *msg;
                int            size;
                char           buf[1024 * 2];
                char          *p_key, *p_value;

                size = sizeof(*msg) - sizeof(msg->data) + keylen + valuelen;

                if (size > (int)sizeof(buf))
                        return false;

                msg = (msg_dht_store*)buf;

                memset(msg, 0, sizeof(*msg));

                msg->keylen   = htons(keylen);
                msg->valuelen = htons(valuelen);
                msg->ttl      = htons(ttl);

                if (is_unique)
                        msg->flags |= dht_flag_unique;


                id->to_binary(msg->id, sizeof(msg->id));
                p_dht->m_id.to_binary(msg->from, sizeof(msg->from));

                p_key   = (char*)msg->data;
                p_value = p_key + keylen;

                memcpy(p_key, key.get(), keylen);
                memcpy(p_value, value.get(), valuelen);

                // send store
                bool me = false;
                BOOST_FOREACH(cageaddr &addr, nodes) {
                        if (*addr.id == p_dht->m_id) {
                                me = true;
                                continue;
                        }

                        send_msg(p_dht->m_udp, &msg->hdr, size,
                                 type_dht_store, addr, p_dht->m_id);
                }

                return me;
        }

        bool
        dht::store_func::store_by_rdp(std::vector<cageaddr>& nodes)
        {
                bool me = false;
                rdp_store_func func;

                func.key       = key;
                func.value     = value;
                func.keylen    = keylen;
                func.valuelen  = valuelen;
                func.ttl       = ttl;
                func.id        = id;
                func.from      = from;
                func.is_unique = is_unique;
                func.p_dht     = p_dht;

                BOOST_FOREACH(cageaddr &addr, nodes) {
                        if (*addr.id == p_dht->m_id) {
                                me = true;
                                continue;
                        }

                        int desc;
                        desc = p_dht->m_rdp.connect(0, addr.id,
                                                    dht::rdp_store_port,
                                                    func);
                        if (desc <= 0)
                                continue;

                        p_dht->m_rdp_store[desc] = time(NULL);
                }

                return me;
        }

        void
        dht::store_func::operator() (std::vector<cageaddr>& nodes)
        {
                bool me;
                if (p_dht->m_is_use_rdp) {
                        me = store_by_rdp(nodes);
                } else {
                        me = store_by_udp(nodes);
                }

                if (nodes.size() >= (uint32_t)num_find_node) {
                        stored_data sdata;

                        sdata.value     = value;
                        sdata.valuelen  = valuelen;
                        sdata.key       = key;
                        sdata.keylen    = keylen;
                        sdata.id        = id;
                        sdata.is_unique = is_unique;
                        sdata.src       = from;


                        int origin = p_dht->dec_origin_sdata(sdata);

                        if (origin == 0 && ! me) {
                                p_dht->erase_sdata(sdata);
                        }
                }
        }

        void
        dht::recv_store(void *msg, int len, sockaddr *from)
        {
                msg_dht_store *req;
                cageaddr  addr;
                uint160_t dst;
                uint16_t  keylen;
                uint16_t  valuelen;
                uint16_t  ttl;
                int       size;

                req = (msg_dht_store*)msg;

                dst.from_binary(req->hdr.dst, sizeof(req->hdr.dst));
                if (dst != m_id)
                        return;

                keylen   = ntohs(req->keylen);
                valuelen = ntohs(req->valuelen);
                ttl      = ntohs(req->ttl);

                size = sizeof(*req) - sizeof(req->data) + keylen + valuelen;

                if (size != len)
                        return;

                addr = new_cageaddr(&req->hdr, from);

                // add to rttable and request cache
                add(addr);
                m_peers.add_node(addr);

                
                // store data
                boost::shared_array<char> key(new char[keylen]);
                boost::shared_array<char> value(new char[valuelen]);
                id_ptr id(new uint160_t);
                id_ptr src(new uint160_t);

                id->from_binary(req->id, sizeof(req->id));
                src->from_binary(req->from, sizeof(req->from));
                memcpy(key.get(), req->data, keylen);
                memcpy(value.get(), (char*)req->data + keylen, valuelen);


                stored_data data;

                data.value       = value;
                data.valuelen    = valuelen;
                data.key         = key;
                data.keylen      = keylen;
                data.ttl         = ttl;
                data.stored_time = time(NULL);
                data.id          = id;
                data.original    = 0;
                data.src         = src;

                if (req->flags & dht_flag_unique)
                        data.is_unique = true;

                if (ttl == 0) {
                        erase_sdata(data);
                        return;
                }

                add_sdata(data, false);
                insert2recvd_sdata(data, addr.id);
        }

        void
        dht::find_value(const uint160_t &dst, const void *key, uint16_t keylen,
                        callback_find_value func)
        {
                node_state state = m_nat.get_state();
                if (state == node_symmetric || state == node_undefined ||
                    state == node_nat) {
                        value_set_ptr p;
                        func(false, p);
                        return;
                }


                find_nv(dst, func, true, key, keylen);
        }

        void
        dht::find_value_func::operator() (bool result, cageaddr &addr)
        {
                if (! result) {
                        return;
                }

                msg_dht_find_value *msg;
                int  size;
                char buf[1024 * 2];

                size = sizeof(*msg) - sizeof(msg->key);

                if (! p_dht->m_is_use_rdp)
                        size += keylen;

                if (size > (int)sizeof(buf))
                        return;

                msg = (msg_dht_find_value*)buf;

                memset(msg, 0, size);

                msg->nonce  = htonl(nonce);
                msg->domain = htons(addr.domain);

                if (p_dht->m_is_use_rdp) {
                        msg->flag = get_by_rdp;
                } else {
                        msg->keylen = htons(keylen);
                        msg->flag   = get_by_udp;
                        memcpy(msg->key, key.get(), keylen);
                }

                dst->to_binary(msg->id, sizeof(msg->id));

                send_msg(p_dht->m_udp, &msg->hdr, size,
                         type_dht_find_value, addr, p_dht->m_id);
        }

        void
        dht::send_find_value(cageaddr &dst, query_ptr q)
        {
                try {
                        if (m_is_dtun && ! dst.id->is_zero())
                                m_peers.get_addr(dst.id);

                        msg_dht_find_value *msg;
                        int  size;
                        char buf[1024 * 2];

                        size = sizeof(*msg) - sizeof(msg->key);

                        if (! m_is_use_rdp)
                                size += q->keylen;

                        if (size > (int)sizeof(buf))
                                return;

                        msg = (msg_dht_find_value*)buf;

                        memset(msg, 0, size);

                        msg->nonce  = htonl(q->nonce);
                        msg->domain = htons(dst.domain);

                        if (m_is_use_rdp) {
                                msg->flag = get_by_rdp;
                        } else {
                                msg->keylen = htons(q->keylen);
                                msg->flag   = get_by_udp;
                                memcpy(msg->key, q->key.get(), q->keylen);
                        }

                        q->dst->to_binary(msg->id, sizeof(msg->id));

                        send_msg(m_udp, &msg->hdr, size, type_dht_find_value,
                                 dst, m_id);
                } catch (std::out_of_range) {
                        find_value_func func;

                        func.key    = q->key;
                        func.keylen = q->keylen;
                        func.dst    = q->dst;
                        func.nonce  = q->nonce;
                        func.p_dht  = this;

                        m_dtun.request(*dst.id, func);
                }
        }

        void
        dht::recv_find_value(void *msg, int len, sockaddr *from)
        {
                boost::shared_array<char> key;
                msg_dht_find_value_reply *reply;
                msg_dht_find_value       *req;
                cageaddr  addr;
                uint160_t dst;
                uint16_t  size;
                uint16_t  keylen;
                id_ptr    id(new uint160_t);
                char      buf[1024 * 2];

                req = (msg_dht_find_value*)msg;

                dst.from_binary(req->hdr.dst, sizeof(req->hdr.dst));
                if (dst != m_id)
                        return;

                keylen = ntohs(req->keylen);

                size = sizeof(*req) - sizeof(req->key) + keylen;
                if (size != len)
                        return;

                // add to rttable and request cache
                addr = new_cageaddr(&req->hdr, from);
                add(addr);
                m_peers.add_node(addr);

                reply = (msg_dht_find_value_reply*)buf;
                id->from_binary(req->id, sizeof(req->id));

                if (req->flag == get_by_rdp) {
                        boost::unordered_map<_id, sdata_map>::iterator it;
                        _id i;

                        i.id = id;
                        it = m_stored.find(i);

                        if (it != m_stored.end()) {
                                size = sizeof(*reply) - sizeof(reply->data);

                                memset(reply, 0, size);

                                reply->nonce = req->nonce;
                                reply->flag  = data_are_nul;

                                memcpy(reply->id, req->id, sizeof(reply->id));

                                send_msg(m_udp, &reply->hdr, size,
                                         type_dht_find_value_reply,
                                         addr, m_id);

                                return;
                        }
                } else if (req->flag == get_by_udp) {
                        // lookup stored data
                        key = boost::shared_array<char>(new char[keylen]);
                        memcpy(key.get(), req->key, keylen);

                        boost::unordered_map<_id, sdata_map>::iterator it1;
                        _id i;

                        i.id = id;
                        it1 = m_stored.find(i);

                        if (it1 != m_stored.end()) {
                                sdata_map::iterator it2;
                                _key k;

                                k.key    = key;
                                k.keylen = keylen;

                                it2 = it1->second.find(k);

                                if (it2 != it1->second.end()) {
                                        sdata_set::iterator it3;
                                        uint16_t i = 1;
                                        for (it3 = it2->second.begin();
                                             it3 != it2->second.end(); ++it3) {
                                                msg_data *data;

                                                size = sizeof(*reply) -
                                                        sizeof(reply->data) +
                                                        sizeof(*data) -
                                                        sizeof(data->data) +
                                                        it3->keylen +
                                                        it3->valuelen;

                                                memset(reply, 0, size);

                                                reply->nonce = req->nonce;
                                                reply->flag  = data_are_values;
                                                reply->index = htons(i);
                                                reply->total = htons((uint16_t)it2->second.size());

                                                memcpy(reply->id, req->id, sizeof(reply->id));

                                                data = (msg_data*)reply->data;

                                                data->keylen   = htons(it3->keylen);
                                                data->valuelen = htons(it3->valuelen);

                                                memcpy(data->data, it3->key.get(),
                                                       it3->keylen);
                                                memcpy((char*)data->data + it3->keylen,
                                                       it3->value.get(), it3->valuelen);

                                                send_msg(m_udp, &reply->hdr, size,
                                                         type_dht_find_value_reply,
                                                         addr, m_id);
                                        }

                                        return;
                                }
                        }
                } else {
                        return;
                }

                // reply nodes
                msg_nodes *data;
                std::vector<cageaddr>    nodes;
                uint16_t domain;

                lookup(*id, num_find_node, nodes);

                domain = ntohs(req->domain);

                if (domain == domain_inet) {
                        msg_inet *min;

                        size = sizeof(*reply) - sizeof(reply->data) +
                                sizeof(*data) - sizeof(data->addrs) +
                                sizeof(*min) * nodes.size();

                        memset(reply, 0, size);

                        reply->nonce = req->nonce;
                        reply->flag  = data_are_nodes;

                        memcpy(reply->id, req->id, sizeof(reply->id));

                        data = (msg_nodes*)reply->data;

                        data->num    = nodes.size();
                        data->domain = req->domain;

                        min = (msg_inet*)data->addrs;
                        write_nodes_inet(min, nodes);
                } else if (domain == domain_inet6) {
                        msg_inet6 *min6;

                        size = sizeof(*reply) - sizeof(reply->data) +
                                sizeof(*data) - sizeof(data->addrs) +
                                sizeof(*min6) * nodes.size();

                        memset(reply, 0, size);

                        reply->nonce = req->nonce;
                        reply->flag  = data_are_nodes;

                        memcpy(reply->id, req->id, sizeof(reply->id));
                                
                        data = (msg_nodes*)reply->data;

                        data->num    = nodes.size();
                        data->domain = req->domain;

                        min6 = (msg_inet6*)data->addrs;
                        write_nodes_inet6(min6, nodes);
                } else {
                        return;
                }

                send_msg(m_udp, &reply->hdr, size,
                         type_dht_find_value_reply, addr, m_id);
        }

        void
        dht::recv_find_value_reply(void *msg, int len, sockaddr *from)
        {
                std::map<uint32_t, query_ptr>::iterator it;
                msg_dht_find_value_reply *reply;
                timer_query_ptr t;
                cageaddr  addr;
                query_ptr q;
                uint160_t dst;
                uint160_t id;
                uint32_t  nonce;
                int       size;
                _id       i;

                reply = (msg_dht_find_value_reply*)msg;

                dst.from_binary(reply->hdr.dst, sizeof(reply->hdr.dst));
                if (dst != m_id)
                        return;

                nonce = ntohl(reply->nonce);

                it = m_query.find(nonce);
                if (it == m_query.end())
                        return;

                q = it->second;

                id.from_binary(reply->id, sizeof(reply->id));
                if (id != *q->dst)
                        return;

                addr = new_cageaddr(&reply->hdr, from);

                i.id = addr.id;
                if (q->timers.find(i) != q->timers.end()) {
                        if (! q->is_find_value)
                                return;

                        // stop timer
                        t = q->timers[i];
                        m_timer.unset_timer(t.get());
                        q->timers.erase(i);

                        // add to rttale and request cache
                        add(addr);
                        m_peers.add_node(addr);


                        q->sent.insert(i);
                        q->num_query--;
                }


                if (reply->flag == data_are_nul && m_is_use_rdp) {
                        if (q->is_rdp_con) {
                                q->ids.push(addr.id);
                                return;
                        }

                        rdp_get_func func(*this, q);

                        q->is_rdp_con = true;
                        q->rdp_time   = time(NULL);

                        q->rdp_desc = m_rdp.connect(0, addr.id, rdp_get_port,
                                                    func);

                        return;
                } else if (reply->flag == data_are_values && ! m_is_use_rdp) {
                        msg_data *data;
                        uint16_t  index;
                        uint16_t  total;
                        uint16_t  keylen;
                        uint16_t  valuelen;
                        char     *key, *value;

                        size = sizeof(*reply) - sizeof(reply->data) +
                                sizeof(*data) - sizeof(data->data);

                        if (len < size)
                                return;

                        data = (msg_data*)reply->data;

                        keylen   = ntohs(data->keylen);
                        valuelen = ntohs(data->valuelen);

                        size += keylen + valuelen;

                        if (size != len)
                                return;

                        key   = (char*)data->data;
                        value = key + keylen;

                        if (keylen != q->keylen)
                                return;

                        if (memcmp(key, q->key.get(), keylen) != 0)
                                return;

                        index = ntohs(reply->index);
                        total = ntohs(reply->total);


                        std::map<_id, query::val_info>::iterator it_val;

                        it_val = q->valinfo.find(i);
                        if (it_val == q->valinfo.end()) {
                                q->valinfo[i].num_value = total;
                                it_val = q->valinfo.find(i);
                        } else if (it_val->second.num_value != total) {
                                return;
                        }


                        boost::shared_array<char> v_ptr(new char[valuelen]);
                        value_t v;

                        memcpy(v_ptr.get(), value, valuelen);
                        v.value = v_ptr;
                        v.len   = valuelen;

                        q->vset->insert(v);
                        it_val->second.values.insert(v);
                        it_val->second.indeces.insert(index);


                        if (! q->is_timer_recvd_started) {
                                // start timer
                                timeval tval;
                                timer_recvd_ptr tp(new timer_recvd_value(*this,
                                                                         q));

                                tval.tv_sec  = 5;
                                tval.tv_usec = 0;

                                m_timer.set_timer(tp.get(), &tval);
                                
                                q->is_timer_recvd_started = true;
                                q->timer_recvd = tp;
                        }


                        for (it_val = q->valinfo.begin();
                             it_val != q->valinfo.end(); ++it_val) {
                                if (it_val->second.num_value >
                                    it_val->second.values.size()) {
                                        return;
                                }
                        }

                        recvd_value(q);
                } else if (reply->flag == data_are_nodes) {
                        std::vector<cageaddr> nodes;
                        msg_nodes *addrs;
                        uint16_t   domain;

                        size = sizeof(*reply) - sizeof(reply->data) +
                                sizeof(*addrs) - sizeof(addrs->addrs);

                        if (len < size)
                                return;

                        addrs = (msg_nodes*)reply->data;

                        // read nodes
                        domain = ntohs(addrs->domain);
                        if (domain == domain_inet) {
                                msg_inet *min;
                                
                                size += addrs->num * sizeof(*min);

                                if (size != len)
                                        return;

                                min = (msg_inet*)addrs->addrs;

                                read_nodes_inet(min, addrs->num, nodes, from,
                                                m_peers);
                        } else if (domain == domain_inet6) {
                                msg_inet6 *min6;

                                size += addrs->num * sizeof(*min6);

                                if (size != len)
                                        return;

                                min6 = (msg_inet6*)addrs->addrs;

                                read_nodes_inet6(min6, addrs->num, nodes, from,
                                                 m_peers);
                        } else {
                                return;
                        }

                        // sort
                        compare cmp;
                        cmp.m_id = &id;
                        std::sort(nodes.begin(), nodes.end(), cmp);

                        // merge
                        std::vector<cageaddr> tmp;

                        tmp = q->nodes;
                        q->nodes.clear();

                        merge_nodes(id, q->nodes, tmp, nodes, num_find_node);

                        // send
                        send_find(q);
                }
        }

        void
        dht::recvd_value(query_ptr q)
        {
                // call callback function
                callback_find_value func;
                func = boost::get<callback_find_value>(q->func);
                func(true, q->vset);

                remove_query(q);
        }

        void
        dht::refresh()
        {
                boost::unordered_map<_id, sdata_map>::iterator it1;
                sdata_map::iterator it2;
                sdata_set::iterator it3;
                
                time_t now = time(NULL);

                for(it1 = m_stored.begin(); it1 != m_stored.end();) {
                        for (it2 = it1->second.begin();
                             it2 != it1->second.end();) {
                                for (it3 = it2->second.begin();
                                     it3 != it2->second.end();) {
                                        time_t diff;
                                        diff = now - it3->stored_time;

                                        if (diff > it3->ttl)
                                                it2->second.erase(it3++);
                                        else
                                                ++it3;
                                }

                                if (it2->second.size() == 0)
                                        it1->second.erase(it2++);
                                else
                                        ++it2;
                        }

                        if (it1->second.size() == 0)
                                m_stored.erase(it1++);
                        else
                                ++it1;
                }
        }

        bool
        dht::restore_func::restore_by_udp(std::vector<cageaddr> &nodes,
                                          sdata_set::iterator &it)
        {
                msg_dht_store *msg;
                uint16_t       ttl;
                int            size;
                char           buf[1024 * 2];
                char          *p_key, *p_value;
                time_t         now = time(NULL);
                time_t         diff;
                bool           me = false;

                if (it->original > 0)
                        return true;
                        

                size = sizeof(*msg) - sizeof(msg->data) +
                        it->keylen + it->valuelen;

                if (size > (int)sizeof(buf))
                        return false;

                diff = now - it->stored_time;
                if (diff >= it->ttl)
                        return false;

                if (it->original > 0) {
                        store_func sfunc;

                        sfunc.key       = it->key;
                        sfunc.value     = it->value;
                        sfunc.id        = it->id;
                        sfunc.from      = it->src;
                        sfunc.keylen    = it->keylen;
                        sfunc.valuelen  = it->valuelen;
                        sfunc.ttl       = it->ttl - diff;
                        sfunc.is_unique = it->is_unique;
                        sfunc.p_dht     = p_dht;

                        p_dht->find_node(*it->id, sfunc);

                        return true;
                }

                ttl = it->ttl - diff;

                msg = (msg_dht_store*)buf;

                memset(msg, 0, sizeof(*msg));

                msg->keylen   = htons(it->keylen);
                msg->valuelen = htons(it->valuelen);
                msg->ttl      = htons(ttl);

                it->id->to_binary(msg->id, sizeof(msg->id));
                it->src->to_binary(msg->from, sizeof(msg->from));

                p_key   = (char*)msg->data;
                p_value = p_key + it->keylen;

                memcpy(p_key, it->key.get(), it->keylen);
                memcpy(p_value, it->value.get(), it->valuelen);

                if (it->is_unique)
                        msg->flags = dht_flag_unique;

                BOOST_FOREACH(cageaddr &addr, nodes) {
                        if (p_dht->m_id == *addr.id) {
                                me = true;
                                continue;
                        }

                        _id i;

                        i.id = addr.id;
                        if (it->recvd.find(i) != it->recvd.end())
                                continue;

                        it->recvd.insert(i);

                        send_msg(p_dht->m_udp, &msg->hdr, size, type_dht_store,
                                 addr, p_dht->m_id);
                }

                return me;
        }

        bool
        dht::restore_func::restore_by_rdp(std::vector<cageaddr> &nodes,
                                          sdata_set::iterator &it)
        {
                rdp_store_func func;
                time_t         now = time(NULL);
                time_t         diff;
                bool           me = false;

                diff = now - it->stored_time;
                if (diff >= it->ttl)
                        return false;

                if (it->original > 0) {
                        store_func sfunc;

                        sfunc.key       = it->key;
                        sfunc.value     = it->value;
                        sfunc.id        = it->id;
                        sfunc.from      = it->src;
                        sfunc.keylen    = it->keylen;
                        sfunc.valuelen  = it->valuelen;
                        sfunc.ttl       = it->ttl - diff;
                        sfunc.is_unique = it->is_unique;
                        sfunc.p_dht     = p_dht;

                        p_dht->find_node(*it->id, sfunc);

                        return true;
                }

                func.key       = it->key;
                func.value     = it->value;
                func.keylen    = it->keylen;
                func.valuelen  = it->valuelen;
                func.ttl       = it->ttl;
                func.id        = it->id;
                func.from      = it->src;
                func.is_unique = it->is_unique;
                func.p_dht     = p_dht;

                BOOST_FOREACH(cageaddr &addr, nodes) {
                        if (*addr.id == p_dht->m_id) {
                                me = true;
                                continue;
                        }

                        _id    i;

                        i.id = addr.id;
                        if (it->recvd.find(i) != it->recvd.end())
                                continue;

                        int desc;
                        desc = p_dht->m_rdp.connect(0, addr.id,
                                                    dht::rdp_store_port,
                                                    func);
                        if (desc <= 0)
                                continue;

                        p_dht->m_rdp_store[desc] = time(NULL);
                }

                return me;
        }

        void
        dht::restore_func::operator() (std::vector<cageaddr> &n)
        {
                boost::unordered_map<_id, sdata_map>::iterator it1;
                sdata_map::iterator   it2;
                sdata_set::iterator   it3;
                std::vector<cageaddr> nodes;

                for(it1 = p_dht->m_stored.begin();
                    it1 != p_dht->m_stored.end();) {
                        nodes.clear();
                        p_dht->lookup(*it1->first.id, num_find_node, nodes);

                        if (nodes.size() == 0) {
                                ++it1;
                                continue;
                        }

                        for (it2 = it1->second.begin();
                             it2 != it1->second.end();) {
                                for (it3 = it2->second.begin();
                                     it3 != it2->second.end();) {
                                        bool me;

                                        if (p_dht->m_is_use_rdp) {
                                                me = restore_by_rdp(nodes, it3);
                                        } else {
                                                me = restore_by_udp(nodes, it3);
                                        }

                                        if (! me)
                                                it2->second.erase(it3++);
                                        else
                                                ++it3;
                                }

                                if (it2->second.size() == 0)
                                        it1->second.erase(it2++);
                                else
                                        ++it2;
                        }

                        if (it1->second.size() == 0)
                                p_dht->m_stored.erase(it1++);
                        else
                                ++it1;
                }
        }

        void
        dht::sync_node::operator() (cageaddr &addr)
        {
                if (! m_dht.has_id(*addr.id))
                        m_dht.add(addr);
        }

        void
        dht::sweep_rdp()
        {
                time_t now = time(NULL);
                time_t diff;

                std::map<int, rdp_recv_store_ptr>::iterator it1;
                for (it1 = m_rdp_recv_store.begin();
                     it1 != m_rdp_recv_store.end(); ) {
                        diff = now - it1->second->last_time;
                        if (diff > rdp_timeout) {
                                m_rdp.close(it1->first);
                                m_rdp_recv_store.erase(it1++);
                        } else {
                                ++it1;
                        }
                }

                std::map<int, time_t>::iterator it2;
                for (it2 = m_rdp_store.begin(); it2 != m_rdp_store.end(); ) {
                        diff = now - it2->second;
                        if (diff > rdp_timeout) {
                                m_rdp.close(it2->first);
                                m_rdp_store.erase(it2++);
                        } else {
                                ++it2;
                        }
                }

                std::map<int, rdp_recv_get_ptr>::iterator it3;
                for (it3 = m_rdp_recv_get.begin();
                     it3 != m_rdp_recv_get.end(); ) {
                        diff = now - it3->second->m_time;
                        if (diff > rdp_timeout) {
                                m_rdp.close(it3->first);
                                m_rdp_recv_get.erase(it3++);
                        } else {
                                ++it3;
                        }
                }

                std::map<uint32_t, query_ptr>::iterator it4, it4_tmp;
                for (it4 = m_query.begin(); it4 != m_query.end(); ) {
                        diff = now - it4->second->rdp_time;
                        if (it4->second->is_rdp_con && diff > rdp_timeout) {
                                m_rdp.close(it4->second->rdp_desc);

                                if (it4->second->vset->size() > 0) {
                                        it4_tmp = it4++;
                                        recvd_value(it4_tmp->second);
                                        continue;
                                }

                                if (it4->second->ids.size() > 0) {
                                        rdp_get_func func(*this, it4->second);
                                        id_ptr id = it4->second->ids.front();

                                        it4->second->ids.pop();
                                        it4->second->rdp_time = time(NULL);
                                        it4->second->rdp_desc = m_rdp.connect(rdp_get_port,
                                                                id, 0, func);
                                } else {
                                        it4->second->is_rdp_con = false;
                                        send_find(it4->second);
                                }
                        }

                        ++it4;
                }
        }

        void
        dht::restore()
        {
                node_state state = m_nat.get_state();
                if (state == node_symmetric || state == node_undefined ||
                    state == node_nat)
                        return;

                time_t diff;
                diff = time(NULL) - m_last_restore;

                if (diff >= restore_interval) {
                        m_last_restore = time(NULL);

                        restore_func rfunc;

                        rfunc.p_dht = this;
                        find_node(m_id, rfunc);
                }
        }

        void
        dht::dht_join::operator() ()
        {
                if (m_dht.get_size() < num_find_node) {
                        node_state state = m_dht.m_nat.get_state();
                        if (state == node_global ||
                            state == node_cone) {
                                try {
                                        cageaddr addr;

                                        addr = m_dht.m_peers.get_first();

                                        if (addr.domain == domain_inet) {
                                                in_ptr in;
                                                in = boost::get<in_ptr>(addr.saddr);
                                                m_dht.find_node((sockaddr*)in.get(), &no_action);
                                        } else if (addr.domain ==
                                                   domain_inet6) {
                                                in6_ptr in6;
                                                in6 = boost::get<in6_ptr>(addr.saddr);
                                                m_dht.find_node((sockaddr*)in6.get(), &no_action);
                                        }
                                } catch (std::out_of_range) {
                                }
                        }

                        if (m_dht.is_zero())
                                m_interval = 3;
                        else
                                m_interval = 30;
                } else {
                        m_interval = 137;
                }

                timeval tval;

                tval.tv_sec  = m_interval;
                tval.tv_usec = 0;

                m_dht.m_timer.set_timer(this, &tval);
        }
}
