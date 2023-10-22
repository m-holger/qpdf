#ifndef QPDF_OPERATOR_HH
#define QPDF_OPERATOR_HH

#include <qpdf/JSON.hh>

class QPDFObject;

class QPDF_Operator
{
    friend class QPDFObject;

  public:
    QPDF_Operator() = default;
    QPDF_Operator(QPDF_Operator&&) = default;
    QPDF_Operator& operator=(QPDF_Operator&&) = default;
    ~QPDF_Operator() = default;
//    static std::shared_ptr<QPDFObject> create(std::string const& val);
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string
    unparse()
    {
        return val;
    }
    JSON
    getJSON(int json_version)
    {
        return JSON::makeNull();
    }
    std::string
    getStringValue() const
    {
        return val;
    }

  private:
    QPDF_Operator(std::string const& val) :
        val(val)
    {
    }
    std::string val;
};

#endif // QPDF_OPERATOR_HH
