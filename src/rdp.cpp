#include "rdp.hpp"

namespace libcage {
        const uint8_t rdp::flag_syn = 0x80;
        const uint8_t rdp::flag_ack = 0x40;
        const uint8_t rdp::flag_eak = 0x20;
        const uint8_t rdp::flag_rst = 0x10;
        const uint8_t rdp::flag_nul = 0x08;
        const uint8_t rdp::flag_ver = 2;

        const uint32_t rdp::rbuf_max_default    = 1024 * 2;
        const uint32_t rdp::snd_max_default     = 1024;
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
                
        }

        rdp::~rdp()
        {

        }

        // passive open
        int
        rdp::listen(uint16_t sport)
        {
                if (m_listening.left.find(sport) == m_listening.left.end()) {
                        int desc;

                        do {
                                desc = random();
                        } while (m_desc_set.find(desc) != m_desc_set.end());

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
                // Fill in SND.MAX, RMAX.BUF from Open parameters
                // If local port not specified
                //   Allocate a local port
                // Endif
                // Send <SEQ=SND.ISS><MAX=SND.MAX><MAXBUF=RMAX.BUF><SYN>
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
                p_con->state    = SYN_SENT;
                p_con->rbuf_max = rbuf_max_default;

                p_con->set_output_func(m_output_func);

                p_con->init_snd();
                p_con->init_swnd();


                // create syn packet
                packetbuf_ptr  pbuf = packetbuf::construct();
                rdp_syn       *syn;
                
                syn = (rdp_syn*)pbuf->append(sizeof(*syn));
                memset(syn, 0, sizeof(*syn));

                syn->head.flags  = flag_syn | flag_ver;
                syn->head.hlen   = (uint8_t)(sizeof(*syn) / 2);
                syn->head.sport  = htons(sport);
                syn->head.dport  = htons(dport);
                syn->head.seqnum = htonl(p_con->snd_nxt);

                syn->out_segs_max = htons(p_con->snd_max);
                syn->seg_size_max = htons(p_con->rbuf_max);

                set_syn_option_seq(syn->options, true);

                // enqueue
                if (! p_con->enqueue_swnd(pbuf))
                        return -1;


                // create descriptor
                int desc;

                do {
                        desc = random();
                } while (m_desc_set.find(desc) != m_desc_set.end());

                m_desc_set.insert(desc);
                m_addr2conn[addr] = p_con;
                m_desc2conn[desc] = p_con;

                // XXX
                // start retransmission timer

                return desc;
        }

        void
        rdp::input_dgram(id_ptr src, const void *buf, int len)
        {
                rdp_head *head;
                rdp_addr  addr;
                uint16_t  sport;
                uint16_t  dport;

                head = (rdp_head*)buf;

                sport = ntohs(head->sport);
                dport = ntohs(head->dport);

                addr.did   = src;
                addr.dport = dport;
                addr.sport = sport;


                boost::unordered_map<rdp_addr, rdp_con_ptr>::iterator it;
                it = m_addr2conn.find(addr);

                if (it != m_addr2conn.end()) {
                        rdp_con_ptr con = it->second;

                        switch (con->state) {
                        case CLOSE_WAIT:
                                in_state_closed_wait(con, addr, head,
                                                     len);
                                break;
                        case SYN_SENT:
                                in_state_syn_sent(con, addr, head, len);
                                break;
                        case SYN_RCVD:
                                in_state_syn_rcvd(con, addr, head, len);
                        case OPEN:
                                in_state_open(con, addr, head, len);
                        default:
                                break;
                        }
                } else if (m_listening.left.find(dport) !=
                           m_listening.left.end()) {
                        // LISTEN
                        in_state_listen(addr, head, len);
                } else {
                        // CLOSED
                        in_state_closed(addr, head, len);
                }
        }

        void
        rdp::in_state_closed_wait(rdp_con_ptr con, rdp_addr addr,
                                  rdp_head *head, int len)
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
        rdp::in_state_closed(rdp_addr addr, rdp_head *head, int len)
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
                //   Send <SEQ=0><RST><ACK=RCV.CUR><ACK>
                //   Discard segment
                //   Return
                // Endif
        }

        void
        rdp::in_state_listen(rdp_addr addr, rdp_head *head, int len)
        {
                // If RST set
                //   Discard the segment
                //   Return
                // Endif
                // 
                // If ACK or NUL set
                //   Send <SEQ=SEG.ACK + 1><RST>
                //   Return
                // Endif
                // 
                // If SYN set
                //   Set RCV.CUR = SEG.SEQ
                //   RCV.IRS = SEG.SEQ
                //   SND.MAX = SEG.MAX
                //   SBUF.MAX = SEG.BMAX
                //   Send <SEQ=SND.ISS><ACK=RCV.CUR><MAX=RCV.MAX>
                //        <BUFMAX=RBUF.MAX><ACK><SYN>
                //   Set State = SYN-RCVD
                //   Return
                // Endif
                // 
                // If anything else (should never get here)
                //   Discard segment
                //   Return
                // Endif

                if (head->flags & flag_ack || head->flags & flag_nul) {
                        // send rst
                        packetbuf_ptr  pbuf = packetbuf::construct();
                        rdp_head      *rst;
                        uint32_t       seg_ack;

                        seg_ack = ntohl(head->seqnum);
                        seg_ack++;

                        rst = (rdp_head*)pbuf->append(sizeof(*rst));
                        memset(rst, 0, sizeof(*rst));

                        rst->flags  = flag_rst | flag_ver;
                        rst->hlen   = sizeof(rst) / 2;
                        rst->sport  = htons(addr.dport);
                        rst->dport  = htons(addr.sport);
                        rst->acknum = htonl(seg_ack);

                        m_output_func(addr.did, pbuf);
                } else if (head->flags & flag_syn) {
                        // XXX
                        // create syn ack packet
                        // enqueue

                        // XXX
                        // create connection
                }
        }

        void
        rdp::in_state_syn_sent(rdp_con_ptr con, rdp_addr addr, rdp_head *head,
                               int len)
        {
                // If ACK set
                //   If RST clear and SEG.ACK != SND.ISS
                //     Send <SEQ=SEG.ACK + 1><RST>
                //   Endif
                //   Discard segment; Return
                // Endif
                //
                // If RST set
                //   If ACK set
                //     Signal "Connection Refused"
                //     Set State =  CLOSED
                //     Deallocate connection record
                //   Endif
                //   Discard segment
                //   Return
                // Endif
                //
                // If SYN set
                //   Set RCV.CUR = SEG.SEQ
                //   RCV.IRS = SEG.SEQ
                //   SND.MAX = SEG.MAX
                //   RBUF.MAX = SEG.BMAX
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
                //
                // If anything else
                //   Discard segment
                //   Return
                // Endif
        }

        void
        rdp::in_state_syn_rcvd(rdp_con_ptr con, rdp_addr addr, rdp_head *head,
                               int len)
        {
                // If RCV.IRS < SEG.SEQ =< RCV.CUR + (RCV.MAX * 2)
                //   Segment sequence number acceptable
                // else
                //   Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                //   Discard segment
                //   Return
                // Endif
                //
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
                // If EACK set
                //   Send <SEQ=SEG.ACK + 1><RST>
                //   Discard segment
                //   Return
                // Endif
                //
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
                //
                // If Data in segment or NUL set
                //   If the received segment is in sequence
                //     Copy the data (if any) to user buffers
                //     Set RCV.CUR=SEG.SEQ
                //     Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK>
                //   else
                //     If out-of-sequence delivery permitted
                //       Copy the data (if any) to user buffers
                //     Endif
                //     Send <SEQ=SND.NXT><ACK=RCV.CUR><ACK><EACK><RCVDSEQNO1>
                //          ...<RCVDSEQNOn>
                //   Endif
                // Endif
        }

        void
        rdp::in_state_open(rdp_con_ptr con, rdp_addr addr, rdp_head *head,
                           int len)
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
        rdp::set_syn_option_seq(uint16_t &options, bool sequenced)
        {
                if (sequenced)
                        options |= 0x8000;
                else
                        options &= 0x7fff;
        }

        void
        rdp_con::init_swnd()
        {
                m_swnd_len    = snd_max * 4;
                m_swnd_used   = 0;
                m_swnd_head   = 0;
                m_swnd_tail   = 0;
                m_swnd_ostand = 0;

                m_swnd = boost::shared_array<wnd>(new wnd[m_swnd_len]);
        }

        bool
        rdp_con::enqueue_swnd(packetbuf_ptr pbuf)
        {
                if (m_swnd_used >= m_swnd_len)
                        return false;

                int  pos = (m_swnd_head + m_swnd_used) % m_swnd_len;
                wnd *p_wnd;

                p_wnd = &m_swnd[pos];
                p_wnd->pbuf      = pbuf;
                p_wnd->sent_time = 0;
                p_wnd->is_acked  = false;
                p_wnd->is_sent   = false;
                p_wnd->seqnum    = snd_nxt;

                snd_nxt++;

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

        void
        rdp_con::recv_ack(uint32_t ack)
        {

        }

        void
        rdp_con::recv_eack(uint32_t eack)
        {
                uint32_t hseq = m_swnd[m_swnd_head].seqnum;
                uint32_t pos;

                if (eack > hseq) {
                        pos = eack - hseq;
                } else {
                        pos = eack + (0xffffffff - hseq) + 1;
                }

                if (pos > (uint32_t)m_swnd_used)
                        return;

                pos += m_swnd_head;
                if (pos >= (uint32_t)m_swnd_len)
                        pos %= m_swnd_len;

                wnd *p_wnd = &m_swnd[pos];
                if (p_wnd->is_sent) {
                        p_wnd->is_acked = true;
                        p_wnd->pbuf.reset();

                        m_swnd_ostand--;

                        if (pos == (uint32_t)m_swnd_head) {
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

                        // XXX
                        // send
                }
        }

        void
        rdp_con::init_snd()
        {
                snd_iss  = random();
                snd_nxt  = snd_iss;
                snd_una  = snd_iss;
                snd_max  = rdp::snd_max_default;
        }
}
