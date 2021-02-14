#include "stream_reassembler.hh"

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity) : _output(capacity), _capacity(capacity), _set() {}

void StreamReassembler::push_set(const std::string s, const size_t index) {
    size_t remain = _capacity - _unassembled - _output.buffer_size();
    if (s.length() > remain) {
        _unassembled += remain;
        _set.insert(Segment(s.substr(0, remain), index));
    }
    else {
        _unassembled += s.length();
        _set.insert(Segment(s, index));
    }
}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string& data, const size_t index, const bool eof) {
    if (eof) {
        _eof = true;
    }
    if (data.length() == 0) {
        if (_eof && _unassembled == 0) {
            _output.end_input();
        }
        return;
    }
    bool finished = false;
    string data_ = data;
    if (index <= _expect) {
        if (index + data.length() > _expect) {
            size_t offset = _expect - index;
            size_t need = data.length();
            size_t wrote = _output.write(data.substr(offset));
            _expect += wrote;
            if ((offset + wrote) != need) {
                // 未写完，剩余的插入
                data_ = data.substr(offset + wrote);
            }
            else {
                finished = true;
            }
        }
    }
    if (!finished) {
        // insert into set, combine
        Segment seg(data_, index);
        auto it = _set.lower_bound(seg);
        // it not less than seg: it->start >= seg.start
        if (it != _set.end()) {
            if (it->start > seg.start) {
                // it是后面一个，先判断是否和后面一个重叠
                string s = seg.data;
                size_t start = index;
                if (it->start <= seg.end) {
                    // 重叠了，合并掉
                    while (it != _set.end() && it->end <= seg.end) {
                        // 完全覆盖，直接删掉
                        _unassembled -= it->data.length();
                        it = _set.erase(it);
                    }
                    if (it != _set.end() && it->start <= seg.end) {
                        s = seg.data + it->data.substr(seg.end - it->start);
                        _unassembled -= it->data.length();
                        it = _set.erase(it);
                    }
                }
                else {
                    // 后面没有重叠
                    s = seg.data;
                }
                // 开始判断前面一个
                if (it != _set.begin()) {
                    it--;
                    // 判断前面一个 it->start < seg.start
                    if (it->end < index) {
                        // 合并不了
                        push_set(s, start);
                    }
                    else {
                        // it->end >= s
                        if (it->end < index + s.length()) {
                            s = it->data.substr(0, index - it->start) + s;
                            _unassembled -= it->data.length();
                            push_set(s, it->start);
                            _set.erase(it);
                        }
                    }
                }
                else {
                    // 前面没有了，直接插入
                    push_set(s, index);
                }
            }
            else {
                // it->start == seg.start
                if (it->end < seg.end) {
                    // seg覆盖it，删除it后检查后一个与seg的关系
                    while (it != _set.end() && it->end < seg.end) {
                        // 完全覆盖，直接删掉
                        _unassembled -= it->data.length();
                        it = _set.erase(it);
                    }
                    if (it != _set.end() && it->start <= seg.end) {
                        // 合并seg和后一个
                        string s = seg.data + it->data.substr(seg.end - it->start);
                        _unassembled -= it->data.length();
                        _set.erase(it);
                        push_set(s, index);
                    }
                    else {
                        push_set(data_, index);
                    }
                }
                else {
                    // it覆盖seg，直接丢弃seg
                }
            }
        }
        else {
            // 没找到it->start >= seg.start的
            // 判断前面一个
            std::string s = seg.data;
            if (it != _set.begin()) {
                it--;
                // 判断前面一个 it->start < seg.start
                if (it->end < index) {
                    // 合并不了
                    push_set(s, index);
                }
                else {
                    // it->end >= s
                    if (it->end < index + s.length()) {
                        s = it->data.substr(0, index - it->start) + s;
                        _unassembled -= it->data.length();
                        push_set(s, it->start);
                        _set.erase(it);
                    }
                }
            }
            else {
                // 前面没有了，直接插入
                push_set(s, index);
            }
        }
    }

    // 判断第一个是否可写
    auto it = _set.begin();
    while (it != _set.end() && it->start <= _expect) {
        if (it->end <= _expect) {
            // 整个都已经输出了
            _unassembled -= it->data.length();
            it = _set.erase(it);
        }
        else {
            // 可以写
            size_t offset = _expect - it->start;
            size_t wrote = _output.write(it->data.substr(offset));
            size_t need = it->data.length();
            _expect += wrote;
            _unassembled -= wrote;
            if ((offset + wrote) != need) {
                string s = it->data.substr(offset + wrote);
                push_set(s, _expect);
            }
            _set.erase(it);
            break;
        }
    }

    if (_eof && _unassembled == 0) {
        _output.end_input();
    }
}

size_t StreamReassembler::unassembled_bytes() const { return _unassembled; }

bool StreamReassembler::empty() const { return _unassembled == 0; }
