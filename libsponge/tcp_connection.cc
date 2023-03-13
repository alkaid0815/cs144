#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return outbound_stream().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _timer; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _timer = 0;
    if (seg.header().rst) {
        _receiver.stream_out().set_error();
        outbound_stream().set_error();
        return;
    }

    _receiver.segment_received(seg);
    if (inbound_stream().input_ended() &&
        !(outbound_stream().eof() && _sender.next_seqno_absolute() >= outbound_stream().bytes_read() + 2)) {
        _linger_after_streams_finish = false;
    }

    if (seg.header().ack) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    if (seg.length_in_sequence_space() > 0) {
        _sender.fill_window();
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
        fill_window();
    }

    if (_receiver.ackno().has_value() && seg.length_in_sequence_space() == 0 &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
        fill_window();
    }
}

bool TCPConnection::active() const {
    if (_receiver.stream_out().error() && outbound_stream().error()) {
        return false;
    }

    // TODO time_wait等待2MSL或者Last_ack
    bool last_ack = !_linger_after_streams_finish && outbound_stream().eof() &&
                    _sender.acknoed() == outbound_stream().bytes_read() + 2;
    bool time_wait = outbound_stream().eof() &&
                    _sender.acknoed() == outbound_stream().bytes_read() + 2 &&
                    _receiver.stream_out().input_ended();
    return !(last_ack || (time_wait && time_since_last_segment_received() >= 10 * _cfg.rt_timeout));
}

size_t TCPConnection::write(const string &data) {
    size_t len = outbound_stream().write(data);
    _sender.fill_window();
    fill_window();
    return len;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        send_rst();
    } else {
        fill_window();
    }

    _timer += ms_since_last_tick;
}

void TCPConnection::send_rst() {
    _receiver.stream_out().set_error();
    outbound_stream().set_error();
    if (_sender.segments_out().empty()) {
        _sender.send_empty_segment();
    }
    fill_window(true);
}

void TCPConnection::end_input_stream() {
    outbound_stream().end_input();
    _sender.fill_window();
    fill_window();
}

void TCPConnection::connect() {
    _sender.fill_window();
    fill_window();
}

void TCPConnection::fill_window(bool is_rst) {
    while (!_sender.segments_out().empty()) {
        auto seg = std::move(_sender.segments_out().front());
        _sender.segments_out().pop();
        if (_receiver.ackno()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = min(static_cast<uint16_t>(_receiver.window_size()), numeric_limits<uint16_t>::max());
        }
        if (is_rst) {
            seg.header().rst = true;
        }
        _segments_out.push(std::move(seg));
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
