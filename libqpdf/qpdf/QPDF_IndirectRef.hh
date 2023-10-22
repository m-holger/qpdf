#ifndef QPDF_INDIRECTREF_HH
#define QPDF_INDIRECTREF_HH

#include <qpdf/QPDF_Destroyed.hh>

#include <memory>
#include <stdexcept>

class QPDFObject;

class QPDF_IndirectRef
{
    friend class QPDFObject;
    friend class QPDF;

  public:
    QPDF_IndirectRef(std::shared_ptr<QPDFObject>& obj) :
        object(obj)
    {
    }
    QPDF_IndirectRef(QPDF_IndirectRef&&) = default;
    QPDF_IndirectRef& operator=(QPDF_IndirectRef&&) = default;
    ~QPDF_IndirectRef() = default;
    static std::shared_ptr<QPDFObject> create(std::shared_ptr<QPDFObject>& obj);
    std::shared_ptr<QPDFObject> copy(bool shallow = false);

    std::shared_ptr<QPDFObject>
    getObj() const
    {
        static auto destroyed = QPDF_Destroyed::create();
        if (auto obj = object.lock()) {
            return obj;
        } else {
            return destroyed;
        }
    }

    std::shared_ptr<QPDFObject>
    getObj()
    {
        static auto destroyed = QPDF_Destroyed::create();
        if (auto obj = object.lock()) {
            return obj;
        } else {
            return destroyed;
        }
    }

  private:
    QPDF_IndirectRef() = default;
    std::weak_ptr<QPDFObject> object;
};

#endif // QPDF_INDIRECTREF_HH
