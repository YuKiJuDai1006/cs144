#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_received; }


void TCPConnection::segment_received(const TCPSegment &seg){
    // Clear the time.
    _time_received = 0;

    // If seg occupied any seqno, at least one segment is sent in reply for ACK.
    bool need_ack_send = seg.length_in_sequence_space();
    // If the rst (reset) flag is set, sets both the inbound and outbound streams
    // to the error state and kills the connection permanently.
    // Unclean shut down
    if(seg.header().rst){
        // Set error flag on inbound and outbound byte-streams.
        // Kill the connection permanently.
        set_rst();
        return;
    }

    // Send seg to TCP-receiver.
    _receiver.segment_received(seg);

    // If ack flag is set, send the seg's info to TCP-sender. 
    if(seg.header().ack){ 
        _sender.ack_received(seg.header().ackno, seg.header().win);
        // If some data need to send, there is no need to send a empty segment for ACK.
        if(!_sender.segments_out().empty())
            need_ack_send = 0;
    }

    // If receiver has get SYN, sender should also start. 
    if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::SYN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::CLOSED){
        connect();
        return;
    }

    // If the TCPConnection’s inbound stream ends before
    // the TCPConnection has ever sent a fin segment, then the TCPConnection
    // doesn’t need to linger after both streams finish.
    if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::SYN_ACKED){
        _linger_after_streams_finish = 0;
    }

    // Don't need to linger.
    // Clean shut down.
    if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        !_linger_after_streams_finish){
        _active = 0;
        return;
    }

    // ACK for any segments owning seqno and respond to a "keep-alive" segment.
    if(need_ack_send)
        _sender.send_empty_segment();

    // Pop from sender's queue and push into another queue for sending to peers.
    send_segments();
}


bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    size_t res = _sender.stream_in().write(data);

    // Send the data over TCP if possible.
    _sender.fill_window();
    // Send segments to peers.
    send_segments();
    return res;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick){
    // Update the time.
    _time_received += ms_since_last_tick;

    // Tell the TCP-sender about the passage of time.
    _sender.tick(ms_since_last_tick);

    // If consecutive-nums exceed MAX_RETX_ATTEMPTS, abort the connection.
    if(_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS){
        // Abort the connection.
        set_rst();

        // Send a reset segment to the peer
        // Pop segment which may be re-transmit.
        _sender.segments_out().pop();
        _sender.send_empty_segment();
        _sender.segments_out().back().header().rst = 1;
        send_segments();
        return;
    }
    send_segments();
    // If ... , linger and then end the connection cleanly.
    if(TCPState::state_summary(_receiver) == TCPReceiverStateSummary::FIN_RECV &&
        TCPState::state_summary(_sender) == TCPSenderStateSummary::FIN_ACKED &&
        _linger_after_streams_finish && _time_received >= 10 * _cfg.rt_timeout){
        _active = 0;
        _linger_after_streams_finish = 0;
    }
}

void TCPConnection::end_input_stream(){
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_segments();
}

// Send SYN and initiate the connection. 
void TCPConnection::connect(){
    _active = 1;
    _sender.fill_window();
    send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            // Abort the connection.
            set_rst();

            // Send a reset segment to the peer
            _sender.send_empty_segment();
            _sender.segments_out().back().header().rst = 1;
            send_segments();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_segments(){
    while(!_sender.segments_out().empty()){
        // Ackno field
        if(_receiver.ackno().has_value()){
            _sender.segments_out().front().header().ack = 1;
            _sender.segments_out().front().header().ackno = _receiver.ackno().value();
        }

        // Window-size field
        // Set uint_16_t max as upper bound.
        uint16_t win_size = _receiver.window_size() > std::numeric_limits<uint16_t>::max() ?
            std::numeric_limits<uint16_t>::max() : _receiver.window_size();
        _sender.segments_out().front().header().win = win_size;
        
        // Pop from sender's queue and push into another queue for sending to peers.
        _segments_out.push(_sender.segments_out().front());
        _sender.segments_out().pop();
    }
}

void TCPConnection::set_rst(){
    // Set both inbound and outbound byte-stream to error state.
    // Unclean shot down.
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
    _active = 0;
    _linger_after_streams_finish = 0;
}
