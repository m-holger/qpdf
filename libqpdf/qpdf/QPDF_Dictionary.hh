#ifndef QPDF_DICTIONARY_HH
#define QPDF_DICTIONARY_HH

#include <map>
#include <set>

#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjectHandle.hh>

class Value;

class QPDF_Dictionary
{
    friend class QPDFObject;

  public:
    QPDF_Dictionary() = default;
    QPDF_Dictionary(QPDF_Dictionary&&) = default;
    QPDF_Dictionary& operator=(QPDF_Dictionary&&) = default;
    ~QPDF_Dictionary() = default;
    static std::shared_ptr<QPDFObject> create(std::map<std::string, QPDFObjectHandle> const& items);
    static std::shared_ptr<QPDFObject> create(std::map<std::string, QPDFObjectHandle>&& items);
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string unparse();
    JSON getJSON(int json_version);
    void disconnect();

    // hasKey() and getKeys() treat keys with null values as if they aren't there.  getKey() returns
    // null for the value of a non-existent key.  This is as per the PDF spec.
    bool hasKey(std::string const&);
    QPDFObjectHandle getKey(std::shared_ptr<QPDFObject>& self, std::string const&);
    std::set<std::string> getKeys();
    std::map<std::string, QPDFObjectHandle> const& getAsMap() const;

    // If value is a direct null, remove key; otherwise, replace the value of key, adding it if it
    // does not exist.
    void replaceKey(std::string const& key, QPDFObjectHandle value);
    // Remove key, doing nothing if key does not exist
    void removeKey(std::string const& key);

  private:
    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle> const& items);
    QPDF_Dictionary(std::map<std::string, QPDFObjectHandle>&& items);
    std::map<std::string, QPDFObjectHandle> items;
};

#endif // QPDF_DICTIONARY_HH
