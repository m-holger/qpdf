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

OH::OH(QPDFObjectHandle oh, QPDFObjectHandle parent, std::string key) :
    QPDFObjectHandle(oh)
{
    obj->parent = parent.obj;
    obj->key = key;
    obj->index = std::numeric_limits<size_t>::max();
}

OH::OH(QPDFObjectHandle oh, QPDFObjectHandle parent, size_t index) :
    QPDFObjectHandle(oh)
{
    obj->parent = parent.obj;
    obj->key = "";
    obj->index = index;
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
