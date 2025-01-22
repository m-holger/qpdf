// Copyright (c) 2005-2024 Jay Berkenbilt
//
// This file is part of qpdf.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License. You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied. See the License for the specific language governing permissions and limitations under
// the License.
//
// Versions of qpdf prior to version 7 were released under the terms of version 2.0 of the Artistic
// License. At your option, you may continue to consider qpdf to be licensed under those terms.
// Please see the manual for additional information.

#ifndef QPDF_STRINGVIEWINPUTSOURCE_HH
#define QPDF_STRINGVIEWINPUTSOURCE_HH

#include <qpdf/InputSource.hh>
#include <qpdf/QIntC.hh>

#include <string_view>

class StringViewInputSource final: public InputSource
{
  public:
    // If own_memory is true, BufferInputSource will delete the buffer when finished with it.
    // Otherwise, the caller owns the memory.

    StringViewInputSource(std::string const& description, std::string_view sv) :
        description(description),
        sv(sv),
        max_offset(QIntC::to_offset(sv.size()))
    {}

    ~StringViewInputSource() final = default;

    qpdf_offset_t findAndSkipNextEOL() final;

    std::string const& getName() const final
    {
            return description;
    }

    qpdf_offset_t tell() final {
        return cur_offset;
    }

    void seek(qpdf_offset_t offset, int whence) final

{
    switch (whence) {
    case SEEK_SET:
        cur_offset = offset;
        break;

    case SEEK_END:
        QIntC::range_check(max_offset, offset);
        cur_offset = max_offset + offset;
        break;

    case SEEK_CUR:
        QIntC::range_check(cur_offset, offset);
        cur_offset += offset;
        break;

    default:
        throw std::logic_error("INTERNAL ERROR: invalid argument to StringViewInputSource::seek");
        break;
    }

    if (cur_offset < 0) {
        throw std::runtime_error(description + ": seek before beginning of buffer");
    }
}

    void rewind() final
    {
        cur_offset = 0;
    }

    size_t read(char* buffer, size_t length) final;

    void unreadCh(char ch) final
    {
        if (cur_offset > 0) {
            --cur_offset;
        }
    }

  private:
    std::string description;
    std::string_view sv;
    qpdf_offset_t cur_offset{0};
    qpdf_offset_t max_offset{0};
};

#endif // QPDF_STRINGVIEWINPUTSOURCE_HH
