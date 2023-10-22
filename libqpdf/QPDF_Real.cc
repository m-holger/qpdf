#include <qpdf/QPDF_Real.hh>

#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>
#include <qpdf/QUtil.hh>

std::shared_ptr<QPDFObject>
QPDF_Real::copy(bool shallow)
{
    return QPDFObject::create<QPDF_Real>(val);
}

JSON
QPDF_Real::getJSON(int json_version)
{
    // While PDF allows .x or -.x, JSON does not. Rather than converting from string to double and
    // back, just handle this as a special case for JSON.
    std::string result;
    if (this->val.length() == 0) {
        // Can't really happen...
        result = "0";
    } else if (this->val.at(0) == '.') {
        result = "0" + this->val;
    } else if ((this->val.length() >= 2) && (this->val.at(0) == '-') && (this->val.at(1) == '.')) {
        result = "-0." + this->val.substr(2);
    } else {
        result = this->val;
    }
    return JSON::makeNumber(result);
}
