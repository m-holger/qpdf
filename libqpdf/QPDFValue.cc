#include <qpdf/QPDFValue.hh>

#include <qpdf/QPDFObject.hh>
#include <qpdf/QPDFObjectHandle.hh>

std::shared_ptr<QPDFObject>
QPDFValue::do_create(QPDFValue* object)
{
    std::shared_ptr<QPDFObject> obj(new QPDFObject());
    obj->self = obj;
    obj->value = std::shared_ptr<QPDFValue>(object);
    return obj;
}

QPDFObjectHandle
QPDFValue::at(size_t index)
{
    throw std::logic_error("should be unreachable");
}

QPDFObjectHandle
QPDFValue::at(std::string const& key)
{
    throw std::logic_error("not a map");
}

size_t
QPDFValue::size() const
{
    return 1;
}
