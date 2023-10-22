#include <qpdf/QPDF_Integer.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>
#include <qpdf/QUtil.hh>

std::shared_ptr<QPDFObject>
QPDF_Integer::copy(bool shallow)
{
    return QPDFObject::create<QPDF_Integer>(val);
}
