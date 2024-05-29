#include <qpdf/Pl_RunLength.hh>

#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

class Pl_RunLength::Members
{
  public:
    Members(action_e action) :
        action(action)
    {
    }

    action_e action;
    state_e state{st_top};
    unsigned char buf[128];
    unsigned int length{0};
};

Pl_RunLength::Pl_RunLength(std::string_view identifier, Pipeline& next, action_e action) :
    Pipeline(identifier, &next),
    m(new Members(action))
{
}

Pl_RunLength::Pl_RunLength(std::string_view identifier, Pipeline* next, action_e action) :
    Pipeline(identifier, next),
    m(new Members(action))
{
    if (!next) {
        throw std::logic_error("Pl_RunLength: next cannot be nullptr");
    }
}

Pl_RunLength::~Pl_RunLength() = default;
// Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer

void
Pl_RunLength::write(unsigned char const* data, size_t len)
{
    if (m->action == a_encode) {
        encode(data, len);
    } else {
        decode(data, len);
    }
}

void
Pl_RunLength::encode(unsigned char const* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        if ((m->state == st_top) != (m->length <= 1)) {
            throw std::logic_error("Pl_RunLength::encode: state/length inconsistency");
        }
        unsigned char ch = data[i];
        if ((m->length > 0) && ((m->state == st_copying) || (m->length < 128)) &&
            (ch == m->buf[m->length - 1])) {
            QTC::TC("libtests", "Pl_RunLength: switch to run", (m->length == 128) ? 0 : 1);
            if (m->state == st_copying) {
                --m->length;
                flush_encode();
                m->buf[0] = ch;
                m->length = 1;
            }
            m->state = st_run;
            m->buf[m->length] = ch;
            ++m->length;
        } else {
            if ((m->length == 128) || (m->state == st_run)) {
                flush_encode();
            } else if (m->length > 0) {
                m->state = st_copying;
            }
            m->buf[m->length] = ch;
            ++m->length;
        }
    }
}

void
Pl_RunLength::decode(unsigned char const* data, size_t len)
{
    for (size_t i = 0; i < len; ++i) {
        unsigned char ch = data[i];
        switch (m->state) {
        case st_top:
            if (ch < 128) {
                // length represents remaining number of bytes to copy
                m->length = 1U + ch;
                m->state = st_copying;
            } else if (ch > 128) {
                // length represents number of copies of next byte
                m->length = 257U - ch;
                m->state = st_run;
            } else // ch == 128
            {
                // EOD; stay in this state
            }
            break;

        case st_copying:
            next->write(&ch, 1);
            if (--m->length == 0) {
                m->state = st_top;
            }
            break;

        case st_run:
            for (unsigned int j = 0; j < m->length; ++j) {
                next->write(&ch, 1);
            }
            m->state = st_top;
            break;
        }
    }
}

void
Pl_RunLength::flush_encode()
{
    if (m->length == 128) {
        QTC::TC(
            "libtests",
            "Pl_RunLength flush full buffer",
            (m->state == st_copying   ? 0
                 : m->state == st_run ? 1
                                      : -1));
    }
    if (m->length == 0) {
        QTC::TC("libtests", "Pl_RunLength flush empty buffer");
    }
    if (m->state == st_run) {
        if ((m->length < 2) || (m->length > 128)) {
            throw std::logic_error("Pl_RunLength: invalid length in flush_encode for run");
        }
        auto ch = static_cast<unsigned char>(257 - m->length);
        next->write(&ch, 1);
        next->write(&m->buf[0], 1);
    } else if (m->length > 0) {
        auto ch = static_cast<unsigned char>(m->length - 1);
        next->write(&ch, 1);
        next->write(m->buf, m->length);
    }
    m->state = st_top;
    m->length = 0;
}

void
Pl_RunLength::finish()
{
    // When decoding, we might have read a length byte not followed by data, which means the stream
    // was terminated early, but we will just ignore this case since this is the only sensible thing
    // to do.
    if (m->action == a_encode) {
        flush_encode();
        unsigned char ch = 128;
        next->write(&ch, 1);
    }
    next->finish();
}
