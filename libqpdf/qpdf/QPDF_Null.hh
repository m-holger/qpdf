#ifndef QPDF_NULL_HH
#define QPDF_NULL_HH

#include <qpdf/JSON.hh>

class QPDFObject;
class Value;

class QPDF_Null
{
    friend class QPDFObject;

  public:
    QPDF_Null(QPDF_Null&&) = default;
    QPDF_Null& operator=(QPDF_Null&&) = default;
    ~QPDF_Null() = default;

    static std::shared_ptr<QPDFObject> create(
        std::shared_ptr<QPDFObject> parent,
        std::string_view const& static_descr,
        std::string var_descr);
    std::string
    unparse()
    {
        return "null";
    }
    JSON
    getJSON(int json_version)
    {
        // If this is updated, QPDF_Array::getJSON must also be updated.
        return JSON::makeNull();
    }

  private:
    QPDF_Null() = default;
};

#endif // QPDF_NULL_HH
