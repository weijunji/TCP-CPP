#include "tcp_receiver.hh"
#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (!_isn.has_value()) {
        if (seg.header().syn) {
            _isn = seg.header().seqno + 1;
        } else {
            std::cerr << "Error: connection not build" << std::endl;
            return;
        }
    }

    bool eof = seg.header().fin;
    std::string&& payload = std::string(seg.payload().str());
    uint64_t index = unwrap(seg.header().seqno + (seg.header().syn ? 1 : 0), _isn.value(), _reassembler.expect());
    std::cerr << "Isn " << _isn.value() << " seq " << seg.header().seqno << std::endl;
    std::cerr << "Recv " << payload << " at " << index << " expect " << _reassembler.expect() << std::endl;
    _reassembler.push_substring(payload, index, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_isn.has_value()) {
        return { wrap(_reassembler.expect(), _isn.value()) + (_reassembler.stream_out().input_ended() ? 1 : 0) };
    } else {
        return std::nullopt;
    }
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
