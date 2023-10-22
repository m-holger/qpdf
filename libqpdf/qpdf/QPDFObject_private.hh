#ifndef QPDFOBJECT_HH
#define QPDFOBJECT_HH

// NOTE: This file is called QPDFObject_private.hh instead of QPDFObject.hh because of
// include/qpdf/QPDFObject.hh. See comments there for an explanation.

#include <qpdf/Constants.h>
#include <qpdf/DLL.h>
#include <qpdf/JSON.hh>
#include <qpdf/QPDFValue.hh>
#include <qpdf/Types.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

class QPDF;
class QPDFObjectHandle;

template <class ...Fs>
struct overload : Fs... {
    overload(Fs const&... fs) : Fs{fs}...
    {}

    using Fs::operator()...;
};

class QPDFObject: public std::enable_shared_from_this<QPDFObject>
{
    friend class QPDFValue;

  public:
    QPDFObject() = default;

    std::shared_ptr<QPDFObject>
    copy(bool shallow = false)
    {
        return std::visit(overload
                   {
                       [](QPDF_Null & arg) -> std::shared_ptr<QPDFObject>{ return QPDFObject::create<QPDF_Null>(); },
                       [](std::monostate & arg) -> std::shared_ptr<QPDFObject>{ return QPDFObject::create<QPDF_Null>(); },
                       [shallow](QPDF_IndirectRef & arg) -> std::shared_ptr<QPDFObject>{ return arg.getObj()->copy(shallow);},
                       [shallow](auto & arg) -> std::shared_ptr<QPDFObject>{ return arg.copy(shallow); }
                   }, value.var);
    }
    std::string
    unparse()
    {
        return std::visit(overload
                          {
                              [](std::monostate & arg) -> std::string{ throw std::logic_error("unexpected variant in QPDFObject::unparse"); },
                              [](QPDF_IndirectRef & arg) -> std::string{ return arg.getObj()->unparse();},
                              [](auto & arg) -> std::string{ return arg.unparse(); }
                          }, value.var);
    }

    JSON
    getJSON(int json_version)
    {
        return std::visit(overload
                          {
                              [](std::monostate & arg) -> JSON{ throw std::logic_error("unexpected variant in QPDFObject::unparse"); },
                              [json_version](QPDF_IndirectRef & arg) -> JSON{ return arg.getObj()->getJSON(json_version);},
                              [json_version](auto & arg) -> JSON{ return arg.getJSON(json_version); }
                          }, value.var);
    }
    std::string
    getStringValue() const
    {
        if (auto* ptr5 = std::get_if<QPDF_Real>(&value.var)) {
            return ptr5->getStringValue();
        } else if (auto* ptr6 = std::get_if<QPDF_String>(&value.var)) {
            return ptr6->getStringValue();
        } else if (auto* ptr7 = std::get_if<QPDF_Name>(&value.var)) {
            return ptr7->getStringValue();
        } else if (auto* ptr11 = std::get_if<QPDF_Operator>(&value.var)) {
            return ptr11->getStringValue();
        } else if (auto* ptr12 = std::get_if<QPDF_InlineImage>(&value.var)) {
            return ptr12->getStringValue();
        } else if (auto* ptr14 = std::get_if<QPDF_IndirectRef>(&value.var)) {
            return ptr14->getObj()->getStringValue();
        } else {
            return "";
        }
    }

    // Return a unique type code for the object.For indirect references, return the type code of the
    // referenced object.
    qpdf_object_type_e
    getTypeCode() const
    {
        auto idx = value.var.index();
        return idx != ::ot_indirectref
            ? static_cast<qpdf_object_type_e>(idx)
            : std::get_if<QPDF_IndirectRef>(&value.var)->getObj()->getTypeCode();
    }

    // Return a unique type code for the object
    qpdf_object_type_e
    getRawTypeCode() const
    {
        return static_cast<qpdf_object_type_e>(value.var.index());
    }

    // Return a string literal that describes the type, useful for debugging and testing
    char const*
    getTypeName() const
    {
        switch (value.var.index()) {
        case 0:
            return "uninitialized";
        case 1:
            return "reserved";
        case 2:
            return "null";
        case 3:
            return "boolean";
        case 4:
            return "integer";
        case 5:
            return "real";
        case 6:
            return "string";
        case 7:
            return "name";
        case 8:
            return "array";
        case 9:
            return "dictionary";
        case 10:
            return "stream";
        case 11:
            return "operator";
        case 12:
            return "inline-image";
        case 13:
            return "unresolved";
        case 14:
            return "destroyed";
        case 15:
            return std::get_if<QPDF_IndirectRef>(&value.var)->getObj()->getTypeName();
        default:
            throw std::logic_error("unexpected variant in QPDFObject::getTypeName");
        }
    }

    QPDF*
    getQPDF() const
    {
        if (value.var.index() == ::ot_indirectref) {
            return std::get_if<QPDF_IndirectRef>(&value.var)->getObj()->getQPDF();
        } else {
            return value.qpdf;
        }
    }
    QPDFObjGen
    getObjGen() const
    {
        if (value.var.index() == ::ot_indirectref) {
            return std::get_if<QPDF_IndirectRef>(&value.var)->getObj()->getObjGen();
        } else {
            return value.og;
        }
    }
    void
    setDescription(
        QPDF* qpdf, std::shared_ptr<QPDFValue::Description>& description, qpdf_offset_t offset = -1)
    {
        if (value.var.index() == ::ot_indirectref) {
            std::get_if<QPDF_IndirectRef>(&value.var)
                ->getObj()
                ->setDescription(qpdf, description, offset);
        } else {
            value.setDescription(qpdf, description, offset);
        }
    }
    void
    setChildDescription(
        std::shared_ptr<QPDFObject> parent,
        std::string_view const& static_descr,
        std::string var_descr)
    {
        if (auto ptr = std::get_if<QPDF_IndirectRef>(&value.var)) {
            ptr->getObj()->setChildDescription(parent, static_descr, var_descr);
        } else {
            auto qpdf = parent ? parent->value.qpdf : nullptr;
            value.setChildDescription(qpdf, parent, static_descr, var_descr);
        }
    }
    bool
    getDescription(QPDF*& qpdf, std::string& description)
    {
        if (auto ptr = std::get_if<QPDF_IndirectRef>(&value.var)) {
            return ptr->getObj()->getDescription(qpdf, description);
        } else {
            qpdf = value.qpdf;
            description = value.getDescription();
            return qpdf != nullptr;
        }
    }
    std::string
    getDescription()
    {
        if (auto ptr = std::get_if<QPDF_IndirectRef>(&value.var)) {
            return ptr->getObj()->getDescription();
        } else {
            return value.getDescription();
        }
    }
    bool
    hasDescription()
    {
        if (auto ptr = std::get_if<QPDF_IndirectRef>(&value.var)) {
            return ptr->getObj()->hasDescription();
        } else {
            return value.hasDescription();
        }
    }
    void
    setParsedOffset(qpdf_offset_t offset)
    {
        if (auto ptr = std::get_if<QPDF_IndirectRef>(&value.var)) {
            ptr->getObj()->setParsedOffset(offset);
        } else {
            value.setParsedOffset(offset);
        }
    }
    qpdf_offset_t
    getParsedOffset()
    {
        if (auto ptr = std::get_if<QPDF_IndirectRef>(&value.var)) {
            return ptr->getObj()->getParsedOffset();
        } else {
            return value.getParsedOffset();
        }
    }
    void
    assign(std::shared_ptr<QPDFObject> o)
    {
        value = std::move(o->value);
        if (auto* ptr2 = std::get_if<QPDF_Array>(&value.var)) {
            ptr2->value = &value;
        } else if (auto* ptr3 = std::get_if<QPDF_Stream>(&value.var)) {
            ptr3->value = &value;
        }
    }
    void
    swapWith(std::shared_ptr<QPDFObject> o)
    {
        auto v = std::move(value);
        value = std::move(o->value);
        o->value = std::move(v);
        std::swap(value.og, o->value.og);
    }

    void
    setDefaultDescription(QPDF* qpdf, QPDFObjGen const& og)
    {
        if (auto ptr = std::get_if<QPDF_IndirectRef>(&value.var)) {
            ptr->getObj()->setDefaultDescription(qpdf, og);
        } else {
            // Intended for use by the QPDF class
            value.setDefaultDescription(qpdf, og);
        }
    }
    void
    setObjGen(QPDF* qpdf, QPDFObjGen const& og)
    {
        if (auto ptr = std::get_if<QPDF_IndirectRef>(&value.var)) {
            ptr->getObj()->setObjGen(qpdf, og);
        } else {
            value.qpdf = qpdf;
            value.og = og;
        }
    }
    void
    disconnect()
    {
        // Disconnect an object from its owning QPDF. This is called by QPDF's destructor.
        if (auto* ptr8 = std::get_if<QPDF_Array>(&value.var)) {
            ptr8->disconnect();
        } else if (auto* ptr9 = std::get_if<QPDF_Dictionary>(&value.var)) {
            ptr9->disconnect();
        } else if (auto* ptr10 = std::get_if<QPDF_Stream>(&value.var)) {
            ptr10->disconnect();
        }
        value.qpdf = nullptr;
        value.og = QPDFObjGen();
    }
    // Mark an object as destroyed. Used by QPDF's destructor for its indirect objects.
    void destroy();

    bool
    isUnresolved() const
    {
        return getTypeCode() == ::ot_unresolved;
    }
    void
    resolve()
    {
        if (isUnresolved()) {
            doResolve();
        }
    }
    void doResolve();

    template <typename T>
    T*
    as()
    {
        if (auto* ptr = std::get_if<T>(&value.var)) {
            return ptr;
        } else if (auto* indirect = std::get_if<QPDF_IndirectRef>(&value.var)) {
            return indirect->getObj()->as<T>();
        } else {
            return {};
        }
    }

    QPDFObject(QPDFValue&& v) :
        value(std::move(v))
    {
    }

    template <typename T, typename... Args>
    static std::shared_ptr<QPDFObject>
    create(Args&&... args)
    {
        return std::make_shared<QPDFObject>(QPDFValue(std::move(T(std::forward<Args>(args)...))));
    }

    template <typename T, typename... Args>
    static std::shared_ptr<QPDFObject>
    create_container(Args&&... args)
    {
        auto obj =
            std::make_shared<QPDFObject>(QPDFValue(std::move(T(std::forward<Args>(args)...))));
        obj->as<T>()->value = &obj->value;
        return obj;
    }

  private:
    QPDFObject(QPDFObject const&) = delete;
    QPDFObject& operator=(QPDFObject const&) = delete;

  public:
    QPDFValue value;
};

#endif // QPDFOBJECT_HH
