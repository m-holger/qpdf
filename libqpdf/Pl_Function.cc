#include <qpdf/Pl_Function.hh>

#include <stdexcept>

class Pl_Function::Members
{
};

Pl_Function::Pl_Function(std::string_view identifier, Pipeline& next, writer_t fn) :
    Pipeline(identifier, &next),
    fn(fn)
{
}

Pl_Function::Pl_Function(std::string_view identifier, Pipeline* next, writer_t fn) :
    Pipeline(identifier, next),
    fn(fn)
{
}

Pl_Function::Pl_Function(std::string_view identifier, Pipeline& next, writer_c_t fn, void* udata) :
    Pl_Function(identifier, &next, fn, udata)
{
}

Pl_Function::Pl_Function(std::string_view identifier, Pipeline* next, writer_c_t fn, void* udata) :
    Pipeline(identifier, next),
    fn([identifier, fn, udata](unsigned char const* data, size_t len) {
        int code = fn(data, len, udata);
        if (code != 0) {
            throw std::runtime_error(
                std::string(identifier) + " function returned code " + std::to_string(code));
        }
    })
{
}

Pl_Function::Pl_Function(
    std::string_view identifier, Pipeline& next, writer_c_char_t fn, void* udata) :
    Pl_Function(identifier, &next, fn, udata)
{
}

Pl_Function::Pl_Function(
    std::string_view identifier, Pipeline* next, writer_c_char_t fn, void* udata) :
    Pipeline(identifier, next),
    fn([identifier, fn, udata](unsigned char const* data, size_t len) {
        int code = fn(reinterpret_cast<char const*>(data), len, udata);
        if (code != 0) {
            throw std::runtime_error(
                std::string(identifier) + " function returned code " + std::to_string(code));
        }
    })
{
}

Pl_Function::~Pl_Function() = default;
// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer

void
Pl_Function::write(unsigned char const* buf, size_t len)
{
    fn(buf, len);
    if (next) {
        next->write(buf, len);
    }
}
