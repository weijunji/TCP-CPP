#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

void TCPConnection::send_all_segments() {
    if (_closed) return;
    while (!_sender.segments_out().empty()) {
        TCPSegment& seg = _sender.segments_out().front();
        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
        }
        size_t max_win = numeric_limits<uint16_t>().max();
        seg.header().win = min(_receiver.window_size(), max_win);
        _segments_out.push(seg);
        _sender.segments_out().pop();
    }

    if (_sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()) {
        if (_linger_after_streams_finish) {
            _time_wait = true;
        }
    }
}

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const {
    return _ticks - _last_ack_time;
}

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!_syn_sent && !seg.header().syn) return;
    if (seg.header().rst) {
        // reset connection
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _linger_after_streams_finish = false;
    }

    _last_ack_time = _ticks;
    _receiver.segment_received(seg);
    _sender.ack_received(seg.header().ackno, seg.header().win);
    _sender.fill_window();
    _syn_sent = true;

    if (_receiver.stream_out().input_ended() && !_sender.stream_in().eof()) {
        // passive close
        _linger_after_streams_finish = false;
    }

    if (!_receiver.ackno().has_value()) {
        return; // no need for ack
    }
    if (_sender.segments_out().empty()) {
        // generate an empty segment to ack
        if (_receiver.stream_out().input_ended() && !seg.header().fin) {
            // no need to ack, server closed and seg not fin
        } else if (seg.length_in_sequence_space() == 0) {
            // no need to ack the empty-ack
        } else {
            _sender.send_empty_segment();
        }
    }
    // send with ack
    send_all_segments();
}

bool TCPConnection::active() const {
    if (_sender.stream_in().error() && _receiver.stream_out().error()) return false;
    return !(_sender.stream_in().eof() && _sender.bytes_in_flight() == 0 && _receiver.stream_out().input_ended()) || _time_wait;
}

size_t TCPConnection::write(const string &data) {
    size_t wrote = _sender.stream_in().write(data);
    _sender.fill_window();
    send_all_segments();
    return wrote;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _ticks += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);

    if (_time_wait && _ticks >= _last_ack_time + _cfg.rt_timeout * 10) {
        // closed
        _time_wait = false;
        _closed = true;
    }

    if (_sender.consecutive_retransmissions() > _cfg.MAX_RETX_ATTEMPTS) {
        // RST
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        _linger_after_streams_finish = false;
        while (!_sender.segments_out().empty()) {
            // pop all segments
            _sender.segments_out().pop();
        }
        _sender.send_empty_segment();
        TCPSegment& seg = _sender.segments_out().front();
        seg.header().rst = true;
    }
    send_all_segments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_all_segments();
}

void TCPConnection::connect() {
    // send SYN
    if (!_syn_sent) {
        _sender.fill_window();
        _syn_sent = true;
        _segments_out.push(_sender.segments_out().front());
        _sender.segments_out().pop();
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            _linger_after_streams_finish = false;
            while (!_sender.segments_out().empty()) {
                // pop all segments
                _sender.segments_out().pop();
            }
            _sender.send_empty_segment();
            TCPSegment& seg = _sender.segments_out().front();
            seg.header().rst = true;
            send_all_segments();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
