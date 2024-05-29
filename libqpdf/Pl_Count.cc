#include <qpdf/Pl_Count.hh>

#include <qpdf/QIntC.hh>

Pl_Count::~Pl_Count() = default;
// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer

void
Pl_Count::write(unsigned char const* buf, size_t len)
{
    if (len) {
        count += QIntC::to_offset(len);
        last_char = buf[len - 1];
        if (next) {
            next->write(buf, len);
        }
    }
}

void
Pl_Count::finish()
{
    if (next) {
        next->finish();
    }
}

qpdf_offset_t
Pl_Count::getCount() const
{
    return count;
}

unsigned char
Pl_Count::getLastChar() const
{
    return last_char;
}
