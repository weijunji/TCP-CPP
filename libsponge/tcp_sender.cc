#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>
#include <iostream>
#include <cassert>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) { }

uint64_t TCPSender::bytes_in_flight() const {
    return _next_seqno - _expect_ack;
}

void TCPSender::fill_window() {
    if (!_syn_sent) {
        TCPSegment seg;
        seg.header().syn = true;
        seg.header().seqno = wrap(0, _isn);
        _segments_out.push(seg);
        _seg_not_ack.push(seg);
        _next_seqno = 1;
        _retrans_timer = _tick + _initial_retransmission_timeout;
        _syn_sent = true;
    }
    uint64_t remain = _window_size - bytes_in_flight();
    bool send = false;
    if (_expect_ack != 0) {
        // SYN received
        while (remain > 0 && _stream.buffer_size() > 0) {
            // send segment
            uint64_t send_bytes = min(remain, TCPConfig::MAX_PAYLOAD_SIZE);
            string payload = _stream.read(send_bytes);
            TCPSegment seg;
            seg.header().seqno = wrap(_next_seqno, _isn);
            seg.payload() = move(payload);
            _next_seqno += seg.length_in_sequence_space();
            remain = _window_size - bytes_in_flight();
            if (_stream.eof() && remain > 0 && !_fin_sent) {
                seg.header().fin = true;
                _next_seqno += 1;
                _fin_sent = true;
            }
            _segments_out.push(seg);
            _seg_not_ack.push(seg);
            send = true;
        }
    }
    if (_stream.eof() && remain > 0 && !_fin_sent) {
        // send FIN
        TCPSegment seg;
        seg.header().fin = true;
        seg.header().seqno = wrap(_next_seqno, _isn);
        _segments_out.push(seg);
        _seg_not_ack.push(seg);
        _next_seqno += 1;
        _fin_sent = true;
        send = true;
    }

    if (send && _retrans_timer == 0) {
        // open timer
        _retrans_timer = _tick + _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;
        _rto_back_off = 0;
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _window_size = window_size;
    _do_back_off = 1;
    if (_window_size == 0) {
        _window_size = 1;
        _do_back_off = 0;
    }
    uint64_t ack = unwrap(ackno, _isn, _expect_ack);
    if (ack <= _next_seqno && ack > _expect_ack) {
        if (ack == _expect_ack) {
            _same_ack++;
        } else {
            _same_ack = 0;
        }
        _expect_ack = ack;
        if (bytes_in_flight() == 0) {
            // close timer
            _retrans_timer = 0;
            _consecutive_retransmissions = 0;
            _rto_back_off = 0;
        } else {
            // reopen timer
            _retrans_timer = _tick + _initial_retransmission_timeout;
            _consecutive_retransmissions = 0;
            _rto_back_off = 0;
        }
    }

    // remove all acked packets
    while (!_seg_not_ack.empty()) {
        TCPSegment seg = _seg_not_ack.front();
        if (seg.length_in_sequence_space() + unwrap(seg.header().seqno, _isn, _expect_ack) <= _expect_ack) {
            _seg_not_ack.pop();
        } else {
            break;
        }
    }

    // faster retransmit
    if (_same_ack == 3 && !_seg_not_ack.empty()) {
        // cout << "!! FASTER RETRANSMIT" << endl;
        _same_ack = 0;
        TCPSegment seg = _seg_not_ack.front();
        _segments_out.push(seg);
        _consecutive_retransmissions += 1;
        _rto_back_off += _do_back_off;
        _retrans_timer += _initial_retransmission_timeout << _rto_back_off;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _tick += ms_since_last_tick;
    
    if (!_seg_not_ack.empty() && _tick >= _retrans_timer) {
        // retransmit the first packet
        // cout << "retransmit" << endl;
        TCPSegment seg = _seg_not_ack.front();
        _segments_out.push(seg);
        _consecutive_retransmissions += 1;
        _rto_back_off += _do_back_off;
        _retrans_timer = _tick + (_initial_retransmission_timeout << _rto_back_off);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    _segments_out.push(seg);
}
