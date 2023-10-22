#ifndef QPDF_RESERVED_HH
#define QPDF_RESERVED_HH

#include <qpdf/JSON.hh>

#include <stdexcept>

class QPDFObject;

class QPDF_Reserved
{
    friend class QPDFObject;

  public:
    QPDF_Reserved(QPDF_Reserved&&) = default;
    QPDF_Reserved& operator=(QPDF_Reserved&&) = default;
    ~QPDF_Reserved() = default;
//    static std::shared_ptr<QPDFObject> create();
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string
    unparse()
    {
        throw std::logic_error("QPDFObjectHandle: attempting to unparse a reserved object");
        return "";
    }
    JSON
    getJSON(int json_version)
    {
        throw std::logic_error("QPDFObjectHandle: attempting to get JSON from a reserved object");
        return JSON::makeNull();
    }

  private:
    QPDF_Reserved() = default;
};

#endif // QPDF_RESERVED_HH
