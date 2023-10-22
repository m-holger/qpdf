#include <qpdf/QPDF_Reserved.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>



std::shared_ptr<QPDFObject>
QPDF_Reserved::copy(bool shallow)
{
    return QPDFObject::create<QPDF_Reserved>();
}
