#ifndef QPDF_BOOL_HH
#define QPDF_BOOL_HH

#include <qpdf/JSON.hh>

class QPDFObject;

class QPDF_Bool
{
    friend class QPDFObject;

  public:
    QPDF_Bool(QPDF_Bool&&) = default;
    QPDF_Bool& operator=(QPDF_Bool&&) = default;
    ~QPDF_Bool() = default;

    std::shared_ptr<QPDFObject>
    copy(bool shallow = false);

    std::string
    unparse()
    {
        return (val ? "true" : "false");
    }

    JSON
    getJSON(int json_version)
    {
        return JSON::makeBool(this->val);
    }
    bool
    getVal() const
    {
        return this->val;
    }

  private:
    QPDF_Bool(bool val) :
        val(val)
    {
    }
    bool val;
};

#endif // QPDF_BOOL_HH
