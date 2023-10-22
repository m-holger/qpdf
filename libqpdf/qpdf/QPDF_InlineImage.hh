#ifndef QPDF_INLINEIMAGE_HH
#define QPDF_INLINEIMAGE_HH

#include <qpdf/JSON.hh>

class QPDFObject;

class QPDF_InlineImage
{
    friend class QPDFObject;

  public:
    QPDF_InlineImage() = default;
    QPDF_InlineImage(QPDF_InlineImage&&) = default;
    QPDF_InlineImage& operator=(QPDF_InlineImage&&) = default;
    ~QPDF_InlineImage() = default;
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
    QPDF_InlineImage(std::string const& val) :
        val(val)
    {
    }
    std::string val;
};

#endif // QPDF_INLINEIMAGE_HH
