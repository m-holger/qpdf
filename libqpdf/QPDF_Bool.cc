#include <qpdf/QPDF_Bool.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>

std::shared_ptr<QPDFObject>
QPDF_Bool::copy(bool shallow)
{
    return QPDFObject::create<QPDF_Bool>(val);
}
