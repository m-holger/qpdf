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

// Generalized Pipeline interface.  By convention, subclasses of Pipeline are called Pl_Something.
//
// When an instance of Pipeline is created with a pointer to a next pipeline, that pipeline writes
// its data to the next one when it finishes with it.  In order to make possible a usage style in
// which a pipeline may be passed to a function which may stick other pipelines in front of it, the
// allocator of a pipeline is responsible for its destruction.  In other words, one pipeline object
// does not attempt to manage the memory of its successor.
//
// The client is required to call finish() before destroying a Pipeline in order to avoid loss of
// data.  A Pipeline class should not throw an exception in the destructor if this hasn't been done
// though since doing so causes too much trouble when deleting pipelines during error conditions.
//
// Some pipelines are reusable (i.e., you can call write() after calling finish() and can call
// finish() multiple times) while others are not.  It is up to the caller to use a pipeline
// according to its own restrictions.

#ifndef PIPELINE_HH
#define PIPELINE_HH

#include <qpdf/DLL.h>

#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

// Remember to use QPDF_DLL_CLASS on anything derived from Pipeline so it will work with
// dynamic_cast across the shared object boundary.
class QPDF_DLL_CLASS Pipeline
{
  public:
    Pipeline() = delete;
    Pipeline(Pipeline const&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline const&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;
    QPDF_DLL
    virtual ~Pipeline() = default;

    // Subclasses should implement write and finish to do their jobs and then, if they are not
    // end-of-line pipelines, call getNext()->write or getNext()->finish.
    QPDF_DLL
    virtual void write(unsigned char const* data, size_t len) = 0;
    QPDF_DLL
    inline virtual void finish();
    QPDF_DLL
    inline std::string const& getIdentifier() const noexcept;

    // These are convenience methods for making it easier to write certain other types of data to
    // pipelines without having to cast. The methods that take char const* expect null-terminated C
    // strings and do not write the null terminators.
    QPDF_DLL
    inline void writeCStr(char const* cstr);
    QPDF_DLL
    inline void writeString(std::string const&);
    // This allows *p << "x" << "y" but is not intended to be a general purpose << compatible with
    // ostream and does not have local awareness or the ability to be "imbued" with properties.
    QPDF_DLL
    inline Pipeline& operator<<(char const* cstr);
    QPDF_DLL
    inline Pipeline& operator<<(std::string const&);
    QPDF_DLL
    inline Pipeline& operator<<(short);
    QPDF_DLL
    inline Pipeline& operator<<(int);
    QPDF_DLL
    inline Pipeline& operator<<(long);
    QPDF_DLL
    inline Pipeline& operator<<(long long);
    QPDF_DLL
    inline Pipeline& operator<<(unsigned short);
    QPDF_DLL
    inline Pipeline& operator<<(unsigned int);
    QPDF_DLL
    inline Pipeline& operator<<(unsigned long);
    QPDF_DLL
    inline Pipeline& operator<<(unsigned long long);

    // Overloaded write to reduce casting
    QPDF_DLL
    inline void write(char const* data, size_t len);

  protected:
    QPDF_DLL
    inline Pipeline(std::string_view identifier, Pipeline* next);
    // For backward compatibility only. Access next directly instead
    QPDF_DLL
    inline Pipeline* getNext(bool allow_null = false);
    std::string identifier;
    Pipeline* const next;
};

inline Pipeline::Pipeline(std::string_view identifier, Pipeline* next) :
    identifier(identifier),
    next(next)
{
}

inline Pipeline*
Pipeline::getNext(bool allow_null)
{
    if (!next && !allow_null) {
        throw std::logic_error(
            identifier + ": Pipeline::getNext() called on pipeline with no next");
    }
    return next;
}

inline void
Pipeline::finish()
{
    if (next) {
        next->finish();
    }
}

inline std::string const&
Pipeline::getIdentifier() const noexcept
{
    return identifier;
}

inline void
Pipeline::writeCStr(char const* cstr)
{
    write(cstr, strlen(cstr));
}

inline void
Pipeline::writeString(std::string const& str)
{
    write(str.c_str(), str.length());
}

inline Pipeline&
Pipeline::operator<<(char const* cstr)
{
    writeCStr(cstr);
    return *this;
}

inline Pipeline&
Pipeline::operator<<(std::string const& str)
{
    writeString(str);
    return *this;
}

inline Pipeline&
Pipeline::operator<<(short i)
{
    writeString(std::to_string(i));
    return *this;
}

inline Pipeline&
Pipeline::operator<<(int i)
{
    writeString(std::to_string(i));
    return *this;
}

inline Pipeline&
Pipeline::operator<<(long i)
{
    writeString(std::to_string(i));
    return *this;
}

inline Pipeline&
Pipeline::operator<<(long long i)
{
    writeString(std::to_string(i));
    return *this;
}

inline Pipeline&
Pipeline::operator<<(unsigned short i)
{
    writeString(std::to_string(i));
    return *this;
}

inline Pipeline&
Pipeline::operator<<(unsigned int i)
{
    writeString(std::to_string(i));
    return *this;
}

inline Pipeline&
Pipeline::operator<<(unsigned long i)
{
    writeString(std::to_string(i));
    return *this;
}

inline Pipeline&
Pipeline::operator<<(unsigned long long i)
{
    writeString(std::to_string(i));
    return *this;
}

inline void
Pipeline::write(char const* data, size_t len)
{
    write(reinterpret_cast<unsigned char const*>(data), len);
}

#endif // PIPELINE_HH
