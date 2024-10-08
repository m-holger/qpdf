#include <qpdf/QPDF_Destroyed.hh>

#include <stdexcept>

QPDF_Destroyed::QPDF_Destroyed() :
    QPDFValue(::ot_destroyed)
{
}

std::shared_ptr<QPDFValue>
QPDF_Destroyed::getInstance()
{
    static std::shared_ptr<QPDFValue> instance(new QPDF_Destroyed());
    return instance;
}

std::shared_ptr<QPDFObject>
QPDF_Destroyed::copy(bool shallow)
{
    throw std::logic_error("attempted to shallow copy QPDFObjectHandle from destroyed QPDF");
    return nullptr;
}

std::string
QPDF_Destroyed::unparse()
{
    throw std::logic_error("attempted to unparse a QPDFObjectHandle from a destroyed QPDF");
    return "";
}

void
QPDF_Destroyed::writeJSON(int json_version, JSON::Writer& p)
{
    throw std::logic_error("attempted to get JSON from a QPDFObjectHandle from a destroyed QPDF");
}