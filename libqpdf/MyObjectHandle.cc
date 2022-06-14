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
    obj->parent_type = parent.getTypeCode();
    obj->parent = std::make_shared<QPDFObjectHandle>(parent);
    obj->key = key;
    obj->index = std::numeric_limits<size_t>::max();
}

OH::OH(QPDFObjectHandle oh, QPDFObjectHandle parent, size_t index) :
    QPDFObjectHandle(oh)
{
    obj->parent_type = parent.getTypeCode();
    obj->parent = std::make_shared<QPDFObjectHandle>(parent);
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
    auto value = other.isIndirect() ? other : other.shallowCopy();
    auto parent_type = obj->parent_type;
    auto parent = obj->parent;
    auto key = obj->key;
    auto index = obj->index;

    objid = value.objid;
    generation = value.generation;
    obj = value.obj;
    reserved = value.reserved;

    obj->parent_type = parent_type;
    obj->parent = parent;
    obj->key = key;
    obj->index = index;

    if (parent_type == QPDFObject::ot_dictionary) {
        parent->replaceKey(key, value);
    } else if (parent_type == QPDFObject::ot_array) {
        parent->setArrayItem(QIntC::to_int(index), value);
    }

    return *this;
}

OH&
OH::operator=(OH&& other)
{
    if (this == &other) {
        return *this;
    }
    auto value = other.isIndirect() ? other : other.shallowCopy();
    auto parent_type = obj->parent_type;
    auto parent = obj->parent;
    auto key = obj->key;
    auto index = obj->index;

    objid = value.objid;
    generation = value.generation;
    obj = value.obj;
    reserved = value.reserved;

    obj->parent_type = parent_type;
    obj->parent = parent;
    obj->key = key;
    obj->index = index;

    if (parent_type == QPDFObject::ot_dictionary) {
        parent->replaceKey(key, value);
    } else if (parent_type == QPDFObject::ot_array) {
        parent->setArrayItem(QIntC::to_int(index), value);
    }

    return *this;
}
