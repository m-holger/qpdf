#ifndef QPDF_ARRAY_HH
#define QPDF_ARRAY_HH

#include <map>
#include <memory>
#include <vector>

class JSON;
class QPDFValue;
class QPDFObject;
class QPDFObjectHandle;

class QPDF_Array
{
    friend class QPDFObject;

  public:
    ~QPDF_Array() = default;
    static std::shared_ptr<QPDFObject> create(std::vector<QPDFObjectHandle> const& items);
    static std::shared_ptr<QPDFObject>
    create(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse);
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string unparse();
    JSON getJSON(int json_version);
    void disconnect();

    int
    size() const noexcept
    {
        return sparse ? m->sp_size : int(elements.size());
    }
    QPDFObjectHandle at(int n) const noexcept;
    bool setAt(int n, QPDFObjectHandle const& oh);
    std::vector<QPDFObjectHandle> getAsVector() const;
    void setFromVector(std::vector<QPDFObjectHandle> const& items);
    bool insert(int at, QPDFObjectHandle const& item);
    void push_back(QPDFObjectHandle const& item);
    bool erase(int at);

    QPDF_Array() = default;
    QPDF_Array(QPDF_Array const& other) :
        m(other.sparse ? std::make_unique<Members>(*other.m) : nullptr),
        sparse(other.sparse),
        elements(other.elements)
    {
    }
    QPDF_Array(QPDF_Array&&) = default;
    QPDF_Array& operator=(QPDF_Array&&) = default;

    QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse);

    void checkOwnership(QPDFObjectHandle const& item) const;

    struct Members
    {
        int sp_size{0};
        std::map<int, std::shared_ptr<QPDFObject>> sp_elements;
    };

    std::unique_ptr<Members> m{nullptr};
    bool sparse{false};
    std::vector<std::shared_ptr<QPDFObject>> elements;
    QPDFValue* value{nullptr};
};

#endif // QPDF_ARRAY_HH
