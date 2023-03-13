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
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return next_seqno_absolute() - _ackno; }

void TCPSender::fill_window() {
    auto window_size = _window_size == 0 ? 1 : _window_size;
    while (true) {
        if (next_seqno_absolute() >= _ackno + window_size) {
            break;
        }

        if (next_seqno_absolute() != 0 && stream_in().buffer_empty() &&
            (!stream_in().input_ended() || next_seqno_absolute() >= 2 + stream_in().bytes_read())) {
            break;
        }

        auto sequence_size = _ackno + window_size - next_seqno_absolute();
        TCPSegment seg;
        seg.header().seqno = next_seqno();
        if (next_seqno_absolute() == 0) {
            seg.header().syn = true;
            sequence_size -= 1;
        }

        if (sequence_size > 0 && !stream_in().buffer_empty()) {
            auto str = stream_in().read(min(TCPConfig::MAX_PAYLOAD_SIZE, sequence_size));
            sequence_size -= str.size();
            seg.payload() = std::move(str);
        }

        if (sequence_size > 0 && stream_in().eof()) {
            seg.header().fin = true;
        }

        segments_out().push(seg);
        _next_seqno += seg.length_in_sequence_space();
        _outstanding_segs.push(std::move(seg));

        if (!_timer.has_value()) {
            _timer = _initial_retransmission_timeout;
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    auto new_ackno = unwrap(ackno, _isn, _ackno);
    if (new_ackno < _ackno || new_ackno > next_seqno_absolute()) {
        return;
    }

    bool acked = false;
    while (!_outstanding_segs.empty()) {
        const auto &seg = _outstanding_segs.front();
        if (unwrap(seg.header().seqno, _isn, _ackno) + seg.length_in_sequence_space() > new_ackno) {
            break;
        }

        acked = true;
        _outstanding_segs.pop();
    }

    if (acked) {
        _retransmission_count = 0;
        _outstanding_segs.empty() ? _timer.reset() : _timer->reset(_initial_retransmission_timeout);
    }

    _window_size = window_size;
    _ackno = new_ackno;
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer.has_value()) {
        return;
    }

    _timer->tick(ms_since_last_tick);

    if (_timer->expired()) {
        segments_out().push(_outstanding_segs.front());
        auto ms = _timer->retransmission_timeout();
        if (_window_size != 0) {
            ++_retransmission_count;
            ms *= 2;
        }
        _timer->reset(ms);
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _retransmission_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = next_seqno();
    segments_out().push(std::move(seg));
}
