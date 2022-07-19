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

OH::OH(OH const& other) :
    QPDFObjectHandle(other)
{
}

OH::OH(OH&& other) :
    QPDFObjectHandle(other)
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

size_t
OH::size() const
{
    return obj->size();
}
