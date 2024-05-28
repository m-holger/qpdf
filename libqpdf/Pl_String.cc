#include <qpdf/Pl_String.hh>

#include <stdexcept>

Pl_String::Pl_String(std::string_view identifier, Pipeline& next, std::string& s) :
    Pipeline(identifier, &next),
    s(s)
{
}

Pl_String::Pl_String(std::string_view identifier, Pipeline* next, std::string& s) :
    Pipeline(identifier, next),
    s(s)
{
}

Pl_String::~Pl_String() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer
}

void
Pl_String::write(unsigned char const* buf, size_t len)
{
    s.append(reinterpret_cast<char const*>(buf), len);
    if (next) {
        next->write(buf, len);
    }
}
