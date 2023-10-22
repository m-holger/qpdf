#ifndef QPDF_NAME_HH
#define QPDF_NAME_HH

#include <qpdf/JSON.hh>

class QPDFObject;

class QPDF_Name
{
    friend class QPDFObject;

  public:
    QPDF_Name() = default;
    QPDF_Name(QPDF_Name&&) = default;
    QPDF_Name& operator=(QPDF_Name&&) = default;
    ~QPDF_Name() = default;
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string
    unparse()
    {
        return normalizeName(this->name);
    }
    JSON
    getJSON(int json_version)
    {
        if (json_version == 1) {
            return JSON::makeString(normalizeName(this->name));
        } else {
            return JSON::makeString(this->name);
        }
    }
    // Put # into strings with characters unsuitable for name token
    static std::string normalizeName(std::string const& name);
    std::string
    getStringValue() const
    {
        return name;
    }

  private:
    QPDF_Name(std::string const& name) :
        name(name)
    {
    }
    std::string name;
};

#endif // QPDF_NAME_HH
