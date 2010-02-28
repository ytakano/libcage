#ifndef RDP_HPP
#define RDP_HPP

#include<stdint.h>
#include<time.h>

#include<vector>

enum rdp_state {
        CLOSED,
        LISTEN,
        SYN_SENT,
        SYN_RCVD,
        OPEN,
        CLOSE_WAIT,
};

class rdp {
public:
        rdp();
        virtual ~rdp();

        int             open();
        void            close(int id);
        uint32_t        send(int id, const void *buf, int len);
        void            receive(int id, void *buf, int *len);
        rdp_state       status(int id);


        void            input_dgram(const void *buf, int len);
        
private:
        rdp_state       state;     // The current state of the connection.
        time_t          closewait; // A timer used to time out the CLOSE-WAIT
                                   // state.
        uint32_t        sbuf_max;  // The largest possible segment (in octets)
                                   // that can legally be sent. This variable is
                                   // specified by the foreign host in the SYN
                                   // segment during connection establishment.
        uint32_t        rbuf_max;  // The largest possible segment (in octets)
                                   // that can be received. This variable is
                                   // specified by the user when the connection
                                   // is opened. The variable is sent to the
                                   // foreign host in the SYN segment.

        // Send Sequence Number Variables:
        uint32_t        snd_nxt; // The sequence number of the next segment
                                 // that is to be sent.
        uint32_t        snd_una; // The sequence number of the oldest
                                 // unacknowledged segment.
        uint32_t        snd_max; // The maximum number of outstanding
                                 // (unacknowledged) segments that can be sent.
                                 // The sender should not send more than this
                                 // number of segments without getting an
                                 // acknowledgement.
        uint32_t        snd_iss; // The initial send sequence number. This is
                                 // the  sequence number that was sent in the
                                 // SYN segment.

        // Receive Sequence Number Variables:
        uint32_t        rcv_cur; // The sequence number of the last segment
                                 // received correctly and in sequence.
        uint32_t        rcv_max; // The maximum number of segments that can be
                                 // buffered for this connection.
        std::vector<uint32_t>   rcvdsendq; // The array of sequence numbers of
                                           // segments that have been received
                                           // and acknowledged out of sequence.

        // Variables from Current Segment:
        uint32_t        seg_seq;  // The sequence number of the segment
                                  // currently being processed.
        uint32_t        seg_qck;  // The acknowledgement sequence number in the
                                  // segment currently being processed.
        uint32_t        seg_max;  // The maximum number of outstanding segments
                                  // the receiver is willing to hold, as
                                  // specified in the SYN segment that
                                  // established the connection.
        uint32_t        seg_bmax; // The maximum segment size (in octets)
                                  // accepted by the foreign host on a
                                  // connection, as specified in the SYN segment
                                  // that established the connection.
};

#endif // RDP_HPP
