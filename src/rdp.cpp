#include "rdp.hpp"

namespace libcage {
        const uint8_t rdp::flag_syn = 0x80;
        const uint8_t rdp::flag_ack = 0x40;
        const uint8_t rdp::flag_eak = 0x20;
        const uint8_t rdp::flag_rst = 0x10;
        const uint8_t rdp::flag_nul = 0x08;
        const uint8_t rdp::flag_ver = 2;

        const uint16_t rdp::syn_opt_in_seq = 0x8000;

        const uint32_t rdp::rbuf_max_default    = 1024 * 2;
        const uint32_t rdp::rcv_max_default     = 1024;
        const uint16_t rdp::well_known_port_max = 1024;

        size_t
        hash_value(const rdp_addr &addr)
        {
                size_t h = addr.did->hash_value();

                boost::hash_combine(h, addr.dport);
                boost::hash_combine(h, addr.sport);

                return h;
        }

        rdp::rdp()
        {
                // XXX
                // start retransmission timer
        }

        rdp::~rdp()
        {

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

                rdp_con_ptr p_con(new rdp_con);
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

                p_con->addr     = addr;
                p_con->is_pasv  = false;
                p_con->state    = SYN_SENT;
                p_con->snd_iss  = random();
                p_con->snd_nxt  = p_con->snd_iss + 1;
                p_con->snd_una  = p_con->snd_iss;
                p_con->rcv_max  = rcv_max_default;
                p_con->rbuf_max = rbuf_max_default;

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
                p_con->syn_num  = 1;

                // init recv buffer
                p_con->init_rwnd();


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
                        case CLOSE_WAIT:
                                in_state_closed_wait(con, addr, pbuf);
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
        rdp::in_state_closed_wait(rdp_con_ptr con, rdp_addr addr,
                                  packetbuf_ptr pbuf)
        {
                // If RST set
                //   Set State = CLOSED
                //   Discard segment
                //   Cancel TIMWAIT timer
                //   Deallocate connection record
                // else
                //   Discard segment
                // Endif
        }

        void
        rdp::in_state_closed(rdp_addr addr, packetbuf_ptr pbuf)
        {
                // If RST set
                //   Discard segment
                //   Return
                // Endif
                //
                // If ACK or NUL set
                //   Send <SEQ=SEG.ACK + 1><RST>
                //   Discard segment
                //   Return
                // else
                //   Send <SEQ=0><RST><ACK=SEG.SEQ><ACK>
                //   Discard segment
                //   Return
                // Endif
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
                        rst->hlen   = sizeof(*rst) / 2;
                        rst->sport  = htons(addr.sport);
                        rst->dport  = htons(addr.dport);
                        rst->seqnum = htonl(seg_ack + 1);

                        m_output_func(addr.did, pbuf_rst);
                } else if (head->flags & flag_syn) {
                        // If SYN set
                        //   Set RCV.CUR = SEG.SEQ
                        //       RCV.IRS = SEG.SEQ
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
                        rdp_con_ptr  p_con(new rdp_con);
                        rdp_syn     *syn_in;
                        uint16_t     opts;

                        syn_in = (rdp_syn*)head;

                        opts = ntohs(syn_in->options);

                        p_con->addr     = addr;
                        p_con->is_pasv  = true;
                        p_con->state    = SYN_RCVD;
                        p_con->snd_iss  = random();
                        p_con->snd_nxt  = p_con->snd_iss + 1;
                        p_con->snd_una  = p_con->snd_iss;
                        p_con->rcv_max  = rcv_max_default;
                        p_con->rbuf_max = rbuf_max_default;
                        p_con->rcv_cur  = ntohl(syn_in->head.seqnum);
                        p_con->rcv_irs  = ntohl(syn_in->head.seqnum);
                        p_con->snd_max  = ntohs(syn_in->out_segs_max);
                        p_con->sbuf_max = ntohs(syn_in->seg_size_max);

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
                        syn_out->head.hlen   = (uint8_t)sizeof(*syn_out) / 2;
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
                                rst->hlen   = (uint8_t)sizeof(*rst) / 2;
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
                        p_con->snd_max  = ntohs(syn->out_segs_max);
                        p_con->sbuf_max = ntohs(syn->seg_size_max);

                        if (opts & syn_opt_in_seq) {
                                p_con->is_in_seq = true;
                        } else {
                                p_con->is_in_seq = false;
                        }

                        p_con->init_swnd();

                        if (syn->head.flags & flag_ack) {
                                p_con->snd_una = ntohl(syn->head.acknum);
                                p_con->state   = OPEN;

                                // send ack
                                packetbuf_ptr  pbuf_ack = packetbuf::construct();
                                rdp_head      *ack;

                                ack = (rdp_head*)pbuf_ack->append(sizeof(*ack));

                                memset(ack, 0, sizeof(*ack));

                                ack->flags  = flag_ack | flag_ver;
                                ack->hlen   = (uint8_t)sizeof(*ack) / 2;
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
                                syn_out->head.hlen   = (uint8_t)sizeof(*syn_out) / 2;
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

                if (!(p_con->rcv_irs < seq &&
                      seq <= p_con->rcv_cur + (p_con->rcv_max * 2))) {
                        // If RCV.IRS < SEG.SEQ =< RCV.CUR + (RCV.MAX * 2)
                        //   Segment sequence number acceptable
                        // else
                        //   Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                        //   Discard segment
                        //   Return
                        // Endif

                        packetbuf_ptr  pbuf_ack = packetbuf::construct();
                        rdp_head      *ack;

                        ack = (rdp_head*)pbuf_ack->append(sizeof(*ack));

                        memset(ack, 0, sizeof(*ack));

                        ack->flags  = flag_ack | flag_ver;
                        ack->hlen   = (uint8_t)sizeof(*ack) / 2;
                        ack->sport  = htons(addr.sport);
                        ack->dport  = htons(addr.dport);
                        ack->seqnum = htonl(p_con->snd_nxt);
                        ack->acknum = htonl(p_con->rcv_cur);

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
                        rst->hlen   = (uint8_t)sizeof(*rst) / 2;
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
                        rst->hlen   = (uint8_t)sizeof(*rst) / 2;
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
                        } else {
                                packetbuf_ptr  pbuf_rst = packetbuf::construct();
                                rdp_head      *rst;

                                rst = (rdp_head*)pbuf_rst->append(sizeof(*rst));

                                memset(rst, 0, sizeof(*rst));

                                rst->flags  = flag_rst | flag_ver;
                                rst->hlen   = (uint8_t)sizeof(*rst) / 2;
                                rst->sport  = htons(addr.sport);
                                rst->dport  = htons(addr.dport);
                                rst->seqnum = htonl(acknum + 1);

                                m_output_func(addr.did, pbuf_rst);
                        }
                }
        }

        void
        rdp::in_state_open(rdp_con_ptr con, rdp_addr addr, 
                           packetbuf_ptr pbuf)
        {
                // If RCV.CUR < SEG.SEQ =< RCV.CUR + (RCV.MAX * 2)
                //   Segment sequence number acceptable
                // else
                //   Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                //   Discard segment and return
                // Endif
                //
                // If RST set
                //   Set State = CLOSE-WAIT
                //   Signal "Connection Reset"
                //   Return
                // Endif
                //
                // If NUL set
                //   Set RCV.CUR=SEG.SEQ
                //   Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                //   Discard segment
                //   Return
                // Endif
                //
                // If SYN set
                //   Send <SEQ=SEG.ACK + 1><RST>
                //   Set State = CLOSED
                //   Signal "Connection Reset"
                //   Discard segment
                //   Deallocate connection record
                //   Return
                // Endif
                //
                // If ACK set
                //   If SND.UNA =< SEG.ACK < SND.NXT
                //     Set SND.UNA = SEG.ACK
                //     Flush acknowledged segments
                //   Endif
                // Endif
                //
                // If EACK set
                //   Flush acknowledged segments
                // Endif
                //
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
        rdp_con::init_rwnd()
        {
                m_rwnd_len  = rdp::rcv_max_default;
                m_rwnd_head = 0;
                m_rwnd_used = 0;

                m_rwnd = boost::shared_array<rwnd>(new rwnd[m_rwnd_len]);
        }

        void
        rdp_con::init_swnd()
        {
                m_swnd_len    = snd_max * 4;
                m_swnd_used   = 0;
                m_swnd_head   = 0;
                m_swnd_tail   = 0;
                m_swnd_ostand = 0;

                m_swnd = boost::shared_array<swnd>(new swnd[m_swnd_len]);
        }

        bool
        rdp_con::enqueue_swnd(packetbuf_ptr pbuf)
        {
                if (m_swnd_used >= m_swnd_len)
                        return false;

                int   pos = (m_swnd_head + m_swnd_used) % m_swnd_len;
                swnd *p_wnd;

                p_wnd = &m_swnd[pos];
                p_wnd->pbuf      = pbuf;
                p_wnd->sent_time = 0;
                p_wnd->is_acked  = false;
                p_wnd->is_sent   = false;
                p_wnd->seqnum    = snd_nxt;

                m_swnd_used++;

                if (m_swnd_ostand < snd_max) {
                        p_wnd->sent_time = time(NULL);
                        p_wnd->is_sent   = true;

                        m_output_func(addr.did, pbuf);

                        m_swnd_tail++;
                        m_swnd_ostand++;

                        if (m_swnd_tail >= m_swnd_len)
                                m_swnd_tail = 0;
                }

                return true;
        }

        void
        rdp_con::set_output_func(callback_dgram_out func)
        {
                m_output_func = func;
        }

        // m_swnd =
        // [0, seqnum = 4  | 1, unused      | 2, unused     |
        //  3, seqnum = 98 | 4, seqnum = 99 | 5, seqnum = 0 |
        //  6, seqnum = 1  | 7, seqnum = 2  | 8, seqnum = 3 ]
        //
        // m_swnd_head = 3
        //
        // num_from_head(99) = 1
        // num_from_head(2)  = 4
        //
        // seq2pos(98) = 3
        // seq2pos(0)  = 5
        // seq2pos(4)  = 0
        uint32_t
        rdp_con::num_from_head(uint32_t seqnum) {
                uint32_t hseq = m_swnd[m_swnd_head].seqnum;
                uint32_t num;

                if (seqnum > hseq) {
                        num = seqnum - hseq;
                } else {
                        num = seqnum + (0xffffffff - hseq) + 1;
                }

                return num;
        }

        int
        rdp_con::seq2pos(uint32_t seqnum)
        {
                uint32_t num;
                int      pos;

                num = num_from_head(seqnum);

                if (num > (uint32_t)m_swnd_used)
                        return -1;

                pos = m_swnd_head + num;

                if (pos >= m_swnd_len)
                        pos %= m_swnd_len;

                return pos;
        }

        void
        rdp_con::ack_ostand(int pos)
        {
                swnd *p_wnd = &m_swnd[pos];

                if (p_wnd->is_sent && ! p_wnd->is_acked) {
                        p_wnd->is_acked = true;
                        p_wnd->pbuf.reset();

                        m_swnd_ostand--;

                        if (pos == m_swnd_head) {
                                p_wnd->sent_time = 0;
                                p_wnd->is_sent   = false;
                                p_wnd->is_acked  = false;

                                m_swnd_used--;

                                if (m_swnd_head == m_swnd_tail) {
                                        m_swnd_tail++;

                                        if (m_swnd_tail >= m_swnd_len)
                                                m_swnd_tail %= m_swnd_len;

                                        m_swnd_head = m_swnd_tail;
                                } else {
                                        m_swnd_head++;
                                        if (m_swnd_head >= m_swnd_len)
                                                m_swnd_head %= m_swnd_len;
                                }
                        }
                }
        }

        void
        rdp_con::recv_ack(uint32_t ack)
        {

        }

        void
        rdp_con::recv_eack(uint32_t eack)
        {
                uint32_t pos;

                pos = seq2pos(eack);
                if (pos < 0)
                        return;

                ack_ostand(eack);

                // XXX
                // send
        }
}
