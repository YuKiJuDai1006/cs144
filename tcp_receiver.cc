#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    uint64_t index;
    // Choose the end of "bytestream" to be the checkpoint
    uint64_t checkpoint = _reassembler.stream_out().bytes_written();
    // The beginning
    // NOTE: unwrap ***doesn't need to subtract 1***.
    if (seg.header().syn) {
        _isn = seg.header().seqno.raw_value();
        _isn_set = 1;
        index = unwrap(seg.header().seqno, WrappingInt32{_isn}, checkpoint);
        _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
        return;
    }
    // Segs before the beginning should be disposeds.
    if (!_isn_set)
        return;
    // The body and tail segs
    // NOTE: unwrap need to ***subtract 1*** to be the real index for "bytestream".
    index = unwrap(seg.header().seqno - 1, WrappingInt32{_isn}, checkpoint);
    // NOTE: seg.payload().copy() has the offset to eliminate the effect of SYN and FIN
    _reassembler.push_substring(seg.payload().copy(), index, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_isn_set == 0)
        return nullopt;
    // The body-segs should ***add 1*** for SYN
    size_t towrap = _reassembler.stream_out().bytes_written() + 1;
    // The tail-seg should ***add 2*** for both SYN and FIN
    if (_reassembler.stream_out().input_ended())
        return wrap(towrap + 1, WrappingInt32{_isn});
    return wrap(towrap, WrappingInt32{_isn});
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
