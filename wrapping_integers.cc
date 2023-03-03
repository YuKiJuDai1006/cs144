#include "wrapping_integers.hh"
// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    uint64_t tmp = 1l << 32;
    n += isn.raw_value();
    n %= tmp;
    return WrappingInt32{uint32_t(n)};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.

uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // Be careful with the "l"
    uint64_t gap = 1l << 32;
    uint32_t start = n - isn;
    // I make a stupid mistake with "/" and "%", which confused me two hours.
    uint64_t left = checkpoint / gap;
    uint64_t right = left + 1;
    uint64_t leftpos, midpos, rightpos, leftdis, rightdis, middis, res;
    // The main principle: Find 3 positions which own the minimum distance with checkpoint
    // and then compare them.

    // Two corner cases
    if (left == 0) {
        // finding
        rightpos = right * gap + start;
        rightdis = rightpos - checkpoint;
        midpos = left * gap + start;
        middis = checkpoint > midpos ? checkpoint - midpos : midpos - checkpoint;
        // make comparsion
        res = middis > rightdis ? rightdis : middis;
        if (res == middis)
            return midpos;
        return rightpos;
    }
    if (right == gap) {
        // finding
        leftpos = (left - 1) * gap + start;
        leftdis = checkpoint - leftpos;
        midpos = left * gap + start;
        middis = checkpoint > midpos ? checkpoint - midpos : midpos - checkpoint;
        // make comparsion
        res = middis > leftdis ? leftdis : middis;
        if (res == middis)
            return midpos;
        return leftpos;
    }
    // Body cases
    else {
        // finding
        leftpos = (left - 1) * gap + start;
        leftdis = checkpoint - leftpos;
        rightpos = right * gap + start;
        rightdis = rightpos - checkpoint;
        midpos = left * gap + start;
        middis = checkpoint > midpos ? checkpoint - midpos : midpos - checkpoint;
        // make comparison
        res = middis > leftdis ? leftdis : middis;
        res = res > rightdis ? rightdis : res;
        if (res == middis)
            return midpos;
        if (res == leftdis)
            return leftpos;
        return rightpos;
    }
}
