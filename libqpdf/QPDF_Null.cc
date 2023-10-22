#include <qpdf/QPDF_Null.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>

std::shared_ptr<QPDFObject>
QPDF_Null::create(
    std::shared_ptr<QPDFObject> parent, std::string_view const& static_descr, std::string var_descr)
{
    auto obj = QPDFObject::create<QPDF_Null>();
    obj->setChildDescription(parent, static_descr, var_descr);
    return obj;
}
