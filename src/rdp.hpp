#ifndef RDP_HPP
#define RDP_HPP

#include "cagetypes.hpp"
#include "common.hpp"
#include "packetbuf.hpp"

#include <stdint.h>
#include <time.h>

#include <vector>
#include <functional>

#include <boost/bimap/bimap.hpp>
#include <boost/bimap/unordered_set_of.hpp>
#include <boost/function.hpp>
#include <boost/shared_array.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#define RDP_VER 2

namespace libcage {
        enum rdp_event {
                ESTABLISHED_FROM,
                ESTABLISHED,
                REFUSED,
                RESET,
                READY_READ,
        };

        enum rdp_state {
                CLOSED,
                LISTEN,
                SYN_SENT,
                SYN_RCVD,
                OPEN,
                CLOSE_WAIT,
        };

        struct rdp_head {
                uint8_t  flags;
                uint8_t  hlen;
                uint16_t sport;
                uint16_t dport;
                uint16_t dlen;
                uint32_t seqnum;   // SEG.SEQ
                uint32_t acknum;   // SEG.ACK
                uint32_t checksum;
        };

        struct rdp_syn {
                rdp_head head;
                uint16_t out_segs_max; // SEG.MAX
                uint16_t seg_size_max; // SEG.BMAX
                uint16_t options;
                uint16_t padding;
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

        size_t hash_value(const rdp_addr &addr);

        typedef boost::function<void (id_ptr, packetbuf_ptr)> callback_dgram_out;
        typedef boost::function<void (int desc, rdp_addr addr,
                                      rdp_event event)> callback_rdp_event;


        class rdp {
        public:
                static const uint8_t   flag_syn;
                static const uint8_t   flag_ack;
                static const uint8_t   flag_eak;
                static const uint8_t   flag_rst;
                static const uint8_t   flag_nul;
                static const uint8_t   flag_ver;

                static const uint16_t  syn_opt_in_seq;

                static const uint32_t  rbuf_max_default;
                static const uint32_t  rcv_max_default;
                static const uint16_t  well_known_port_max;

                rdp();
                virtual ~rdp();

                int             listen(uint16_t sport); // passive open
                int             connect(uint16_t sport, id_ptr did,
                                        uint16_t dport); // active open
                void            close(int desc);
                int             send(int desc, const void *buf, int len);
                void            receive(int desc, void *buf, int *len);
                rdp_state       status(int desc);

                void            set_callback_rdp_event(callback_rdp_event func);

                void            input_dgram(id_ptr src, packetbuf_ptr pbuf);
                void            in_state_closed(rdp_addr addr,
                                                packetbuf_ptr pbuf);
                void            in_state_listen(rdp_addr addr,
                                                packetbuf_ptr pbuf);
                void            in_state_closed_wait(rdp_con_ptr con,
                                                     rdp_addr addr,
                                                     packetbuf_ptr pbuf);
                void            in_state_syn_sent(rdp_con_ptr p_con,
                                                  rdp_addr addr,
                                                  packetbuf_ptr pbuf);
                void            in_state_syn_rcvd(rdp_con_ptr p_con,
                                                  rdp_addr addr,
                                                  packetbuf_ptr pbuf);
                void            in_state_open(rdp_con_ptr con, rdp_addr addr,
                                              packetbuf_ptr pbuf);
                void            set_callback_dgram_out(callback_dgram_out func);

        private:
                typedef boost::bimaps::unordered_set_of<uint16_t> _uint16_set;
                typedef boost::bimaps::unordered_set_of<int>      _int_set;
                typedef boost::bimaps::bimap<_uint16_set,
                                             _int_set>            listening_t;
                typedef boost::bimaps::bimap<_uint16_set,
                                             _int_set>::value_type listening_val;

                boost::unordered_set<int>       m_desc_set;
                listening_t                     m_listening; // <port, desc>
                
                boost::unordered_map<rdp_addr, rdp_con_ptr>     m_addr2conn;
                boost::unordered_map<int, rdp_con_ptr>          m_desc2conn;

                callback_dgram_out         m_output_func;
                callback_rdp_event         m_event_func;

                void            set_syn_option_seq(uint16_t &options,
                                                   bool sequenced);
                int             generate_desc();
        };

        class rdp_con {
        public:
                rdp_addr        addr;
                int             desc;
                bool            is_pasv;
                bool            is_in_seq;

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
                std::vector<uint32_t>   rcvdseqno; // The array of sequence
                                                   // numbers of segments that
                                                   // have been received and
                                                   // acknowledged out of
                                                   // sequence.

                packetbuf_ptr   syn_pbuf;
                time_t          syn_time;
                int             syn_num;

                void            init_swnd();
                void            init_rwnd();
                bool            enqueue_swnd(packetbuf_ptr pbuf);

                void            set_output_func(callback_dgram_out func);

                void            recv_ack(uint32_t ack);
                void            recv_eack(uint32_t eack);

        private:
                class swnd {
                public:
                        packetbuf_ptr   pbuf;
                        time_t          sent_time;
                        bool            is_acked;
                        bool            is_sent;
                        uint32_t        seqnum;
                };

                boost::shared_array<swnd>        m_swnd;
                int             m_swnd_len;
                int             m_swnd_used;
                int             m_swnd_head;
                int             m_swnd_tail;
                uint32_t        m_swnd_ostand;

                callback_dgram_out      m_output_func;

                uint32_t        num_from_head(uint32_t seqnum);
                int             seq2pos(uint32_t num);
                void            ack_ostand(int pos);


                class rwnd {
                public:
                        packetbuf_ptr   pbuf;
                        uint32_t        seqnum;
                        bool            is_used;

                        rwnd() : is_used(false) { }
                };

                boost::shared_array<rwnd>       m_rwnd;
                int             m_rwnd_len;
                int             m_rwnd_head;
                int             m_rwnd_used;
        };
}

#endif // RDP_HPP
