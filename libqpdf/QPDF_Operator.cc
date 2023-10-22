#include <qpdf/QPDF_Operator.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>

//std::shared_ptr<QPDFObject>
//QPDF_Operator::create(std::string const& val)
//{
//    // auto obj = std::make_shared<QPDFObject>();
//    // obj->value = std::make_shared<Value>(QPDF_Operator(val));
//    // return obj;
//    return QPDFObject::create<QPDF_Operator>(val);
//    // return do_create(new QPDF_Operator(val));
//}

std::shared_ptr<QPDFObject>
QPDF_Operator::copy(bool shallow)
{
    return QPDFObject::create<QPDF_Operator>(val);
}
