#include "tcp_receiver.hh"

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    if (stream_out().input_ended() || (!_isn.has_value() && !seg.header().syn)) {
        return;
    }

    auto seqno = seg.header().seqno;
    if (seg.header().syn) {
        _isn = seqno;
        seqno = seqno + 1;
    }

    _reassembler.push_substring(seg.payload().copy(), unwrap(seqno, _isn.value(), stream_out().bytes_written() + 1) - 1, seg.header().fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (!_isn.has_value()) {
        return {};
    }

    auto n = stream_out().bytes_written() + stream_out().input_ended() + 1;
    return wrap(n, _isn.value());
}

size_t TCPReceiver::window_size() const {
    return stream_out().remaining_capacity();
}
