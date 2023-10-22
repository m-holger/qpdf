#include <qpdf/QPDF_Unresolved.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>
#include <stdexcept>

std::shared_ptr<QPDFObject>
QPDF_Unresolved::create(QPDF* qpdf, QPDFObjGen const& og)
{
    auto obj = QPDFObject::create<QPDF_Unresolved>();
    obj->setObjGen(qpdf, og);
    return obj;
}
