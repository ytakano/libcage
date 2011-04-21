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

#ifndef RDP_HPP
#define RDP_HPP

#include "cagetime.hpp"
#include "cagetypes.hpp"
#include "common.hpp"
#include "packetbuf.hpp"
#include "timer.hpp"

#include <stdint.h>
#include <time.h>

#include <functional>
#include <map>
#include <set>
#include <vector>
#include <queue>

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <boost/function.hpp>
#include <boost/random.hpp>
#include <boost/shared_array.hpp>
#include <boost/unordered_map.hpp>

//#define DEBUG_RDP

namespace libcage {
        enum rdp_event {
                ACCEPTED,
                CONNECTED,
                REFUSED,
                RESET,
                FAILED,
                READY2READ,
                BROKEN,
        };

        enum rdp_state {
                CLOSED,
                LISTEN,
                SYN_SENT,
                SYN_RCVD,
                OPEN,
                CLOSE_WAIT_PASV,
                CLOSE_WAIT_ACTIVE,
        };

        struct rdp_head {
                uint8_t  flags;
                uint8_t  hlen;
                uint16_t sport;
                uint16_t dport;
                uint16_t dlen;
                uint32_t seqnum;   // SEG.SEQ
                uint32_t acknum;   // SEG.ACK
                uint32_t reserved;
        };

        struct rdp_syn {
                rdp_head head;
                uint16_t out_segs_max; // SEG.MAX
                uint16_t seg_size_max; // SEG.BMAX
        };

        class rdp_con;
        typedef boost::shared_ptr<rdp_con> rdp_con_ptr;

        class rdp_addr {
        public:
                id_ptr          did;   // destination id
                uint16_t        dport; // destination port
                uint16_t        sport; // source port

                bool operator== (const rdp_addr &addr) const
                {
                        return *did == *addr.did && dport == addr.dport &&
                                sport == addr.sport;
                }
        };

        struct rdp_status {
                rdp_state    state;
                id_const_ptr did;
                uint16_t     dport;
                uint16_t     sport;
        };

        size_t hash_value(const rdp_addr &addr);

        typedef boost::function<void (id_ptr, packetbuf_ptr)> callback_dgram_out;
        typedef boost::function<void (int desc, rdp_addr addr,
                                      rdp_event event)> callback_rdp_event;


        class rdp {
                static const uint8_t   flag_syn;
                static const uint8_t   flag_ack;
                static const uint8_t   flag_eak;
                static const uint8_t   flag_rst;
                static const uint8_t   flag_nul;
                static const uint8_t   flag_fin;
                static const uint8_t   flag_ver;

                static const uint32_t  rbuf_max_default;
                static const uint32_t  rcv_max_default;
                static const uint16_t  well_known_port_max;
                static const uint16_t  sbuf_limit;
                static const uint32_t  timer_rdp_usec;
                static const double    ack_interval;

        public:
                rdp(rand_uint &rnd, timer &tm);
                virtual ~rdp();

                int             listen(uint16_t sport,
                                       callback_rdp_event func); // passive open
                int             connect(uint16_t sport, id_ptr did,
                                        uint16_t dport,
                                        callback_rdp_event func); // active open
                void            close(int desc);
                int             send(int desc, const void *buf, int len);
                void            receive(int desc, void *buf, int *len);
                rdp_state       get_desc_state(int desc);
                void            get_status(std::vector<rdp_status> &vec);

                void            set_max_retrans(time_t sec);
                time_t          get_max_retrans();

                void            set_callback_rdp_event(int desc,
                                                       callback_rdp_event func);
                void            set_callback_dgram_out(callback_dgram_out func);
                void            input_dgram(id_ptr src, packetbuf_ptr pbuf);

        private:
                class timer_rdp : public timer::callback {
                public:
                        virtual void operator() ();

                        timer_rdp(rdp &r) : m_rdp(r) { }

                        rdp    &m_rdp;
                };

                typedef boost::bimaps::set_of<uint16_t> _uint16_set;
                typedef boost::bimaps::set_of<int>      _int_set;
                typedef boost::bimaps::bimap<_uint16_set,
                                             _int_set>            listening_t;
                typedef boost::bimaps::bimap<_uint16_set,
                                             _int_set>::value_type listening_val;

                std::set<int>               m_desc_set;
                listening_t                 m_listening; // <port, desc>
                
                boost::unordered_map<rdp_addr, rdp_con_ptr>     m_addr2conn;
                std::map<int, rdp_con_ptr>          m_desc2conn;
                std::map<int, callback_rdp_event>   m_desc2event;

                
                callback_dgram_out          m_output_func;

                rand_uint                  &m_rnd;

                time_t                      m_max_retrans;
                timer                      &m_timer;
                timer_rdp                   m_timer_rdp;

                bool            m_is_invoke;
                std::set<int>   m_desc_closed;

                void            output(id_ptr id, packetbuf_ptr pbuf);
                int             generate_desc();
                void            invoke_event(int desc1, int desc2,
                                             rdp_addr addr, rdp_event event);

                void            in_state_closed(rdp_addr addr,
                                                packetbuf_ptr pbuf);
                void            in_state_listen(rdp_addr addr,
                                                packetbuf_ptr pbuf);
                void            in_state_close_wait_pasv(rdp_con_ptr p_con,
                                                         rdp_addr addr,
                                                         packetbuf_ptr pbuf);
                void            in_state_close_wait_active(rdp_con_ptr p_con,
                                                           rdp_addr addr,
                                                           packetbuf_ptr pbuf);
                void            in_state_syn_sent(rdp_con_ptr p_con,
                                                  rdp_addr addr,
                                                  packetbuf_ptr pbuf);
                void            in_state_syn_rcvd(rdp_con_ptr p_con,
                                                  rdp_addr addr,
                                                  packetbuf_ptr pbuf);
                void            in_state_open(rdp_con_ptr p_con, rdp_addr addr,
                                              packetbuf_ptr pbuf);

                friend class rdp_con;
        };

        class rdp_con {
        public:
                rdp_addr        addr;
                int             desc;
                bool            is_pasv;
                bool            is_closed;

                rdp_state       state;     // The current state of the
                                           // connection.
                time_t          closewait; // A timer used to time out the
                                           // CLOSE-WAIT state.
                uint32_t        sbuf_max;  // The largest possible segment (in
                                           // octets) that can legally be sent.
                                           // This variable is specified by the
                                           // foreign host in the SYN segment
                                           // during connection establishment.
                uint32_t        rbuf_max;  // The largest possible segment (in
                                           // octets) that can be received. This
                                           // variable is specified by the user
                                           // when the connection is opened. The
                                           // variable is sent to the foreign
                                           // host in the SYN segment.

                // Send Sequence Number Variables:
                uint32_t        snd_nxt; // The sequence number of the next
                                         // segment that is to be sent.
                uint32_t        snd_una; // The sequence number of the oldest
                                         // unacknowledged segment.
                uint32_t        snd_max; // The maximum number of outstanding
                                         // (unacknowledged) segments that can
                                         // be sent. The sender should not send
                                         // more than this number of segments
                                         // without getting an acknowledgement.
                uint32_t        snd_iss; // The initial send sequence number.
                                         // This is the sequence number that was
                                         // sent in the SYN segment.

                // Receive Sequence Number Variables:
                uint32_t        rcv_cur; // The sequence number of the last
                                         // segment received correctly and in
                                         // sequence.
                uint32_t        rcv_max; // The maximum number of segments that
                                         // can be buffered for this connection.
                uint32_t        rcv_irs; // The initial receive sequence number.
                                         // This is  the  sequence number of the
                                         // SYN segment that established this
                                         // connection.
                uint32_t        rcv_ack; // The sequence number last acked

                cagetime        acked_time;

                std::vector<uint32_t>   rcvdseqno; // The array of sequence
                                                   // numbers of segments that
                                                   // have been received and
                                                   // acknowledged out of
                                                   // sequence.

                time_t          syn_time;
                time_t          syn_tout;

                time_t          rst_time;
                time_t          rst_tout;
                bool            is_retry_rst;

                void            init_swnd();
                bool            enqueue_swnd(packetbuf_ptr pbuf);
                void            send_ostand_swnd();

                void            init_rwnd();

                bool            retransmit();

                void            set_output_func(callback_dgram_out func);
                void            set_event_func(callback_rdp_event func);

                void            recv_ack(uint32_t acknum);
                void            recv_eack(uint32_t eacknum);

                void            delayed_ack();

                std::queue<packetbuf_ptr>       rqueue; // read queue


                rdp            &ref_rdp;

                rdp_con(rdp &r) : ref_rdp(r) { }

        private:
                class swnd {
                public:
                        packetbuf_ptr   pbuf;
                        time_t          sent_time;
                        bool            is_acked;
                        bool            is_sent;
                        uint32_t        seqnum;
                        time_t          rt_sec;
                };

                boost::shared_array<swnd>        m_swnd;
                int             m_swnd_len;
                int             m_swnd_head;
                int             m_swnd_used;
                int             m_swnd_ostand; // index of outstanding data

                class rwnd {
                public:
                        packetbuf_ptr   pbuf;
                        uint32_t        seqnum;
                        bool            is_used;
                        bool            is_eacked;

                        rwnd() : is_used(false), is_eacked(false) { }
                };

                boost::shared_array<rwnd>       m_rwnd;
                int             m_rwnd_len;
                int             m_rwnd_head;
                int             m_rwnd_used;

        public:
                void            rwnd_recv_data(packetbuf_ptr pbuf,
                                               uint32_t seqnum);
        };
}

#endif // RDP_HPP
