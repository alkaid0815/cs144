#include "byte_stream.hh"

#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity), _buffer(make_unique<char[]>(capacity)) {}

size_t ByteStream::write(const string &data) {
    auto len = ::min(data.size(), remaining_capacity());
    auto end = ::min(_capacity, _current + len);
    std::copy(data.begin(), data.begin() + end - _current, &_buffer[_current]);
    if (_current + len > _capacity) {
        std::copy(data.begin() + end - _current, data.begin() + len, _buffer.get());
    }
    _current = (_current + len) % _capacity;
    _size += len;
    _written_bytes += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    auto length = ::min(len, buffer_size());
    if (_current >= buffer_size()) {
        return {&_buffer[_current - buffer_size()], &_buffer[_current - buffer_size() + length]};
    }

    std::string res{&_buffer[_capacity + _current - buffer_size()],
                    &_buffer[::min(_capacity, _capacity + _current - buffer_size() + length)]};
    length -= res.size();
    if (length > 0) {
        res.append(_buffer.get(), _buffer.get() + length);
    }

    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) { _size -= ::min(len, buffer_size()); }

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string str = peek_output(len);
    pop_output(len);
    return str;
}

void ByteStream::end_input() { _ended = true; }

bool ByteStream::input_ended() const { return _ended; }

size_t ByteStream::buffer_size() const { return _size; }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _written_bytes; }

size_t ByteStream::bytes_read() const { return _written_bytes - buffer_size(); }

size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
