#ifndef QPDF_REAL_HH
#define QPDF_REAL_HH

#include <qpdf/JSON.hh>
#include <qpdf/QUtil.hh>

class QPDFObject;

class QPDF_Real
{
    friend class QPDFObject;

  public:
    QPDF_Real() = default;
    QPDF_Real(QPDF_Real&&) = default;
    QPDF_Real& operator=(QPDF_Real&&) = default;
    ~QPDF_Real() = default;
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string
    unparse()
    {
        return this->val;
    }
    JSON getJSON(int json_version);
    std::string
    getStringValue() const
    {
        return val;
    }

  private:
    QPDF_Real(std::string const& val) :
        val(val)
    {
    }

    QPDF_Real(double value, int decimal_places, bool trim_trailing_zeroes) :
        val(QUtil::double_to_string(value, decimal_places, trim_trailing_zeroes))
    {
    }
    // Store reals as strings to avoid roundoff errors.
    std::string val;
};

#endif // QPDF_REAL_HH
