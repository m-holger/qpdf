#ifndef MYOBJECTHANDLE_HH
#define MYOBJECTHANDLE_HH

#include <qpdf/DLL.h>

#include <string>

#include <qpdf/QPDFObjectHandle.hh>

class OH: public QPDFObjectHandle
{
  public:
    class iterator;

    QPDF_DLL
    OH(QPDFObjectHandle&);

    QPDF_DLL
    OH(QPDFObjectHandle&&);

    QPDF_DLL
    OH(OH const&) = default;

    QPDF_DLL
    OH& operator=(OH&);

    QPDF_DLL
    OH& operator=(OH&&);

    QPDF_DLL
    iterator begin();
    QPDF_DLL
    iterator end();

    // OH inherits the at() method from QPDFObjectHandle

    QPDF_DLL
    size_t size() const;
};

class OH::iterator
{
    friend class OH;

  public:
    typedef OH T;
    using iterator_category = std::bidirectional_iterator_tag;
    using value_type = T;
    using difference_type = long;
    using pointer = T*;
    using reference = T&;

    QPDF_DLL
    iterator(size_t index);
    QPDF_DLL
    virtual ~iterator() = default;
    QPDF_DLL
    iterator
    operator++()
    {
        ++index;
        return *this;
    }
    QPDF_DLL
    iterator
    operator++(int)
    {
        ++index;
        return *this;
    }
    QPDF_DLL
    iterator&
    operator--()
    {
        --index;
        return *this;
    }
    QPDF_DLL
    iterator
    operator--(int)
    {
        --index;
        return *this;
    }
    QPDF_DLL
    OH
    operator*()
    {
        return oh.at(index);
    }
    QPDF_DLL
    pointer
    operator->()
    {
        value = oh.at(index);
        return &value;
    }
    QPDF_DLL
    bool
    operator==(iterator const& other) const
    {
        return index == other.index;
    }
    QPDF_DLL
    bool
    operator!=(iterator const& other) const
    {
        return index != other.index;
    }

  private:
    iterator(OH& oh, size_t index);
    OH& oh;
    size_t index;
    OH value;
};

#endif // MYOBJECTHANDLE_HH
