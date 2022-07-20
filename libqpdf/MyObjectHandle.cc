#include <qpdf/MyObjectHandle.hh>

#include <qpdf/QIntC.hh>

OH::OH(QPDFObjectHandle& oh) :
    QPDFObjectHandle(oh)
{
}

OH::OH(QPDFObjectHandle&& oh) :
    QPDFObjectHandle(oh)
{
}

OH&
OH::operator=(OH& other)
{
    if (this == &other) {
        return *this;
    }
    auto val = other.isIndirect() ? other : other.shallowCopy();

    objid = val.objid;
    generation = val.generation;
    obj->assign2(val);
    obj = val.obj;

    return *this;
}

OH&
OH::operator=(OH&& other)
{
    if (this == &other) {
        return *this;
    }
    auto val = other.isIndirect() ? other : other.shallowCopy();

    objid = val.objid;
    generation = val.generation;
    obj->assign2(val);
    obj = val.obj;

    return *this;
}

OH::iterator
OH::begin()
{
    return iterator(*this, 0);
}

OH::iterator
OH::end()
{
    return iterator(*this, size());
}

size_t
OH::size() const
{
    return obj->size();
}

OH::iterator::iterator(OH& oh, size_t index) :
    oh(oh),
    index(index),
    value(oh)
{
}
