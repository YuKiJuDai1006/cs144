#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    // DEBUG fsm_ack_rst: "test 1 failed: seg queued on early seqno" and "test 1 failed: seg queued on late seqno".
    size_t tmp = index > _output.bytes_read() ? index - _output.bytes_read() : 0;
    if (tmp >= _capacity) return;
    if (eof) {
        _eof_sign = 1;
        _eof = index + data.length();
    }
    // may overlapped
    if (index <= _end) {
        if (index + data.length() > _end) {  // overlapped
            gomap(data, index);
            prolong();
        }
    } else
        gomap(data, index);  // bigger than current _end

    if (_eof_sign == 1 && _end >= _eof)
        _output.end_input();
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t res = 0;
    // iterate hole map
    for (auto iter = _buf.begin(); iter != _buf.end(); iter++) {
        res += iter->second.length();
    }
    return res;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }

// Helper Method
void StreamReassembler::gomap(const string &data, const size_t index) {
    // some corner cases
    if (_buf.size() == 0) {
        _buf[index] = data;
        return;
    }
    auto iter = _buf.lower_bound(index);
    // every map-item's index all smaller than data's index
    if (iter == _buf.end()) {
        iter--;
        if (iter->first + iter->second.length() > index) {
            // partly overlapped
            if (iter->first + iter->second.length() < index + data.length()) {
                string newdata = iter->second.substr(0, index - iter->first);
                // update
                _buf[iter->first] = newdata;
                // insert new data
                _buf[index] = data;
            }
        } else {
            _buf[index] = data;
        }
        return;
    }

    // every map-item's index all bigger than data's index
    if (iter == _buf.begin()) {
        while (iter != _buf.end()) {
            const size_t data_end_pox = index + data.length();
            // overlapped
            if (data_end_pox > iter->first) {
                // partly overlapped
                if (data_end_pox < iter->first + iter->second.length()) {
                    if (index < iter->first) {
                        string newdata = data.substr(0, iter->first - index);
                        // insert new data
                        _buf[index] = newdata;
                    }
                    return;
                }
                // totally overlaped
                else {
                    iter = _buf.erase(iter);
                    continue;
                }
            } else
                break;
        }
        _buf[index] = data;
        return;
    }

    // iter is in the middle place
    iter--;
    if (iter->first + iter->second.length() < index + data.length()) {
        string newdata = data;
        size_t newindex = index;
        if (iter->first + iter->second.length() > index) {
            // front item
            size_t newpos = iter->first + iter->second.length() - index;
            newdata = data.substr(newpos, data.length() - newpos);
            newindex = index + newpos;
        }
        // back item
        iter++;
        while (iter != _buf.end()) {
            const size_t data_end_pox = newindex + newdata.length();
            // overlapped
            if (data_end_pox > iter->first) {
                // partly overlapped
                if (data_end_pox < iter->first + iter->second.length()) {
                    if (newindex < iter->first) {
                        string res = newdata.substr(0, iter->first - newindex);
                        // insert new data
                        _buf[newindex] = res;
                    }
                    return;
                }
                // totally overlaped
                else {
                    iter = _buf.erase(iter);
                    continue;
                }
            } else
                break;
        }
        _buf[newindex] = newdata;
        return;
    }
}

size_t StreamReassembler::totalsize() {
    size_t res;
    res = _output.buffer_size() + unassembled_bytes();
    return res;
}

void StreamReassembler::prolong() {
    for (auto iter = _buf.begin(); iter != _buf.end();) {
        if (iter->first <= _end) {
            if (iter->first + iter->second.length() > _end) {  // go to the assembled part
                size_t towrite = iter->first + iter->second.length() - _end;
                _end += _output.write(iter->second.substr(_end - iter->first, towrite));
            }
            // erase the overlapped item
            iter = _buf.erase(iter);
            continue;
        } else
            break;
    }

    if (_eof_sign == 1 && _end >= _eof) {
        _output.end_input();
    }
}
