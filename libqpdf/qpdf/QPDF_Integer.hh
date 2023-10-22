#ifndef QPDF_INTEGER_HH
#define QPDF_INTEGER_HH

#include <qpdf/JSON.hh>

class QPDFObject;

class QPDF_Integer
{
    friend class QPDFObject;

  public:
    QPDF_Integer() = default;
    QPDF_Integer(QPDF_Integer&&) = default;
    QPDF_Integer& operator=(QPDF_Integer&&) = default;
    ~QPDF_Integer() = default;
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string
    unparse()
    {
        return std::to_string(this->val);
    }
    JSON
    getJSON(int json_version)
    {
        return JSON::makeInt(this->val);
    }
    long long
    getVal() const
    {
        return this->val;
    }

  private:
    QPDF_Integer(long long val) :
        val(val)
    {
    }
    long long val;
};

#endif // QPDF_INTEGER_HH
