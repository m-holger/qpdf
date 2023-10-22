#include <qpdf/QPDF_IndirectRef.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>

std::shared_ptr<QPDFObject>
QPDF_IndirectRef::create(std::shared_ptr<QPDFObject>& obj)
{
    return QPDFObject::create<QPDF_IndirectRef>(obj);
}

std::shared_ptr<QPDFObject>
QPDF_IndirectRef::copy(bool shallow)
{
    return object.lock();
}
