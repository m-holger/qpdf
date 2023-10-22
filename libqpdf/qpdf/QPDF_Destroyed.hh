#ifndef QPDF_DESTROYED_HH
#define QPDF_DESTROYED_HH

#include <qpdf/JSON.hh>
#include <stdexcept>

class QPDFObject;
class QPDFValue;

class QPDF_Destroyed
{
    friend class QPDFObject;

  public:
    ~QPDF_Destroyed() = default;
    static std::shared_ptr<QPDFObject> create();
    std::shared_ptr<QPDFObject>
    copy(bool shallow = false)
    {
        throw std::logic_error("attempted to shallow copy QPDFObjectHandle from destroyed QPDF");
        return nullptr;
    }
    std::string
    unparse()
    {
        throw std::logic_error("attempted to unparse a QPDFObjectHandle from a destroyed QPDF");
        return "";
    }
    JSON
    getJSON(int json_version)
    {
        throw std::logic_error("attempted to get JSON from a QPDFObjectHandle "
                               "from a destroyed QPDF");
        return JSON::makeNull();
    }
    static QPDFValue getInstance();

    QPDF_Destroyed() = default;
    QPDF_Destroyed& operator=(QPDF_Destroyed&&) = default;
    QPDF_Destroyed(QPDF_Destroyed&& other) = default;
};

#endif // QPDF_DESTROYED_HH
