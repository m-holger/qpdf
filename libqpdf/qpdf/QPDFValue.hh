#ifndef QPDFVALUE_HH
#define QPDFVALUE_HH

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjGen.hh>
#include <qpdf/Types.h>

#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Bool.hh>
#include <qpdf/QPDF_Destroyed.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_IndirectRef.hh>
#include <qpdf/QPDF_InlineImage.hh>
#include <qpdf/QPDF_Integer.hh>
#include <qpdf/QPDF_Name.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Operator.hh>
#include <qpdf/QPDF_Real.hh>
#include <qpdf/QPDF_Reserved.hh>
#include <qpdf/QPDF_Stream.hh>
#include <qpdf/QPDF_String.hh>
#include <qpdf/QPDF_Unresolved.hh>

#include <memory>
#include <string>
#include <string_view>
#include <variant>

class QPDF;
class QPDFObjectHandle;
class QPDFObject;

using ObjValue = std::variant<
    std::monostate,
    QPDF_Reserved,
    QPDF_Null,
    QPDF_Bool,
    QPDF_Integer,
    QPDF_Real,
    QPDF_String,
    QPDF_Name,
    QPDF_Array,
    QPDF_Dictionary,
    QPDF_Stream,
    QPDF_Operator,
    QPDF_InlineImage,
    QPDF_Unresolved,
    QPDF_Destroyed,
    QPDF_IndirectRef>;

class QPDFValue
{
    friend class QPDFObject;
    friend class QPDF;

  public:
    ~QPDFValue() = default;

    struct JSON_Descr
    {
        JSON_Descr(std::shared_ptr<std::string> input, std::string const& object) :
            input(input),
            object(object)
        {
        }

        std::shared_ptr<std::string> input;
        std::string object;
    };

    struct ChildDescr
    {
        ChildDescr(
            std::shared_ptr<QPDFObject> parent,
            std::string_view const& static_descr,
            std::string var_descr) :
            parent(parent),
            static_descr(static_descr),
            var_descr(var_descr)
        {
        }

        std::weak_ptr<QPDFObject> parent;
        std::string_view const& static_descr;
        std::string var_descr;
    };

    using Description = std::variant<std::string, JSON_Descr, ChildDescr>;

    void
    setDescription(QPDF* qpdf_p, std::shared_ptr<Description>& description, qpdf_offset_t offset)
    {
        qpdf = qpdf_p;
        object_description = description;
        setParsedOffset(offset);
    }
    void
    setDefaultDescription(QPDF* a_qpdf, QPDFObjGen const& a_og)
    {
        qpdf = a_qpdf;
        og = a_og;
    }
    void
    setChildDescription(
        QPDF* a_qpdf,
        std::shared_ptr<QPDFObject> parent,
        std::string_view const& static_descr,
        std::string var_descr)
    {
        object_description =
            std::make_shared<Description>(ChildDescr(parent, static_descr, var_descr));
        qpdf = a_qpdf;
    }
    std::string getDescription();
    bool
    hasDescription()
    {
        return object_description || og.isIndirect();
    }
    void
    setParsedOffset(qpdf_offset_t offset)
    {
        if (parsed_offset < 0) {
            parsed_offset = offset;
        }
    }
    qpdf_offset_t
    getParsedOffset()
    {
        return parsed_offset;
    }
    QPDF*
    getQPDF()
    {
        return qpdf;
    }
    QPDFObjGen
    getObjGen()
    {
        return og;
    }
    QPDFValue(ObjValue&& ov) :

        var(std::move(ov))
    {
    }

    QPDFValue() = default;

  private:
    QPDFValue(QPDFValue const&) = delete;
    QPDFValue& operator=(QPDFValue const&) = delete;
    QPDFValue(QPDFValue&& other) = default;

    QPDFValue& operator=(QPDFValue&&) = default;
    std::shared_ptr<Description> object_description;

  public:
    ObjValue var;

  public:
    QPDF* qpdf{nullptr};
    QPDFObjGen og{};
    qpdf_offset_t parsed_offset{-1};
};

#endif // QPDFVALUE_HH
