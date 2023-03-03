#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , RTO{retx_timeout} {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t res = 0;

    // Iterate through the outstanding map.
    auto iter = _outstanding_map.begin();
    for (; iter != _outstanding_map.end(); iter++)
        res += iter->second.length_in_sequence_space();
    return res;
}

void TCPSender::fill_window() {
    uint64_t window_size;
    uint64_t old_bytes = bytes_in_flight();

    // Tricky case: uint64_t means unsigned long long which will never be negatives
    // Therefore, if window size < flight bytes, we has to return now, or window size - old bytes
    // will be positive.
    if (_window_size < old_bytes)
        return;

    // Act like the window size is one.
    if (_window_size == 0)
        window_size = 1;
    else
        window_size = _window_size - old_bytes;

    while (window_size > 0) {
        // Construct the segment's header
        TCPHeader _header;
        _header.seqno = next_seqno();
        // The beigning case
        if (_next_seqno == 0)
            _syn_set = _header.syn = 1;

        // Construct the segment's payload
        size_t read_len = min(TCPConfig::MAX_PAYLOAD_SIZE, window_size - _header.syn);
        Buffer _payload = Buffer(_stream.read(read_len));

        // Tail-case
        // NOTE: only set _fin when there is enough space.
        if (!_fin_set && _stream.eof() && window_size > _payload.size())
            _header.fin = _fin_set = 1;

        // Combine segment
        TCPSegment segment;
        // Something special for Left-value. "&"
        segment.header() = _header;
        segment.payload() = _payload;

        // Pointless segment
        if (segment.length_in_sequence_space() == 0)
            break;
        
        // DEBUG fsm_passive_close: re-transmit FIN.
        if(_outstanding_map.empty()){
            RTO = _initial_retransmission_timeout;
            _time_pass = 0;
        }

        // Enter the outgoing queue.
        _segments_out.push(segment);

        // Enter the outstanding-map
        _outstanding_map[_next_seqno] = segment;

        // Narrow the window size.
        window_size -= segment.length_in_sequence_space();

        // Renew the next seqno
        _next_seqno += segment.length_in_sequence_space();

        // If reach end, break.
        if (_fin_set)
            break;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t _abs_ackno = unwrap(ackno, _isn, _ackno);

    // "_ackno" refers to the last received ackno.
    if (_abs_ackno < _ackno || _abs_ackno > _next_seqno)
        return;

    // Update
    _ackno = _abs_ackno;
    _window_size = window_size;

    // Iterate through the outstanding map, if any data has been acknowledged
    // set RTO back to the inital value and consecutive-nums to zero.
    auto iter = _outstanding_map.begin();
    for (; iter != _outstanding_map.end();) {
        // Fully acknowledged
        if (iter->first + iter->second.length_in_sequence_space() <= _ackno) {
            // NOTE: map.erase() return next iter and the inner structure of map data structure is
            // red-black tree which indicates the map is *ordered*.
            iter = _outstanding_map.erase(iter);
            RTO = _initial_retransmission_timeout;
            _consecutive_nums = 0;
            _time_pass = 0;
            // I forget to add "continue" which once caused a bug.
            continue;
        }
        // Save the time
        // If iterate the whole map, time-out warning.
        break;
    }
    // Maybe the founction has set aside some space for segments.
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    // Update timer
    _time_pass += ms_since_last_tick;

    // If reach the RTO limit, alarm!
    if (_time_pass >= RTO) {
        // Alarm!
        // Retransmit the earliest segment in the outstanding map.
        auto iter = _outstanding_map.begin();
        if (iter != _outstanding_map.end()) {
            _segments_out.push(iter->second);
            // Keep track of the number of consecutive retransmissions.
            _consecutive_nums += 1;
        }
        // Reset the retransmission timer.
        _time_pass = 0;

        // Exponential backoff
        if (_window_size != 0)
            RTO *= 2;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_nums; }

// This is useful if the owner want to send an empty ACK segment.
void TCPSender::send_empty_segment() {
    TCPSegment segment;
    segment.header().seqno = next_seqno();
    _segments_out.push(segment);
}
