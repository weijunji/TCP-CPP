#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity) : buffer(capacity), cap(capacity) {}

size_t ByteStream::write(const string &data) {
    size_t wlen;
    if(data.length() > cap - length){
        wlen = cap - length;
    }else{
        wlen = data.length();
    }
    for(size_t i = 0; i < wlen; i++){
        buffer[tail] = data[i];
        tail = (tail + 1) % cap;
    }
    length += wlen;
    total_write += wlen;
    return wlen;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t rlen;
    if(len > length){
        rlen = length;
    }else{
        rlen = len;
    }
    string res(rlen, 0);
    size_t p = head;
    for(size_t i = 0; i < rlen; i++){
        res[i] = buffer[p];
        p = (p + 1) % cap;
    }
    return res;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    if(len > length){
        length = 0;
        head = this->tail;
        total_read += length;
    }else{
        length -= len;
        head = (head + len) % cap;
        total_read += len;
    }
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() {
    end = true;
}

bool ByteStream::input_ended() const {
    return end;
}

size_t ByteStream::buffer_size() const {
    return length;
}

bool ByteStream::buffer_empty() const {
    // cout << len << endl;
    return length == 0;
}

bool ByteStream::eof() const {
    return end && length == 0;
}

size_t ByteStream::bytes_written() const {
    return total_write;
}

size_t ByteStream::bytes_read() const {
    return total_read;
}

size_t ByteStream::remaining_capacity() const {
    return cap - length;
}
