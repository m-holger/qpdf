#include <qpdf/QPDF_Name.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>
#include <qpdf/QUtil.hh>

//std::shared_ptr<QPDFObject>
//QPDF_Name::create(std::string const& name)
//{
//    // auto obj = std::make_shared<QPDFObject>();
//    // obj->value = std::make_shared<Value>(QPDF_Name(name));
//    // return obj;
//    return QPDFObject::create<QPDF_Name>(name);
//    // return do_create(new QPDF_Name(name));
//}

std::shared_ptr<QPDFObject>
QPDF_Name::copy(bool shallow)
{
    return QPDFObject::create<QPDF_Name>(name);
}

std::string
QPDF_Name::normalizeName(std::string const& name)
{
    if (name.empty()) {
        return name;
    }
    std::string result;
    result += name.at(0);
    for (size_t i = 1; i < name.length(); ++i) {
        char ch = name.at(i);
        // Don't use locale/ctype here; follow PDF spec guidelines.
        if (ch == '\0') {
            // QPDFTokenizer embeds a null character to encode an invalid #.
            result += "#";
        } else if (
            ch < 33 || ch == '#' || ch == '/' || ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
            ch == '<' || ch == '>' || ch == '[' || ch == ']' || ch == '%' || ch > 126) {
            result += QUtil::hex_encode_char(ch);
        } else {
            result += ch;
        }
    }
    return result;
}
