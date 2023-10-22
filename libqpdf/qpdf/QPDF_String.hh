#ifndef QPDF_STRING_HH
#define QPDF_STRING_HH

#include <qpdf/JSON.hh>

class QPDFObject;

// QPDF_Strings may included embedded null characters.

class QPDF_String
{
    friend class QPDFWriter;
    friend class QPDFObject;

  public:
    QPDF_String(QPDF_String&&) = default;
    QPDF_String& operator=(QPDF_String&&) = default;
    ~QPDF_String() = default;
//    static std::shared_ptr<QPDFObject> create(std::string const& val);
    static std::shared_ptr<QPDFObject> create_utf16(std::string const& utf8_val);
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string unparse();
    std::string unparse(bool force_binary);
    JSON getJSON(int json_version);
    std::string getUTF8Val() const;
    std::string
    getStringValue() const
    {
        return val;
    }

  private:
    QPDF_String(std::string const& val) :
        val(val)
    {
    }
    bool useHexString() const;
    std::string val;
};

#endif // QPDF_STRING_HH
