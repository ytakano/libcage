/*
 * Copyright (c) 2010, Yuuki Takano (ytakanoster@gmail.com).
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

#include "rdp.hpp"

#include <boost/foreach.hpp>

namespace libcage {
        const uint8_t rdp::flag_syn = 0x80;
        const uint8_t rdp::flag_ack = 0x40;
        const uint8_t rdp::flag_eak = 0x20;
        const uint8_t rdp::flag_rst = 0x10;
        const uint8_t rdp::flag_nul = 0x08;
        const uint8_t rdp::flag_fin = 0x04;
        const uint8_t rdp::flag_ver = 2;

        const uint16_t rdp::syn_opt_in_seq = 0x8000;

        const uint32_t rdp::rbuf_max_default    = 1500;
        const uint32_t rdp::rcv_max_default     = 1024;
        const uint16_t rdp::well_known_port_max = 1024;
        const uint32_t rdp::timer_rdp_usec      = 300 * 1000;
        const time_t   rdp::max_retrans         = 32;
        const double   rdp::ack_interval        = 0.3;
        const int      rdp::max_data_size       = 1024; // should be 380?

        size_t
        hash_value(const rdp_addr &addr)
        {
                size_t h = addr.did->hash_value();

                boost::hash_combine(h, addr.dport);
                boost::hash_combine(h, addr.sport);

                return h;
        }
        
        void
        rdp::timer_rdp::operator() ()
        {

                timeval tval;

                tval.tv_sec  = 0;
                tval.tv_usec = rdp::timer_rdp_usec;

                get_timer()->set_timer(this, &tval);


                if (m_rdp.m_desc2conn.size() == 0)
                        return;
                
                boost::unordered_map<int, rdp_con_ptr>::iterator it;
                
                for (it = m_rdp.m_desc2conn.begin();
                     it != m_rdp.m_desc2conn.end();) {
                        time_t diff;
                        switch (it->second->state) {
                        case SYN_SENT:
                        case SYN_RCVD:
                        {
                                diff = time(NULL) - it->second->syn_time;
                                if (diff > max_retrans) {
                                        if (it->second->is_pasv) {
                                                // delete connection
                                                m_rdp.m_desc_set.erase(it->first);
                                                m_rdp.m_desc2event.erase(it->first);
                                                m_rdp.m_addr2conn.erase(it->second->addr);
                                                m_rdp.m_desc2conn.erase(it++);
                                                break;
                                        } else {
                                                // invoke the signal of
                                                // "Connection Failed"
                                                rdp_addr addr = it->second->addr;
                                                int desc      = it->first;

                                                ++it;
                                                m_rdp.invoke_event(desc, 0,
                                                                   addr,
                                                                   FAILED);
                                                break;
                                        }
                                } else if (diff > it->second->syn_tout) {
                                        // retry sending syn
                                        m_rdp.m_output_func(it->second->addr.did,
                                                            it->second->syn_pbuf);
                                        it->second->syn_tout *= 2;
                                        it->second->syn_time  = time(NULL);
                                }
                                ++it;
                                break;
                        }
                        case CLOSE_WAIT_ACTIVE:
                        case CLOSE_WAIT_PASV:
                        {
                                diff = time(NULL) - it->second->rst_time;
                                if (diff > max_retrans) {
                                        if (it->second->is_closed) {
                                                // deallocate
                                                m_rdp.m_desc_set.erase(it->first);
                                                m_rdp.m_desc2event.erase(it->first);
                                                m_rdp.m_addr2conn.erase(it->second->addr);
                                                m_rdp.m_desc2conn.erase(it++);
                                                break;
                                        } else {
                                                it->second->state = CLOSED;
                                        }
                                } else if (diff > it->second->rst_tout) {
                                        it->second->rst_tout *= 2;
                                        it->second->rst_time  = time(NULL);

                                        if (it->second->is_retry_rst) {
                                                m_rdp.m_output_func(it->second->addr.did,
                                                                    it->second->rst_pbuf);
                                        }
                                }
                                ++it;
                                break;
                        }
                        case OPEN:
                        {
                                rdp_con_ptr p_con = it->second;

                                ++it;

                                // retransmission
                                if (! p_con->retransmit())
                                        break;


                                // delayed ack;
#ifndef WIN32
                                timeval t;
                                double  sec1, sec2;

                                gettimeofday(&t, NULL);

                                sec1 = t.tv_sec + t.tv_usec / 1000000.0;
                                sec2 = p_con->acked_time.tv_sec + 
                                        p_con->acked_time.tv_usec / 1000000.0;

                                if (sec1 - sec2 > ack_interval)
                                        p_con->delayed_ack();
#else
                                // XXX
                                // for Windows
#endif
                                break;
                        }
                        default:
                                ++it;
                        }
                }
        }

        rdp::rdp(timer &tm) : m_timer(tm), m_timer_rdp(*this)
        {
                timeval   tval;

                // start timer for retransmission and delayed ack
                tval.tv_sec  = 0;
                tval.tv_usec = timer_rdp_usec;

                m_timer.set_timer(&m_timer_rdp, &tval);
        }

        rdp::~rdp()
        {
                m_timer.unset_timer(&m_timer_rdp);
        }

        rdp_state
        rdp::status(int desc)
        {
                if (m_desc_set.find(desc) == m_desc_set.end())
                        return CLOSED;

                listening_t::right_iterator it_ls;

                it_ls = m_listening.right.find(desc);
                if (it_ls != m_listening.right.end())
                        return LISTEN;


                boost::unordered_map<int, rdp_con_ptr>::iterator it;
                it = m_desc2conn.find(desc);
                if (it != m_desc2conn.end())
                        return it->second->state;

                return CLOSED;
        }

        void
        rdp::receive(int desc, void *buf, int *len)
        {
                boost::unordered_map<int, rdp_con_ptr>::iterator it;

                it = m_desc2conn.find(desc);
                if (it == m_desc2conn.end()) {
                        *len = 0;
                        return;
                }
                
                int   total = 0;
                char *dst = (char*)buf;
                while (! it->second->rqueue.empty()) {
                        packetbuf_ptr  pbuf = it->second->rqueue.front();

                        if (total + pbuf->get_len() > *len) {
                                *len = total;
                                return;
                        }

                        memcpy(dst, pbuf->get_data(), pbuf->get_len());

                        total += pbuf->get_len();
                        dst   += pbuf->get_len();

                        it->second->rqueue.pop();
                }

                *len = total;
        }

        void
        rdp::invoke_event(int desc1, int desc2, rdp_addr addr, rdp_event event)
        {
                boost::unordered_map<int, callback_rdp_event>::iterator it;

                it = m_desc2event.find(desc1);
                if (it == m_desc2event.end())
                        return;

                if (desc2 > 0) {
                        it->second(desc2, addr, event);
                        boost::unordered_map<int, callback_rdp_event>::iterator it2;

                        it2 = m_desc2event.find(desc2);

                        if (event == ACCEPTED && it2 == m_desc2event.end())
                                m_desc2event[desc2] = it->second;
                } else {
                        it->second(desc1, addr, event);
                }
        }

        int
        rdp::send(int desc, const void *buf, int len)
        {
                boost::unordered_map<int, rdp_con_ptr>::iterator it;

                it = m_desc2conn.find(desc);
                if (it == m_desc2conn.end() || it->second->state != OPEN)
                        return -1;

                if (len <= 0)
                        return 0;

                int total = 0;

                for (;;) {
                        packetbuf_ptr pbuf = packetbuf::construct();
                        int   size = (len < max_data_size) ? len : max_data_size;
                        void *data;

                        data = pbuf->append(size);
                        memcpy(data, buf, size);

                        if (! it->second->enqueue_swnd(pbuf))
                                return total;

                        total += size;

                        if (total == len)
                                return total;

                        buf = (char*)buf + size;
                }

                return -1;
        }

        void
        rdp::close(int desc)
        {
                listening_t::right_iterator it_ls;

                it_ls = m_listening.right.find(desc);
                if (it_ls != m_listening.right.end()) {
                        m_listening.right.erase(it_ls);
                        return;
                }


                boost::unordered_map<int, rdp_con_ptr>::iterator it;

                it = m_desc2conn.find(desc);
                if (it == m_desc2conn.end())
                        return;

                switch (it->second->state) {
                case OPEN:
                {
                        // Send <SEQ=SND.NXT><RST>
                        // Set State = CLOSE-WAIT

                        it->second->state     = CLOSE_WAIT_ACTIVE;
                        it->second->is_closed = true;

                        packetbuf_ptr  pbuf = packetbuf::construct();
                        rdp_head      *rst;

                        rst = (rdp_head*)pbuf->append(sizeof(*rst));

                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ver;
                        rst->hlen   = (uint8_t)sizeof(*rst) / 2;
                        rst->sport  = htons(it->second->addr.sport);
                        rst->dport  = htons(it->second->addr.dport);
                        rst->seqnum = htonl(it->second->snd_nxt);

                        it->second->rst_pbuf     = pbuf;
                        it->second->rst_time     = time(NULL);
                        it->second->rst_tout     = 1;
                        it->second->is_retry_rst = true;

                        m_output_func(it->second->addr.did, pbuf);
                        break;
                }
                case CLOSE_WAIT_PASV:
                {
                        it->second->is_closed = true;
                        break;
                }
                case CLOSED:
                {
                        m_desc_set.erase(it->first);
                        m_desc2event.erase(it->first);
                        m_addr2conn.erase(it->second->addr);
                        m_desc2conn.erase(it);

                        break;
                }
                case SYN_SENT:
                case SYN_RCVD:
                {
                        it->second->state = CLOSED;

                        // Send <SEQ=SND.NXT><RST>
                        packetbuf_ptr  pbuf = packetbuf::construct();
                        rdp_head      *rst;

                        rst = (rdp_head*)pbuf->append(sizeof(*rst));

                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ver;
                        rst->hlen   = (uint8_t)sizeof(*rst) / 2;
                        rst->sport  = htons(it->second->addr.sport);
                        rst->dport  = htons(it->second->addr.dport);
                        rst->seqnum = htonl(it->second->snd_nxt);

                        m_output_func(it->second->addr.did, pbuf);

                        m_desc_set.erase(it->first);
                        m_desc2event.erase(it->first);
                        m_addr2conn.erase(it->second->addr);
                        m_desc2conn.erase(it);

                        break;
                }
                default:
                        ;
                }
        }

        int
        rdp::generate_desc()
        {
                int desc;

                do {
                        desc = rand();
                } while (m_desc_set.find(desc) != m_desc_set.end());

                return desc;
        }

        // passive open
        int
        rdp::listen(uint16_t sport, callback_rdp_event func)
        {
                if (m_listening.left.find(sport) == m_listening.left.end()) {
                        int desc = generate_desc();

                        m_desc_set.insert(desc);
                        m_listening.insert(listening_val(sport, desc));
                        m_desc2event[desc] = func;

                        return desc;
                }

                return -1;
        }

        // active open
        int
        rdp::connect(uint16_t sport, id_ptr did, uint16_t dport,
                     callback_rdp_event func)
        {
                // If remote port not specified
                //   Return "Error - remote port not specified"
                // Endif
                // Generate SND.ISS
                // Set SND.NXT = SND.ISS + 1
                //     SND.UNA = SND.ISS
                // Fill in RCV.MAX, RBUF.MAX from Open parameters
                // If local port not specified
                //   Allocate a local port
                // Endif
                // Send <SEQ=SND.ISS><MAX=RCV.MAX><BUFMAX=RBUF.MAX><SYN>
                // Set State = SYN-SENT
                // Return (local port, connection identifier)

                rdp_con_ptr p_con(new rdp_con(*this));
                rdp_addr    addr;


                // check whether resouces are avilable
                addr.did   = did;
                addr.dport = dport;

                if (sport != 0) {
                        addr.sport = sport;
                        if (m_addr2conn.find(addr) != m_addr2conn.end()) {
                                return -1;
                        }
                } else {
                        do {
                                addr.sport = rand() & 0xffff;
                                if (addr.sport < well_known_port_max)
                                        continue;
                        } while (m_addr2conn.find(addr) != m_addr2conn.end());
                }

                p_con->addr      = addr;
                p_con->is_pasv   = false;
                p_con->state     = SYN_SENT;
                p_con->snd_iss   = rand();
                p_con->snd_nxt   = p_con->snd_iss + 1;
                p_con->snd_una   = p_con->snd_iss;
                p_con->rcv_max   = rcv_max_default;
                p_con->rbuf_max  = rbuf_max_default;
                p_con->is_closed = false;

                p_con->set_output_func(m_output_func);


                // create syn packet
                packetbuf_ptr  pbuf = packetbuf::construct();
                rdp_syn       *syn;
                
                syn = (rdp_syn*)pbuf->append(sizeof(*syn));
                memset(syn, 0, sizeof(*syn));

                syn->head.flags  = flag_syn | flag_ver;
                syn->head.hlen   = (uint8_t)(sizeof(*syn) / 2);
                syn->head.sport  = htons(sport);
                syn->head.dport  = htons(dport);
                syn->head.seqnum = htonl(p_con->snd_iss);

                syn->out_segs_max = htons(p_con->rcv_max);
                syn->seg_size_max = htons(p_con->rbuf_max);

                set_syn_option_seq(syn->options, true);

                p_con->syn_pbuf = pbuf;
                p_con->syn_time = time(NULL);
                p_con->syn_tout = 1;


                // create descriptor
                int desc = generate_desc();

                p_con->desc = desc;

                m_desc_set.insert(desc);
                m_addr2conn[addr]  = p_con;
                m_desc2conn[desc]  = p_con;
                m_desc2event[desc] = func;


                // send syn
                m_output_func(addr.did, pbuf);

                return desc;
        }

        void
        rdp::input_dgram(id_ptr src, packetbuf_ptr pbuf)
        {
                rdp_head *head;
                rdp_addr  addr;
                uint16_t  sport;
                uint16_t  dport;

                if (pbuf->get_len() < (int)sizeof(rdp_head))
                        return;

                head = (rdp_head*)pbuf->get_data();

                sport = ntohs(head->dport);
                dport = ntohs(head->sport);

                addr.did   = src;
                addr.dport = dport;
                addr.sport = sport;

                boost::unordered_map<rdp_addr, rdp_con_ptr>::iterator it;
                it = m_addr2conn.find(addr);

#ifdef DEBUG
                std::cout << "input: src = " << addr.dport
                          << ", dst = " << addr.sport
                          << ", seq = " << ntohl(head->seqnum)
                          << ", ack = " << ntohl(head->acknum)
                          << "\n       flags = ";

                if (head->flags & flag_syn) {
                        std::cout << "syn, ";
                }

                if (head->flags & flag_ack) {
                        std::cout << "ack, ";
                }

                if (head->flags & flag_rst) {
                        std::cout << "rst, ";
                }

                if (head->flags & flag_nul) {
                        std::cout << "nul, ";
                }

                if (head->flags & flag_fin) {
                        std::cout << "fin, ";
                }


                if (it != m_addr2conn.end()) {
                        rdp_con_ptr con = it->second;

                        switch (con->state) {
                        case CLOSE_WAIT_PASV:
                                std::cout << "state = CLOSE-WAIT-PASV";
                                break;
                        case CLOSE_WAIT_ACTIVE:
                                std::cout << "state = CLOSE-WAIT-ACTIVE";
                                break;
                        case SYN_SENT:
                                std::cout << "state = SYN-SENT";
                                break;
                        case SYN_RCVD:
                                std::cout << "state = SYN-RCVD";
                                break;
                        case OPEN:
                                std::cout << "state = OPEN";
                                break;
                        case CLOSED:
                                std::cout << "state = CLOSED";
                                break;
                        default:
                                break;
                        }
                }

                std::cout << std::endl;
#endif // DEBUG

                if (it != m_addr2conn.end()) {
                        rdp_con_ptr con = it->second;

                        switch (con->state) {
                        case CLOSE_WAIT_PASV:
                                in_state_close_wait_pasv(con, addr, pbuf);
                                break;
                        case CLOSE_WAIT_ACTIVE:
                                in_state_close_wait_active(con, addr, pbuf);
                                break;
                        case SYN_SENT:
                                in_state_syn_sent(con, addr, pbuf);
                                break;
                        case SYN_RCVD:
                                in_state_syn_rcvd(con, addr, pbuf);
                                break;
                        case OPEN:
                                in_state_open(con, addr, pbuf);
                                break;
                        case CLOSED:
                                in_state_closed(addr, pbuf);
                                break;
                        default:
                                break;
                        }
                } else if (m_listening.left.find(sport) !=
                           m_listening.left.end()) {
                        // LISTEN
                        in_state_listen(addr, pbuf);
                } else {
                        // CLOSED
                        in_state_closed(addr, pbuf);
                }
        }

        void
        rdp::in_state_close_wait_active(rdp_con_ptr p_con, rdp_addr addr,
                                        packetbuf_ptr pbuf)
        {
                // If RST, FIN set
                //   Send <SEQ=SND.NXT><FIN>
                // Endif

                rdp_head *head = (rdp_head*)pbuf->get_data();

                if (head->flags & flag_rst && head->flags & flag_fin) {
                        p_con->is_retry_rst = false;

                        packetbuf_ptr  pbuf_fin = packetbuf::construct();
                        rdp_head      *fin;

                        fin = (rdp_head*)pbuf_fin->append(sizeof(*fin));
                        memset(fin, 0, sizeof(*fin));

                        fin->flags  = flag_fin | flag_ver;
                        fin->hlen   = (uint8_t)(sizeof(*fin) / 2);
                        fin->sport  = htons(addr.sport);
                        fin->dport  = htons(addr.dport);
                        fin->seqnum = htonl(p_con->snd_nxt);
  
                        m_output_func(p_con->addr.did, pbuf_fin);
                }
        }

        void
        rdp::in_state_close_wait_pasv(rdp_con_ptr p_con, rdp_addr addr,
                                      packetbuf_ptr pbuf)
        {
                // If FIN set
                //   Return
                // Endif

                // If RST set
                //   Send <SEQ=SND.NXT><FIN><RST>
                //   Return
                // Endif

                rdp_head *head = (rdp_head*)pbuf->get_data();

                if (head->flags & flag_fin) {
                        p_con->is_retry_rst = false;
                } else if (head->flags & flag_rst) {
                        p_con->rst_time = time(NULL);
                        m_output_func(p_con->addr.did, p_con->rst_pbuf);
                }
        }

        void
        rdp::in_state_closed(rdp_addr addr, packetbuf_ptr pbuf)
        {
                rdp_head *head = (rdp_head*)pbuf->get_data();

                if (head->flags & flag_rst) {
                        // If RST set
                        //   Discard segment
                        //   Return
                        // Endif
                } else if (head->flags & flag_ack || head->flags & flag_nul) {
                        // If ACK or NUL set
                        //   Send <SEQ=SEG.ACK + 1><RST>
                        //   Discard segment
                        //   Return

                        // send rst
                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;
                        uint32_t       seg_ack;

                        seg_ack = ntohl(head->acknum);

                        rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));
                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->seqnum = htonl(seg_ack + 1);

                        m_output_func(addr.did, pbuf_rst);
                } else {
                        // else
                        //   Send <SEQ=0><RST><ACK=SEG.SEQ><ACK>
                        //   Discard segment
                        //   Return
                        // Endif

                        // send rst | ack
                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;

                        rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));
                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ack | flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->acknum = head->seqnum;
                        
                        m_output_func(addr.did, pbuf_rst);
                }
        }

        void
        rdp::in_state_listen(rdp_addr addr, packetbuf_ptr pbuf)
        {
                rdp_head *head = (rdp_head*)pbuf->get_data();

                if (head->flags & flag_rst) {
                        // If RST set
                        //   Discard the segment
                        //   Return
                        // Endif
                        return;
                } else if (head->flags & flag_ack || head->flags & flag_nul) {
                        // If ACK or NUL set
                        //   Send <SEQ=SEG.ACK + 1><RST>
                        //   Return
                        // Endif

                        // send rst
                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;
                        uint32_t       seg_ack;

                        seg_ack = ntohl(head->acknum);

                        rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));
                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->seqnum = htonl(seg_ack + 1);

                        m_output_func(addr.did, pbuf_rst);
                } else if (head->flags & flag_syn) {
                        // If SYN set
                        //   Set RCV.CUR = SEG.SEQ
                        //       RCV.IRS = SEG.SEQ
                        //       RCV.ACK = SEG.SEQ
                        //       SND.MAX = SEG.MAX
                        //       SBUF.MAX = SEG.BMAX
                        //   Send <SEQ=SND.ISS><ACK=RCV.CUR><MAX=RCV.MAX>
                        //        <BUFMAX=RBUF.MAX><ACK><SYN>
                        //   Set State = SYN-RCVD
                        //   Return
                        // Endif

                        if (pbuf->get_len() < (int)sizeof(rdp_syn))
                                return;

                        // create connection
                        rdp_con_ptr  p_con(new rdp_con(*this));
                        rdp_syn     *syn_in;
                        uint16_t     opts;

                        syn_in = (rdp_syn*)head;

                        opts = ntohs(syn_in->options);

                        p_con->addr      = addr;
                        p_con->is_pasv   = true;
                        p_con->state     = SYN_RCVD;
                        p_con->snd_iss   = rand();
                        p_con->snd_nxt   = p_con->snd_iss + 1;
                        p_con->snd_una   = p_con->snd_iss;
                        p_con->rcv_max   = rcv_max_default;
                        p_con->rbuf_max  = rbuf_max_default;
                        p_con->rcv_cur   = ntohl(syn_in->head.seqnum);
                        p_con->rcv_irs   = p_con->rcv_cur;
                        p_con->rcv_ack   = p_con->rcv_cur;
                        p_con->snd_max   = ntohs(syn_in->out_segs_max);
                        p_con->sbuf_max  = ntohs(syn_in->seg_size_max);
                        p_con->is_closed = false;

                        if (opts & syn_opt_in_seq) {
                                p_con->is_in_seq = true;
                        } else {
                                p_con->is_in_seq = false;
                        }

                        p_con->set_output_func(m_output_func);

                        p_con->init_swnd();
                        p_con->init_rwnd();


                        // create syn ack packet
                        // enqueue
                        packetbuf_ptr  pbuf_syn = packetbuf::construct();
                        rdp_syn       *syn_out;

                        syn_out = (rdp_syn*)pbuf_syn->append(sizeof(*syn_out));

                        memset(syn_out, 0, sizeof(*syn_out));

                        syn_out->head.flags  = flag_syn | flag_ack | flag_ver;
                        syn_out->head.hlen   = (uint8_t)(sizeof(*syn_out) / 2);
                        syn_out->head.sport  = htons(addr.sport);
                        syn_out->head.dport  = htons(addr.dport);
                        syn_out->head.seqnum = htonl(p_con->snd_iss);
                        syn_out->head.acknum = htonl(p_con->rcv_cur);

                        syn_out->out_segs_max = htons(p_con->rcv_max);
                        syn_out->seg_size_max = htons(p_con->rbuf_max);

                        set_syn_option_seq(syn_out->options, true);

                        p_con->syn_pbuf = pbuf_syn;
                        p_con->syn_time = time(NULL);
                        p_con->syn_tout = 1;

#ifndef WIN32
                        gettimeofday(&p_con->acked_time, NULL);
#else
                        // XXX
                        // for Windows
#endif

                        // create descriptor
                        int desc = generate_desc();

                        p_con->desc = desc;

                        m_desc_set.insert(desc);
                        m_addr2conn[addr] = p_con;
                        m_desc2conn[desc] = p_con;


                        // send syn ack
                        m_output_func(addr.did, pbuf_syn);
                }

                // If anything else (should never get here)
                //   Discard segment
                //   Return
                // Endif
        }

        void
        rdp::in_state_syn_sent(rdp_con_ptr p_con, rdp_addr addr, 
                               packetbuf_ptr pbuf)
        {
                rdp_head *head = (rdp_head*)pbuf->get_data();

                if (head->flags & flag_ack) {
                        // If ACK set
                        //   If RST clear and SEG.ACK != SND.ISS
                        //     Send <SEQ=SEG.ACK + 1><RST>
                        //     Discard segment; Return
                        //   Endif
                        // Endif

                        uint32_t ack = ntohl(head->acknum);

                        if (! (head->flags & flag_rst) &&
                            ack != p_con->snd_iss) {
                                packetbuf_ptr  pbuf_rst = packetbuf::construct();
                                rdp_head      *rst;

                                rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));

                                memset(rst, 0, sizeof(*rst));

                                rst->flags  = flag_rst | flag_ver;
                                rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                                rst->sport  = htons(addr.sport);
                                rst->dport  = htons(addr.dport);
                                rst->seqnum = htonl(ack + 1);

                                m_output_func(addr.did, pbuf_rst);
                                return;
                        }
                } 

                if (head->flags & flag_rst) {
                        // If RST set
                        //   If ACK set
                        //     Signal "Connection Refused"
                        //     Set State =  CLOSED
                        //     Deallocate connection record
                        //   Endif
                        //   Discard segment
                        //   Return
                        // Endif

                        if (head->flags & flag_ack) {
                                p_con->state = CLOSED;

                                // invoke the signal of "Connection Refused"
                                invoke_event(p_con->desc, 0, addr, REFUSED);
                        }
                        return;
                }

                if (head->flags & flag_syn) {
                        // If SYN set
                        //   Set RCV.CUR = SEG.SEQ
                        //       RCV.IRS = SEG.SEQ
                        //       RCV.ACK = SEG.SEQ
                        //       SND.MAX = SEG.MAX
                        //       SBUF.MAX = SEG.BMAX
                        //   If ACK set
                        //     Set SND.UNA = SEG.ACK
                        //     State = OPEN
                        //     Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                        //   else
                        //     Set State = SYN-RCVD
                        //     Send <SEQ=SND.ISS><ACK=RCV.CUR><MAX=RCV.MAX>
                        //          <BUFMAX=RBUF.MAX><SYN><ACK>
                        //   Endif
                        //   Return
                        // Endif

                        if (pbuf->get_len() < (int)sizeof(rdp_syn))
                                return;

                        rdp_syn *syn = (rdp_syn*)head;
                        uint16_t opts;

                        opts = ntohs(syn->options);

                        p_con->rcv_cur  = ntohl(syn->head.seqnum);
                        p_con->rcv_irs  = p_con->rcv_cur;
                        p_con->rcv_ack  = p_con->rcv_cur;
                        p_con->snd_max  = ntohs(syn->out_segs_max);
                        p_con->sbuf_max = ntohs(syn->seg_size_max);

                        p_con->init_swnd();
                        p_con->init_rwnd();

#ifndef WIN32
                        gettimeofday(&p_con->acked_time, NULL);
#else
                        // XXX
                        // for Windows
#endif

                        if (opts & syn_opt_in_seq) {
                                p_con->is_in_seq = true;
                        } else {
                                p_con->is_in_seq = false;
                        }


                        if (syn->head.flags & flag_ack) {
                                p_con->snd_una = ntohl(syn->head.acknum);
                                p_con->state   = OPEN;

                                // send ack
                                packetbuf_ptr  pbuf_ack = packetbuf::construct();
                                rdp_head      *ack;

                                ack = (rdp_head*)pbuf_ack->append(sizeof(*ack));

                                memset(ack, 0, sizeof(*ack));

                                ack->flags  = flag_ack | flag_ver;
                                ack->hlen   = (uint8_t)(sizeof(*ack) / 2);
                                ack->sport  = htons(addr.sport);
                                ack->dport  = htons(addr.dport);
                                ack->seqnum = htonl(p_con->snd_nxt);
                                ack->acknum = htonl(p_con->rcv_cur);

                                p_con->syn_pbuf.reset();

                                // invoke the signal of "Connection Established"
                                invoke_event(p_con->desc, 0, addr, CONNECTED);

                                m_output_func(addr.did, pbuf_ack);
                        } else {
                                p_con->state = SYN_RCVD;

                                // send syn ack
                                packetbuf_ptr  pbuf_syn = packetbuf::construct();
                                rdp_syn       *syn_out;

                                syn_out = (rdp_syn*)pbuf_syn->append(sizeof(*syn_out));

                                memset(syn_out, 0, sizeof(*syn_out));

                                syn_out->head.flags  = flag_syn | flag_ack | flag_ver;
                                syn_out->head.hlen   = (uint8_t)(sizeof(*syn_out) / 2);
                                syn_out->head.sport  = htons(addr.sport);
                                syn_out->head.dport  = htons(addr.dport);
                                syn_out->head.seqnum = htonl(p_con->snd_iss);
                                syn_out->head.acknum = htonl(p_con->rcv_cur);

                                syn_out->out_segs_max = htons(p_con->rcv_max);
                                syn_out->seg_size_max = htons(p_con->rbuf_max);

                                p_con->syn_pbuf = pbuf_syn;
                                p_con->syn_time = time(NULL);
                                p_con->syn_tout = 1;

                                m_output_func(addr.did, pbuf_syn);
                        }
                }

                // If anything else
                //   Discard segment
                //   Return
                // Endif
        }

        void
        rdp::in_state_syn_rcvd(rdp_con_ptr p_con, rdp_addr addr,
                               packetbuf_ptr pbuf)
        {
                rdp_head *head = (rdp_head*)pbuf->get_data();
                uint32_t  seq = ntohl(head->seqnum);
                uint32_t  seq_irs;
                uint32_t  rcv_max2;

                seq_irs = seq - p_con->rcv_irs;

                rcv_max2  = p_con->rcv_cur + (p_con->rcv_max * 2);
                rcv_max2 -= p_con->rcv_irs;

                if (! (0 < seq_irs && seq_irs <= rcv_max2) ) {
                        // If RCV.IRS < SEG.SEQ =< RCV.CUR + (RCV.MAX * 2)
                        //   Segment sequence number acceptable
                        // else
                        //   Set RCV.ACK = RCV.CUR
                        //   Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                        //   Discard segment
                        //   Return
                        // Endif

                        packetbuf_ptr  pbuf_ack = packetbuf::construct();
                        rdp_head      *ack;

                        ack = (rdp_head*)pbuf_ack->append(sizeof(*ack));

                        memset(ack, 0, sizeof(*ack));

                        ack->flags  = flag_ack | flag_ver;
                        ack->hlen   = (uint8_t)(sizeof(*ack) / 2);
                        ack->sport  = htons(addr.sport);
                        ack->dport  = htons(addr.dport);
                        ack->seqnum = htonl(p_con->snd_nxt);
                        ack->acknum = htonl(p_con->rcv_cur);

                        p_con->rcv_ack = p_con->rcv_cur;
#ifndef WIN32
                        gettimeofday(&p_con->acked_time, NULL);
#else
                        // XXX
                        // for Windows
#endif

                        m_output_func(addr.did, pbuf_ack);
                } else if (head->flags & flag_rst) {
                        // If RST set
                        //   If passive Open
                        //     Set State = LISTEN
                        //   else
                        //     Set State = CLOSED
                        //     Signal "Connection Refused"
                        //     Discard segment
                        //     Deallocate connection record
                        //   Endif
                        //   Return
                        // Endif

                        if (p_con->is_pasv) {
                                m_desc_set.erase(p_con->desc);
                                m_desc2event.erase(p_con->desc);
                                m_addr2conn.erase(p_con->addr);
                                m_desc2conn.erase(p_con->desc);
                        } else {
                                p_con->state = CLOSED;

                                // invoke the signal of "Connection Refused"
                                invoke_event(p_con->desc, 0, addr, REFUSED);
                        }
                } else if (head->flags & flag_syn) {
                        // If SYN set
                        //   Send <SEQ=SEG.ACK + 1><RST>
                        //   Set State = CLOSED
                        //   Signal "Connection Reset"
                        //   Discard segment
                        //   Deallocate connection record
                        //   Return
                        // Endif

                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;
                        uint32_t       acknum;

                        acknum = ntohl(head->acknum);

                        rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));

                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->seqnum = htonl(acknum + 1);

                        if (p_con->is_pasv) {
                                m_desc_set.erase(p_con->desc);
                                m_desc2event.erase(p_con->desc);
                                m_addr2conn.erase(p_con->addr);
                                m_desc2conn.erase(p_con->desc);
                        } else {
                                p_con->state = CLOSED;

                                // invoke the signal of "Connection Reset"
                                invoke_event(p_con->desc, 0, addr, RESET);
                        }

                        m_output_func(addr.did, pbuf_rst);
                } else if (head->flags & flag_eak) {
                        // If EACK set
                        //   Send <SEQ=SEG.ACK + 1><RST>
                        //   Discard segment
                        //   Return
                        // Endif

                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;
                        uint8_t        acknum;

                        acknum = ntohl(head->acknum);

                        rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));

                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->seqnum = htonl(acknum + 1);

                        m_output_func(addr.did, pbuf_rst);
                } else if (head->flags & flag_ack) {
                        // If ACK set
                        //   If SEG.ACK = SND.ISS
                        //     Set State = OPEN
                        //   else
                        //     Send <SEQ=SEG.ACK + 1><RST>
                        //     Discard segment
                        //     Return
                        //   Endif
                        // else
                        //   Discard segment
                        //   Return
                        // Endif

                        uint32_t acknum;

                        acknum = ntohl(head->acknum);

                        if (acknum == p_con->snd_iss) {
                                p_con->state = OPEN;
                                p_con->syn_pbuf.reset();

                                if (p_con->is_pasv) {
                                        // invoke the signal of "Connection
                                        // Established"
                                        listening_t::left_iterator it;

                                        it = m_listening.left.find(addr.sport);

                                        if (it != m_listening.left.end()) {
                                                invoke_event(it->second,
                                                             p_con->desc, addr,
                                                             ACCEPTED);
                                        } else {
                                                close(p_con->desc);
                                        }
                                }
                                // If Data in segment or NUL set
                                //   If the received segment is in sequence
                                //     Copy the data (if any) to user buffers
                                //     Set RCV.CUR=SEG.SEQ
                                //     Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                                //   else
                                //     If out-of-sequence delivery permitted
                                //       Copy the data (if any) to user buffers
                                //     Endif
                                //     Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                                //          <EACK><RCVDSEQNO1>...<RCVDSEQNOn>
                                //   Endif
                                // Endif

                                if (pbuf->get_len() - (int)sizeof(*head) > 0 ||
                                    head->flags & flag_nul) {
                                        uint16_t dlen = ntohs(head->dlen);

                                        if ((int)(dlen + sizeof(*head)) !=
                                            pbuf->get_len()) {
                                                return;
                                        }

                                        if (head->flags & flag_nul &&
                                            dlen > 0) {
                                                return;
                                        }

                                        pbuf->rm_head(sizeof(*head));
                                        
                                        p_con->rwnd_recv_data(pbuf, seq);

                                        if (p_con->rqueue.size() > 0) {
                                                // invoke the signal of
                                                // "Ready to Read"
                                                invoke_event(p_con->desc, 0,
                                                             addr, READY2READ);
                                        }
                                }
                        } else {
                                packetbuf_ptr  pbuf_rst = packetbuf::construct();
                                rdp_head      *rst;

                                rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));

                                memset(rst, 0, sizeof(*rst));

                                rst->flags  = flag_rst | flag_ver;
                                rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                                rst->sport  = htons(addr.sport);
                                rst->dport  = htons(addr.dport);
                                rst->seqnum = htonl(acknum + 1);

                                m_output_func(addr.did, pbuf_rst);
                        }
                }
        }

        void
        rdp::in_state_open(rdp_con_ptr p_con, rdp_addr addr, 
                           packetbuf_ptr pbuf)
        {
                rdp_head *head = (rdp_head*)pbuf->get_data();
                uint32_t  seq = ntohl(head->seqnum);
                uint32_t  seq_irs;
                uint32_t  rcv_max2;

                seq_irs = seq - p_con->rcv_irs;

                rcv_max2  = p_con->rcv_cur + (p_con->rcv_max * 2);
                rcv_max2 -= p_con->rcv_irs;

                if (! (0 < seq_irs && seq_irs <= rcv_max2) ) {
                        // If RCV.CUR < SEG.SEQ =< RCV.CUR + (RCV.MAX * 2)
                        //   Segment sequence number acceptable
                        // else
                        //   Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                        //   Discard segment and return
                        // Endif

                        packetbuf_ptr  pbuf_ack = packetbuf::construct();
                        rdp_head      *ack;

                        ack = (rdp_head*)pbuf_ack->append(sizeof(*ack));

                        memset(ack, 0, sizeof(*ack));

                        ack->flags  = flag_ack | flag_ver;
                        ack->hlen   = (uint8_t)(sizeof(*ack) / 2);
                        ack->sport  = htons(addr.sport);
                        ack->dport  = htons(addr.dport);
                        ack->seqnum = htonl(p_con->snd_nxt);
                        ack->acknum = htonl(p_con->rcv_cur);

                        m_output_func(addr.did, pbuf_ack);

                        p_con->rcv_ack = p_con->rcv_cur;

#ifndef WIN32
                        gettimeofday(&p_con->acked_time, NULL);
#else
                        // XXX
                        // for Windows
#endif

                        return;
                } else if (head->flags & flag_rst) {
                        // passive close

                        // If RST set
                        //   Set State = CLOSE-WAIT-PASV
                        //   Signal "Connection Reset"
                        //   Send <SEQ=SND.NXT><FIN><RST>
                        //   Return
                        // Endif

                        p_con->state = CLOSE_WAIT_PASV;

                        // send rst | fin
                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;

                        rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));

                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_fin | flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->seqnum = htonl(p_con->snd_nxt);

                        p_con->rst_pbuf     = pbuf_rst;
                        p_con->rst_time     = time(NULL);
                        p_con->rst_tout     = 1;
                        p_con->is_retry_rst = true;


                        // invoke the signal of "Connection Reset"
                        invoke_event(p_con->desc, 0, addr, RESET);

                        m_output_func(addr.did, pbuf_rst);
                        
                        return;
                } else if (head->flags & flag_nul) {
                        // If NUL set
                        //   Set RCV.CUR=SEG.SEQ
                        //   Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                        //   Discard segment
                        //   Return
                        // Endif

                        if (pbuf->get_len() != (int)sizeof(rdp_head))
                                return;

                        pbuf->rm_head(pbuf->get_len());

                        p_con->rwnd_recv_data(pbuf, seq);

                        return;
                } else if (head->flags & flag_syn) {
                        // If SYN set
                        //   Send <SEQ=SEG.ACK + 1><RST>
                        //   Set State = CLOSED
                        //   Signal "Connection Reset"
                        //   Discard segment
                        //   Deallocate connection record
                        //   Return
                        // Endif

                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;
                        uint32_t       acknum;

                        acknum = ntohl(head->acknum);

                        rst = (rdp_head*)pbuf->append(sizeof(*rst));

                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->seqnum = htonl(acknum + 1);

                        m_output_func(addr.did, pbuf);

                        p_con->state = CLOSED;

                        // invoke the signal of "Connetion Reset"
                        invoke_event(p_con->desc, 0, addr, RESET);

                        return;
                }

                if (head->flags & flag_ack) {
                        p_con->recv_ack(ntohl(head->acknum));
                }

                if (head->flags & flag_eak) {
                        // If EACK set
                        //   Flush acknowledged segments
                        // Endif

                        uint32_t *eacks;
                        uint8_t   len = head->hlen;
                        int       i;

                        len -= sizeof(*head) / 2;
                        len /= 2;

                        eacks = (uint32_t*)&head[1];
                        for (i = 0; i < len; i++) {
                                p_con->recv_eack(ntohl(eacks[i]));
                        }
                }

                // If Data in segment
                //   If the received segment is in sequence
                //     Copy the data to user buffers
                //     Set RCV.CUR=SEG.SEQ
                //     Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                //   else
                //     If out-of-sequence delivery permitted
                //       Copy the data to user buffers
                //     Endif
                //     Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK><EACK><RCVDSEQNO1>
                //          ...<RCVDSEQNOn>
                //   Endif
                // Endif

                uint16_t dlen;
                uint16_t hlen;

                dlen = ntohs(head->dlen);
                hlen = head->hlen * 2;

                if (dlen > 0 && dlen + hlen == pbuf->get_len()) {
                        pbuf->rm_head(hlen);

                        p_con->rwnd_recv_data(pbuf, seq);

                        if (p_con->rqueue.size() > 0) {
                                // invoke the signal of "Ready to Read"
                                invoke_event(p_con->desc, 0, addr, READY2READ);
                        }
                }
        }

        void
        rdp::set_callback_dgram_out(callback_dgram_out func)
        {
                m_output_func = func;
        }

        void
        rdp::set_callback_rdp_event(int desc, callback_rdp_event func)
        {
                if  (m_desc_set.find(desc) == m_desc_set.end())
                        return;

                m_desc2event[desc] = func;
        }

        void
        rdp::set_syn_option_seq(uint16_t &options, bool sequenced)
        {
                if (sequenced)
                        options |= syn_opt_in_seq;
                else
                        options &= ~syn_opt_in_seq;

                options = htons(options);
        }

        void
        rdp_con::set_output_func(callback_dgram_out func)
        {
                m_output_func = func;
        }

        void
        rdp_con::init_rwnd()
        {
                m_rwnd_len  = rcv_max * 2;
                m_rwnd_head = 0;
                m_rwnd_used = 0;

                m_rwnd = boost::shared_array<rwnd>(new rwnd[m_rwnd_len]);
        }

        void
        rdp_con::init_swnd()
        {
                m_swnd_len    = snd_max * 4;
                m_swnd_head   = 0;
                m_swnd_used   = 0;
                m_swnd_ostand = 0;

                m_swnd = boost::shared_array<swnd>(new swnd[m_swnd_len]);
        }

        bool
        rdp_con::retransmit()
        {
                if (m_swnd_used == 0)
                        return true;

                for (int i = m_swnd_head;; i++) {
                        swnd   *p_wnd = &m_swnd[i];
                        time_t  diff  = time(NULL) - p_wnd->sent_time;

                        if (! p_wnd->is_sent)
                                break;

                        if (p_wnd->is_acked)
                                continue;

                        if (diff > rdp::max_retrans) {
                                // broken pipe
                                state = CLOSED;
                                ref_rdp.invoke_event(desc, 0, addr, BROKEN);

                                return false;
                        } else if (diff > p_wnd->rt_sec) {
                                // retransmit
                                rdp_head *head = (rdp_head*)p_wnd->pbuf->get_data();

                                head->acknum = htonl(rcv_cur);

                                p_wnd->sent_time  = time(NULL);
                                p_wnd->rt_sec    *= 2;

                                m_output_func(addr.did, p_wnd->pbuf);
                        }
                }

                return true;
        }

        bool
        rdp_con::enqueue_swnd(packetbuf_ptr pbuf)
        {
                if (m_swnd_used >= m_swnd_len || state != OPEN)
                        return false;

                swnd *p_wnd;
                int   pos = (m_swnd_head + m_swnd_used) % m_swnd_len;

                p_wnd = &m_swnd[pos];

                p_wnd->pbuf      = pbuf;
                p_wnd->sent_time = 0;
                p_wnd->is_acked  = false;
                p_wnd->is_sent   = false;
                p_wnd->rt_sec    = 1;

                m_swnd_used++;

                send_ostand_swnd();

                return true;
        }

        void
        rdp_con::send_ostand_swnd()
        {
                int i   = m_swnd_ostand;
                int end = (m_swnd_head + m_swnd_used) % m_swnd_len;

                while (i != end) {
                        if (snd_nxt - snd_una < snd_max) {
                                swnd *p_wnd = &m_swnd[i];

                                p_wnd->sent_time = time(NULL);
                                p_wnd->is_sent   = true;
                                p_wnd->seqnum    = snd_nxt;

                                
                                rdp_head *head;
                                uint16_t  len = p_wnd->pbuf->get_len();

                                head = (rdp_head*)p_wnd->pbuf->prepend(sizeof(*head));

                                memset(head, 0, sizeof(*head));

                                head->flags  = rdp::flag_ack | rdp::flag_ver;
                                head->hlen   = (uint8_t)sizeof(*head) / 2;
                                head->sport  = htons(addr.sport);
                                head->dport  = htons(addr.dport);
                                head->dlen   = htons(len);
                                head->seqnum = htonl(snd_nxt);
                                head->acknum = htonl(rcv_cur);

                                m_output_func(addr.did, p_wnd->pbuf);


                                snd_nxt++;

                                i++;
                                if (i >= m_swnd_len)
                                        i %= m_swnd_len;

                        } else {
                                break;
                        }
                }

                m_swnd_ostand = i;
        }

        void
        rdp_con::recv_ack(uint32_t acknum)
        {
                // If ACK set
                //   If SND.UNA =< SEG.ACK < SND.NXT
                //     Set SND.UNA = SEG.ACK
                //     Flush acknowledged segments
                //   Endif
                // Endif

                if (acknum - snd_una < snd_nxt - snd_una) {
                        int i = m_swnd_head;

                        while (i != m_swnd_ostand) {
                                swnd *p_wnd = &m_swnd[i];

                                if (p_wnd->seqnum - snd_una <=
                                    acknum - snd_una) {
                                        if (p_wnd->is_sent) {
                                                if (! p_wnd->is_acked)
                                                        p_wnd->pbuf.reset();
                                                m_swnd_used--;
                                        }
                                } else {
                                        break;
                                }

                                i++;
                                if (i >= m_swnd_len)
                                        i %= m_swnd_len;
                        }

                        m_swnd_head = i;
                        snd_una     = acknum;
                }

                send_ostand_swnd();
        }

        void
        rdp_con::recv_eack(uint32_t eacknum)
        {
                if (m_swnd_used == 0)
                        return;

                uint32_t pos = eacknum - m_swnd[m_swnd_head].seqnum;

                if (pos >= (uint32_t)m_swnd_len)
                        return;

                pos += m_swnd_head;
                if (pos >= (uint32_t)m_swnd_len)
                        pos %= m_swnd_len;

                
                swnd *p_wnd = &m_swnd[pos];

                if (p_wnd->seqnum == eacknum &&
                    p_wnd->is_sent && ! p_wnd->is_acked) {
                        p_wnd->pbuf.reset();
                        p_wnd->is_acked = true;
                }


                // remove head of sending window
                while (m_swnd_head != m_swnd_ostand &&
                       m_swnd[m_swnd_head].is_sent &&
                       m_swnd[m_swnd_head].is_acked) {
                        m_swnd_head++;
                        if (m_swnd_head >= m_swnd_len)
                                m_swnd_head %= m_swnd_len;
                }
        }

        void
        rdp_con::rwnd_recv_data(packetbuf_ptr pbuf, uint32_t seqnum)
        {
                uint32_t seq_cur;
                int      idx;

                seq_cur = seqnum - rcv_cur;

                if (! (0 < seq_cur && seq_cur <= (uint32_t)m_rwnd_len))
                        return;

                idx = m_rwnd_head + seq_cur - 1;
                if (idx >= m_rwnd_len)
                        idx %= m_rwnd_len;

                // insert buffer to receive window
                if (! m_rwnd[idx].is_used) {
                        m_rwnd[idx].pbuf      = pbuf;
                        m_rwnd[idx].seqnum    = seqnum;
                        m_rwnd[idx].is_used   = true;
                        m_rwnd[idx].is_eacked = false;

                        m_rwnd_used++;
                }

                // remove head of receive window
                while (m_rwnd[m_rwnd_head].is_used) {
                        rcv_cur++;

                        if (m_rwnd[m_rwnd_head].pbuf->get_len() > 0)
                                rqueue.push(m_rwnd[m_rwnd_head].pbuf);

                        m_rwnd[m_rwnd_head].pbuf.reset();
                        m_rwnd[m_rwnd_head].is_used   = false;
                        m_rwnd[m_rwnd_head].is_eacked = false;

                        m_rwnd_used--;
                        m_rwnd_head++;
                        if (m_rwnd_head >= m_rwnd_len)
                                m_rwnd_head %= m_rwnd_len;
                }

                // send ack
                if (rcv_cur - rcv_ack > rcv_max / 4 || pbuf->get_len() == 0)
                        delayed_ack();

                return;
        }

        void
        rdp_con::delayed_ack()
        {
                // for eack
#define MAX_EACK 64
                uint32_t seqs[MAX_EACK];
                int idx;
                int i, j;

                idx = m_rwnd_head;

                i = 0;
                j = 0;
                while (i < m_rwnd_used && j < MAX_EACK) {
                        if (m_rwnd[idx].is_used) {
                                if (! m_rwnd[idx].is_eacked) {
                                        seqs[j] = htonl(m_rwnd[idx].seqnum);
                                        m_rwnd[idx].is_eacked = true;
                                        j++;
                                }
                                i++;
                        }

                        idx++;
                        if (idx >= m_rwnd_len)
                                idx %= m_rwnd_len;
                }

                if (rcv_cur == rcv_ack && j == 0)
                        return;


                packetbuf_ptr  pbuf_ack = packetbuf::construct();
                rdp_head      *ack;

                ack = (rdp_head*)pbuf_ack->append(sizeof(*ack));

                memset(ack, 0, sizeof(*ack));

                ack->flags  = rdp::flag_ack | rdp::flag_ver;
                ack->hlen   = (uint8_t)(sizeof(*ack) / 2);
                ack->sport  = htons(addr.sport);
                ack->dport  = htons(addr.dport);
                ack->seqnum = htonl(snd_nxt);
                ack->acknum = htonl(rcv_cur);

                if (j > 0) {
                        pbuf_ack->append(sizeof(seqs[0]) * j);

                        memcpy(&ack[1], seqs, sizeof(seqs[0]) * j);

                        ack->flags |= rdp::flag_eak;
                        ack->hlen  += (uint8_t)(sizeof(seqs[0]) * j / 2);
                }


                m_output_func(addr.did, pbuf_ack);

                rcv_ack = rcv_cur;

#ifndef WIN32
                gettimeofday(&acked_time, NULL);
#else
                // XXX
                // for Windows
#endif
        }
}
