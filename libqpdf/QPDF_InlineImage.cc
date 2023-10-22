#include <qpdf/QPDF_InlineImage.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>


std::shared_ptr<QPDFObject>
QPDF_InlineImage::copy(bool)
{
    return QPDFObject::create<QPDF_InlineImage>(val);
}
