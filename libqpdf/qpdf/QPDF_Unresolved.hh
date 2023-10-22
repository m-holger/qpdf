#ifndef QPDF_UNRESOLVED_HH
#define QPDF_UNRESOLVED_HH

#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjGen.hh>

#include <stdexcept>

class QPDF;
class QPDFObject;

class QPDF_Unresolved
{
    friend class QPDFObject;

  public:
    QPDF_Unresolved(QPDF_Unresolved&&) = default;
    QPDF_Unresolved& operator=(QPDF_Unresolved&&) = default;
    ~QPDF_Unresolved() = default;
    static std::shared_ptr<QPDFObject> create(QPDF* qpdf, QPDFObjGen const& og);
    std::shared_ptr<QPDFObject>
    copy(bool shallow = false)
    {
        throw std::logic_error("attempted to shallow copy an unresolved QPDFObjectHandle");
        return nullptr;
    }
    std::string
    unparse()
    {
        throw std::logic_error("attempted to unparse an unresolved QPDFObjectHandle");
        return "";
    }
    JSON
    getJSON(int json_version)
    {
        throw std::logic_error("attempted to get JSON from an unresolved QPDFObjectHandle");
        return JSON::makeNull();
    }

  private:
    QPDF_Unresolved() = default;
};

#endif // QPDF_UNRESOLVED_HH
