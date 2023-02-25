#include "stream_reassembler.hh"

#include <vector>
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
    // empty string
    if (eof && data.size() == 0 && index >= _output.bytes_written() && index < _output.bytes_read() + _capacity) {
        _has_end = true;
        _end = index;

        if (_end == _output.bytes_written()) {
            _output.end_input();
        }
        return;
    }

    auto begin = ::max(index, _output.bytes_written());
    auto end = ::min(_output.bytes_read() + _capacity, index + data.size());
    if (begin >= end) {
        return;
    }

    for (auto curr = begin; curr < end; ++curr) {
        _cache[curr] = data[curr - index];
    }

    if (eof && end == index + data.size()) {
        _has_end = true;
        _end = end;
    }

    string buffer{};
    auto curr = _output.bytes_written();
    while (_cache.count(curr) > 0) {
        if (_has_end && curr == _end) {
            break;
        }

        buffer += _cache[curr];
        _cache.erase(curr);
        ++curr;
    }

    if (buffer.empty()) {
        return;
    }

    _output.write(buffer);
    if (_has_end && _end == _output.bytes_written()) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _cache.size(); }

bool StreamReassembler::empty() const { return unassembled_bytes() == 0 && _output.buffer_empty(); }
