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
                typedef boost::unordered_map<int, rdp_con_ptr> map;
                
                BOOST_FOREACH(map::value_type &p, m_rdp.m_desc2conn) {
                        // retransmission
                        p.second->retransmit();


                        // delayed ack;
#ifndef WIN32
                        timeval t;
                        double  sec1, sec2;

                        gettimeofday(&t, NULL);

                        sec1 = t.tv_sec + t.tv_usec / 1000000.0;
                        sec2 = p.second->acked_time.tv_sec + 
                                p.second->acked_time.tv_usec;

                        if (sec1 - sec2 > ack_interval)
                                p.second->delayed_ack();
#else
                        // XXX
                        // for Windows
#endif
                }

                timeval tval;

                tval.tv_sec  = 0;
                tval.tv_usec = rdp::timer_rdp_usec;

                get_timer()->set_timer(this, &tval);
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

        int
        rdp::generate_desc()
        {
                int desc;

                do {
                        desc = random();
                } while (m_desc_set.find(desc) != m_desc_set.end());

                return desc;
        }

        // passive open
        int
        rdp::listen(uint16_t sport)
        {
                if (m_listening.left.find(sport) == m_listening.left.end()) {
                        int desc = generate_desc();

                        m_desc_set.insert(desc);
                        m_listening.insert(listening_val(sport, desc));

                        return desc;
                }

                return -1;
        }

        // active open
        int
        rdp::connect(uint16_t sport, id_ptr did, uint16_t dport)
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
                                addr.sport = random() & 0xffff;
                                if (addr.sport < well_known_port_max)
                                        continue;
                        } while (m_addr2conn.find(addr) != m_addr2conn.end());
                }

                p_con->addr      = addr;
                p_con->is_pasv   = false;
                p_con->state     = SYN_SENT;
                p_con->snd_iss   = random();
                p_con->snd_nxt   = p_con->snd_iss + 1;
                p_con->snd_una   = p_con->snd_iss;
                p_con->rcv_max   = rcv_max_default;
                p_con->rbuf_max  = rbuf_max_default;
                p_con->is_closed = false;

                p_con->set_output_func(m_output_func);
                p_con->set_event_func(m_event_func);


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
                p_con->syn_num  = 1;


                // send syn
                m_output_func(addr.did, pbuf);


                // create descriptor
                int desc = generate_desc();

                p_con->desc = desc;

                m_desc_set.insert(desc);
                m_addr2conn[addr] = p_con;
                m_desc2conn[desc] = p_con;

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

                if (it != m_addr2conn.end()) {
                        rdp_con_ptr con = it->second;

                        switch (con->state) {
                        case CLOSE_WAIT_PASV:
                                in_state_closed_wait_pasv(con, addr, pbuf);
                                break;
                        case SYN_SENT:
                                in_state_syn_sent(con, addr, pbuf);
                                break;
                        case SYN_RCVD:
                                in_state_syn_rcvd(con, addr, pbuf);
                        case OPEN:
                                in_state_open(con, addr, pbuf);
                        case CLOSED:
                                in_state_closed(addr, pbuf);
                        default:
                                break;
                        }
                } else if (m_listening.left.find(dport) !=
                           m_listening.left.end()) {
                        // LISTEN
                        in_state_listen(addr, pbuf);
                } else {
                        // CLOSED
                        in_state_closed(addr, pbuf);
                }
        }

        void
        rdp::in_state_closed_wait_pasv(rdp_con_ptr p_con, rdp_addr addr,
                                       packetbuf_ptr pbuf)
        {
                // If FIN set
                //   Set State = CLOSED
                //   Discard segment
                //   Cancel TIMWAIT timer
                //   Deallocate connection record
                // else
                //   Discard segment
                // Endif

                rdp_head *head = (rdp_head*)pbuf->get_data();

                if (head->flags == flag_rst) {
                        p_con->state = CLOSED;

                        // stop close wiat timer
                        m_timer.unset_timer(&p_con->timer_cw_pasv);

                        if (p_con->is_closed) {
                                m_desc_set.erase(p_con->desc);
                                m_addr2conn.erase(p_con->addr);
                                m_desc2conn.erase(p_con->desc);
                        }
                }
        }

        void
        rdp::in_state_closed(rdp_addr addr, packetbuf_ptr pbuf)
        {
                rdp_head *head = (rdp_head*)pbuf->get_data();

                if (head->flags == flag_rst) {
                        // If RST set
                        //   Discard segment
                        //   Return
                        // Endif
                } else if (head->flags == flag_ack || head->flags == flag_nul) {
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
                        p_con->snd_iss   = random();
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
                        p_con->set_event_func(m_event_func);

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
                        p_con->syn_num  = 1;

#ifndef WIN32
                        gettimeofday(&p_con->acked_time, NULL);
#else
                        // XXX
                        // for Windows
#endif


                        // send syn ack
                        m_output_func(addr.did, pbuf_syn);


                        // create descriptor
                        int desc = generate_desc();

                        p_con->desc = desc;

                        m_desc_set.insert(desc);
                        m_addr2conn[addr] = p_con;
                        m_desc2conn[desc] = p_con;
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

                if (head->flags & flag_ack && ~head->flags & flag_rst) {
                        // If ACK set
                        //   If RST clear and SEG.ACK != SND.ISS
                        //     Send <SEQ=SEG.ACK + 1><RST>
                        //   Endif
                        //   Discard segment; Return
                        // Endif

                        uint32_t ack = ntohl(head->acknum);

                        if (ack != p_con->snd_iss) {
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
                        }

                } else if (head->flags & flag_rst) {
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
                                m_event_func(p_con->desc, addr, REFUSED);
                        }
                } else if (head->flags & flag_syn) {
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
                                ack->sport  = htons(addr.dport);
                                ack->seqnum = htonl(p_con->snd_nxt);
                                ack->acknum = htonl(p_con->rcv_cur);

                                m_output_func(addr.did, pbuf_ack);

                                p_con->syn_pbuf.reset();

                                // invoke the signal of "Connection Established"
                                m_event_func(p_con->desc, addr, ESTABLISHED);
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
                                p_con->syn_num  = 1;

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

                        m_output_func(addr.did, pbuf_ack);

                        p_con->rcv_ack = p_con->rcv_cur;

#ifndef WIN32
                        gettimeofday(&p_con->acked_time, NULL);
#else
                        // XXX
                        // for Windows
#endif
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
                                m_addr2conn.erase(p_con->addr);
                                m_desc2conn.erase(p_con->desc);
                        } else {
                                p_con->state = CLOSED;

                                // invoke the signal of "Connection Refused"
                                m_event_func(p_con->desc, addr, REFUSED);
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

                        m_output_func(addr.did, pbuf_rst);

                        if (p_con->is_pasv) {
                                m_addr2conn.erase(p_con->addr);
                                m_desc2conn.erase(p_con->desc);
                        } else {
                                p_con->state = CLOSED;

                                // invoke the signal of "Connection Reset"
                                m_event_func(p_con->desc, addr, RESET);
                        }
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
                                        m_event_func(p_con->desc, addr,
                                                     ESTABLISHED_FROM);
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
                                                m_event_func(p_con->desc, addr,
                                                             READY2READ);
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
                        //   Send <SEQ=SND.NXT><ACK=RCV.CUR><FIN><RST>
                        //   Return
                        // Endif

                        p_con->state = CLOSE_WAIT_PASV;

                        // invoke the signal of "Connection Reset"
                        m_event_func(p_con->desc, addr, RESET);


                        // send rst | fin
                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;

                        rst = (rdp_head*)pbuf_rst.get();

                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_fin | flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->seqnum = htonl(p_con->snd_nxt);
                        rst->acknum = htonl(p_con->rcv_cur);

                        m_output_func(addr.did, pbuf_rst);
                        

                        // start close wait timer
                        timeval tval;

                        tval.tv_sec  = 1;
                        tval.tv_usec = 0;

                        p_con->timer_cw_pasv.m_sec = 1;

                        m_timer.set_timer(&p_con->timer_cw_pasv, &tval);

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
                        m_event_func(p_con->desc, addr, RESET);

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
                                m_event_func(p_con->desc, addr, READY2READ);
                        }
                }
        }

        void
        rdp::set_callback_dgram_out(callback_dgram_out func)
        {
                m_output_func = func;
        }

        void
        rdp::set_callback_rdp_event(callback_rdp_event func)
        {
                m_event_func = func;
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
        rdp_con::set_event_func(callback_rdp_event func)
        {
                m_event_func = func;
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

        void
        rdp_con::retransmit()
        {
                if (m_swnd_used == 0)
                        return;

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
                                m_event_func(desc, addr, BROKEN);

                                return;
                        } else if (diff > p_wnd->rt_sec) {
                                // retransmit
                                rdp_head *head = (rdp_head*)p_wnd->pbuf.get();

                                head->acknum = htonl(rcv_cur);

                                p_wnd->sent_time = time(NULL);

                                m_output_func(addr.did, p_wnd->pbuf);
                        }
                }
        }

        bool
        rdp_con::enqueue_swnd(packetbuf_ptr pbuf)
        {
                if (m_swnd_used >= m_swnd_len)
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

                                if (p_wnd->seqnum - snd_una <
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

        void
        rdp_con::timer_close_wait_pasv::operator() ()
        {
                if (m_sec > rdp::max_retrans) {
                        m_con.state = CLOSED;

                        if (m_con.is_closed) {
                                m_con.ref_rdp.m_desc_set.erase(m_con.desc);
                                m_con.ref_rdp.m_addr2conn.erase(m_con.addr);
                                m_con.ref_rdp.m_desc2conn.erase(m_con.desc);
                        }
                } else {
                        // send rst | fin
                        packetbuf_ptr  pbuf_rst = packetbuf::construct();
                        rdp_head      *rst;

                        rst = (rdp_head*)pbuf_rst.get();

                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = rdp::flag_rst | rdp::flag_fin | rdp::flag_ver;
                        rst->hlen   = (uint8_t)(sizeof(*rst) / 2);
                        rst->sport  = htons(m_con.addr.sport);
                        rst->dport  = htons(m_con.addr.dport);
                        rst->seqnum = htonl(m_con.snd_nxt);
                        rst->acknum = htonl(m_con.rcv_cur);

                        m_con.m_output_func(m_con.addr.did, pbuf_rst);


                        // restart timer
                        timeval tval;

                        m_sec *= 2;

                        tval.tv_sec  = m_sec;
                        tval.tv_usec = 0;

                        get_timer()->set_timer(this, &tval);
                }
        }
}
