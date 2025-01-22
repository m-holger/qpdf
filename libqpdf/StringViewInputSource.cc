#include <qpdf/StringViewInputSource.hh>


#include <qpdf/QIntC.hh>
#include <algorithm>
#include <cstring>
#include <sstream>

qpdf_offset_t
StringViewInputSource::findAndSkipNextEOL()
{
    if (cur_offset < 0) {
        throw std::logic_error("INTERNAL ERROR: StringViewInputSource offset < 0");
    }
    qpdf_offset_t end_pos = max_offset;
    if (cur_offset >= end_pos) {
        last_offset = end_pos;
        cur_offset = end_pos;
        return end_pos;
    }
    
    qpdf_offset_t result = 0;
    char const* buffer = sv.data();
    char const* end = buffer + end_pos;
    char const* p = buffer + cur_offset;

    while ((p < end) && !((*p == '\r') || (*p == '\n'))) {
        ++p;
    }
    if (p < end) {
        result = p - buffer;
        cur_offset = result + 1;
        ++p;
        while ((cur_offset < end_pos) && ((*p == '\r') || (*p == '\n'))) {
            ++p;
            ++cur_offset;
        }
    } else {
        cur_offset = end_pos;
        result = end_pos;
    }
    return result;
}

size_t
StringViewInputSource::read(char* buffer, size_t length)
{
    if (cur_offset < 0) {
        throw std::logic_error("INTERNAL ERROR: BufferInputSource offset < 0");
    }
    qpdf_offset_t end_pos = max_offset;
    if (cur_offset >= end_pos) {
        last_offset = end_pos;
        return 0;
    }
    
    last_offset = cur_offset;
    size_t len = std::min(QIntC::to_size(end_pos - cur_offset), length);
    memcpy(buffer, sv.data() + cur_offset, len);
    cur_offset += QIntC::to_offset(len);
    return len;
}
