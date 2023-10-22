#include <qpdf/QPDF_Destroyed.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>

QPDFValue
QPDF_Destroyed::getInstance()
{
    static ObjValue destroyed{QPDF_Destroyed()};
    return QPDFValue(QPDF_Destroyed());
}

std::shared_ptr<QPDFObject>
QPDF_Destroyed::create()
{
    return QPDFObject::create<QPDF_Destroyed>();
}
