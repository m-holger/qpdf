#include <qpdf/QPDFTokenizer_private.hh>

// DO NOT USE ctype -- it is locale dependent for some things, and it's not worth the risk of
// including it in case it may accidentally be used.

#include <qpdf/InputSource_private.hh>
#include <qpdf/QIntC.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>
#include <qpdf/Util.hh>

#include <cstdlib>
#include <cstring>
#include <stdexcept>

using namespace qpdf;

using Token = QPDFTokenizer::Token;
using tt = QPDFTokenizer::token_type_e;

static inline bool
delimiter(char ch)
{
    return (
        ch == ' ' || ch == '\n' || ch == '/' || ch == '(' || ch == ')' || ch == '{' || ch == '}' ||
        ch == '<' || ch == '>' || ch == '[' || ch == ']' || ch == '%' || ch == '\t' || ch == '\r' ||
        ch == '\v' || ch == '\f' || ch == 0);
}

static inline bool
space(char ch)
{
    return ch == '\0' || util::is_space(ch);
}

namespace
{
    class QPDFWordTokenFinder: public InputSource::Finder
    {
      public:
        QPDFWordTokenFinder(InputSource& is, std::string const& str) :
            is(is),
            str(str)
        {
        }
        ~QPDFWordTokenFinder() override = default;
        bool check() override;

      private:
        InputSource& is;
        std::string str;
    };
} // namespace

bool
QPDFWordTokenFinder::check()
{
    // Find a word token matching the given string, preceded by a delimiter, and followed by a
    // delimiter or EOF.
    Tokenizer tokenizer;
    tokenizer.next_token(is, "finder", str.size() + 2);
    qpdf_offset_t pos = is.tell();
    if (tokenizer.get_type() != tt::tt_word || tokenizer.get_value() != str) {
        return false;
    }
    qpdf_offset_t token_start = is.getLastOffset();
    char next;
    bool next_okay = false;
    if (is.read(&next, 1) == 0) {
        next_okay = true;
    } else {
        next_okay = delimiter(next);
    }
    is.seek(pos, SEEK_SET);
    if (!next_okay) {
        return false;
    }
    if (token_start == 0) {
        // Can't actually happen...we never start the search at the beginning of the input.
        return false;
    }
    return true;
}

void
Tokenizer::reset()
{
    state_ = st_before_token;
    type_ = tt::tt_bad;
    val_.clear();
    raw_val_.clear();
    error_message_ = "";
    before_token_ = true;
    in_token_ = false;
    char_to_unread_ = '\0';
    inline_image_bytes_ = 0;
    string_depth_ = 0;
    bad_ = false;
}

QPDFTokenizer::Token::Token(token_type_e type, std::string const& value) :
    type(type),
    value(value),
    raw_value(value)
{
    if (type == tt_string) {
        raw_value = QPDFObjectHandle::newString(value).unparse();
    } else if (type == tt_name) {
        raw_value = QPDFObjectHandle::newName(value).unparse();
    }
}

QPDFTokenizer::QPDFTokenizer() :
    m(std::make_unique<qpdf::Tokenizer>())
{
}

QPDFTokenizer::~QPDFTokenizer() = default;

Tokenizer::Tokenizer()
{
    reset();
}

void
QPDFTokenizer::allowEOF()
{
    m->allow_eof();
}

void
Tokenizer::allow_eof()
{
    allow_eof_ = true;
}

void
QPDFTokenizer::includeIgnorable()
{
    m->include_ignorable();
}

void
Tokenizer::include_ignorable()
{
    include_ignorable_ = true;
}

void
QPDFTokenizer::presentCharacter(char ch)
{
    m->present_character(ch);
}

void
Tokenizer::present_character(char ch)
{
    handle_character(ch);

    if (in_token_) {
        raw_val_ += ch;
    }
}

void
Tokenizer::handle_character(char ch)
{
    // In some cases, functions called below may call a second handler. This happens whenever you
    // have to use a character from the next token to detect the end of the current token.

    switch (state_) {
    case st_top:
        in_top(ch);
        return;

    case st_in_space:
        in_space(ch);
        return;

    case st_in_comment:
        in_comment(ch);
        return;

    case st_lt:
        in_lt(ch);
        return;

    case st_gt:
        in_gt(ch);
        return;

    case st_in_string:
        in_string(ch);
        return;

    case st_name:
        in_name(ch);
        return;

    case st_number:
        in_number(ch);
        return;

    case st_real:
        in_real(ch);
        return;

    case st_string_after_cr:
        in_string_after_cr(ch);
        return;

    case st_string_escape:
        in_string_escape(ch);
        return;

    case st_char_code:
        in_char_code(ch);
        return;

    case st_literal:
        in_literal(ch);
        return;

    case st_inline_image:
        in_inline_image(ch);
        return;

    case st_in_hexstring:
        in_hexstring(ch);
        return;

    case st_in_hexstring_2nd:
        in_hexstring_2nd(ch);
        return;

    case st_name_hex1:
        in_name_hex1(ch);
        return;

    case st_name_hex2:
        in_name_hex2(ch);
        return;

    case st_sign:
        in_sign(ch);
        return;

    case st_decimal:
        in_decimal(ch);
        return;

    case (st_before_token):
        in_before_token(ch);
        return;

    case (st_token_ready):
        in_token_ready(ch);
        return;

    default:
        throw std::logic_error("INTERNAL ERROR: invalid state while reading token");
    }
}

void
Tokenizer::in_token_ready(char ch)
{
    throw std::logic_error(
        "INTERNAL ERROR: QPDF tokenizer presented character while token is waiting");
}

void
Tokenizer::in_before_token(char ch)
{
    // Note: we specifically do not use ctype here.  It is locale-dependent.
    if (space(ch)) {
        before_token_ = !include_ignorable_;
        in_token_ = include_ignorable_;
        if (include_ignorable_) {
            state_ = st_in_space;
        }
    } else if (ch == '%') {
        before_token_ = !include_ignorable_;
        in_token_ = include_ignorable_;
        state_ = st_in_comment;
    } else {
        before_token_ = false;
        in_token_ = true;
        in_top(ch);
    }
}

void
Tokenizer::in_top(char ch)
{
    switch (ch) {
    case '(':
        string_depth_ = 1;
        state_ = st_in_string;
        return;

    case '<':
        state_ = st_lt;
        return;

    case '>':
        state_ = st_gt;
        return;

    case (')'):
        type_ = tt::tt_bad;
        error_message_ = "unexpected )";
        state_ = st_token_ready;
        return;

    case '[':
        type_ = tt::tt_array_open;
        state_ = st_token_ready;
        return;

    case ']':
        type_ = tt::tt_array_close;
        state_ = st_token_ready;
        return;

    case '{':
        type_ = tt::tt_brace_open;
        state_ = st_token_ready;
        return;

    case '}':
        type_ = tt::tt_brace_close;
        state_ = st_token_ready;
        return;

    case '/':
        state_ = st_name;
        val_ += ch;
        return;

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
        state_ = st_number;
        return;

    case '+':
    case '-':
        state_ = st_sign;
        return;

    case '.':
        state_ = st_decimal;
        return;

    default:
        state_ = st_literal;
        return;
    }
}

void
Tokenizer::in_space(char ch)
{
    // We only enter this state if include_ignorable_ is true.
    if (!space(ch)) {
        type_ = tt::tt_space;
        in_token_ = false;
        char_to_unread_ = ch;
        state_ = st_token_ready;
    }
}

void
Tokenizer::in_comment(char ch)
{
    if ((ch == '\r') || (ch == '\n')) {
        if (include_ignorable_) {
            type_ = tt::tt_comment;
            in_token_ = false;
            char_to_unread_ = ch;
            state_ = st_token_ready;
        } else {
            state_ = st_before_token;
        }
    }
}

void
Tokenizer::in_string(char ch)
{
    switch (ch) {
    case '\\':
        state_ = st_string_escape;
        return;

    case '(':
        val_ += ch;
        ++string_depth_;
        return;

    case ')':
        if (--string_depth_ == 0) {
            type_ = tt::tt_string;
            state_ = st_token_ready;
            return;
        }

        val_ += ch;
        return;

    case '\r':
        // CR by itself is converted to LF
        val_ += '\n';
        state_ = st_string_after_cr;
        return;

    case '\n':
        val_ += ch;
        return;

    default:
        val_ += ch;
        return;
    }
}

void
Tokenizer::in_name(char ch)
{
    if (delimiter(ch)) {
        // A C-locale whitespace character or delimiter terminates token.  It is important to unread
        // the whitespace character even though it is ignored since it may be the newline after a
        // stream keyword.  Removing it here could make the stream-reading code break on some files,
        // though not on any files in the test suite as of this
        // writing.

        type_ = bad_ ? tt::tt_bad : tt::tt_name;
        in_token_ = false;
        char_to_unread_ = ch;
        state_ = st_token_ready;
    } else if (ch == '#') {
        char_code_ = 0;
        state_ = st_name_hex1;
    } else {
        val_ += ch;
    }
}

void
Tokenizer::in_name_hex1(char ch)
{
    hex_char_ = ch;

    if (char hval = util::hex_decode_char(ch); hval < '\20') {
        char_code_ = int(hval) << 4;
        state_ = st_name_hex2;
    } else {
        error_message_ = "name with stray # will not work with PDF >= 1.2";
        // Use null to encode a bad # -- this is reversed in QPDF_Name::normalizeName.
        val_ += '\0';
        state_ = st_name;
        in_name(ch);
    }
}

void
Tokenizer::in_name_hex2(char ch)
{
    if (char hval = util::hex_decode_char(ch); hval < '\20') {
        char_code_ |= int(hval);
    } else {
        error_message_ = "name with stray # will not work with PDF >= 1.2";
        // Use null to encode a bad # -- this is reversed in QPDF_Name::normalizeName.
        val_ += '\0';
        val_ += hex_char_;
        state_ = st_name;
        in_name(ch);
        return;
    }
    if (char_code_ == 0) {
        error_message_ = "null character not allowed in name token";
        val_ += "#00";
        state_ = st_name;
        bad_ = true;
    } else {
        val_ += char(char_code_);
        state_ = st_name;
    }
}

void
Tokenizer::in_sign(char ch)
{
    if (util::is_digit(ch)) {
        state_ = st_number;
    } else if (ch == '.') {
        state_ = st_decimal;
    } else {
        state_ = st_literal;
        in_literal(ch);
    }
}

void
Tokenizer::in_decimal(char ch)
{
    if (util::is_digit(ch)) {
        state_ = st_real;
    } else {
        state_ = st_literal;
        in_literal(ch);
    }
}

void
Tokenizer::in_number(char ch)
{
    if (util::is_digit(ch)) {
    } else if (ch == '.') {
        state_ = st_real;
    } else if (delimiter(ch)) {
        type_ = tt::tt_integer;
        state_ = st_token_ready;
        in_token_ = false;
        char_to_unread_ = ch;
    } else {
        state_ = st_literal;
    }
}

void
Tokenizer::in_real(char ch)
{
    if (util::is_digit(ch)) {
    } else if (delimiter(ch)) {
        type_ = tt::tt_real;
        state_ = st_token_ready;
        in_token_ = false;
        char_to_unread_ = ch;
    } else {
        state_ = st_literal;
    }
}
void
Tokenizer::in_string_escape(char ch)
{
    state_ = st_in_string;
    switch (ch) {
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
        state_ = st_char_code;
        char_code_ = 0;
        digit_count_ = 0;
        in_char_code(ch);
        return;

    case 'n':
        val_ += '\n';
        return;

    case 'r':
        val_ += '\r';
        return;

    case 't':
        val_ += '\t';
        return;

    case 'b':
        val_ += '\b';
        return;

    case 'f':
        val_ += '\f';
        return;

    case '\n':
        return;

    case '\r':
        state_ = st_string_after_cr;
        return;

    default:
        // PDF spec says backslash is ignored before anything else
        val_ += ch;
        return;
    }
}

void
Tokenizer::in_string_after_cr(char ch)
{
    state_ = st_in_string;
    if (ch != '\n') {
        in_string(ch);
    }
}

void
Tokenizer::in_lt(char ch)
{
    if (ch == '<') {
        type_ = tt::tt_dict_open;
        state_ = st_token_ready;
        return;
    }

    state_ = st_in_hexstring;
    in_hexstring(ch);
}

void
Tokenizer::in_gt(char ch)
{
    if (ch == '>') {
        type_ = tt::tt_dict_close;
        state_ = st_token_ready;
    } else {
        type_ = tt::tt_bad;
        error_message_ = "unexpected >";
        in_token_ = false;
        char_to_unread_ = ch;
        state_ = st_token_ready;
    }
}

void
Tokenizer::in_literal(char ch)
{
    if (delimiter(ch)) {
        // A C-locale whitespace character or delimiter terminates token.  It is important to unread
        // the whitespace character even though it is ignored since it may be the newline after a
        // stream keyword.  Removing it here could make the stream-reading code break on some files,
        // though not on any files in the test suite as of this writing.

        in_token_ = false;
        char_to_unread_ = ch;
        state_ = st_token_ready;
        type_ = (raw_val_ == "true") || (raw_val_ == "false")
            ? tt::tt_bool
            : (raw_val_ == "null" ? tt::tt_null : tt::tt_word);
    }
}

void
Tokenizer::in_hexstring(char ch)
{
    if (char hval = util::hex_decode_char(ch); hval < '\20') {
        char_code_ = int(hval) << 4;
        state_ = st_in_hexstring_2nd;

    } else if (ch == '>') {
        type_ = tt::tt_string;
        state_ = st_token_ready;

    } else if (space(ch)) {
        // ignore

    } else {
        type_ = tt::tt_bad;
        error_message_ = std::string("invalid character (") + ch + ") in hexstring";
        state_ = st_token_ready;
    }
}

void
Tokenizer::in_hexstring_2nd(char ch)
{
    if (char hval = util::hex_decode_char(ch); hval < '\20') {
        val_ += char(char_code_) | hval;
        state_ = st_in_hexstring;

    } else if (ch == '>') {
        // PDF spec says odd hexstrings have implicit trailing 0.
        val_ += char(char_code_);
        type_ = tt::tt_string;
        state_ = st_token_ready;

    } else if (space(ch)) {
        // ignore

    } else {
        type_ = tt::tt_bad;
        error_message_ = std::string("invalid character (") + ch + ") in hexstring";
        state_ = st_token_ready;
    }
}

void
Tokenizer::in_char_code(char ch)
{
    bool handled = false;
    if (('0' <= ch) && (ch <= '7')) {
        char_code_ = 8 * char_code_ + (int(ch) - int('0'));
        if (++(digit_count_) < 3) {
            return;
        }
        handled = true;
    }
    // We've accumulated \ddd or we have \d or \dd followed by other than an octal digit. The PDF
    // Spec says to ignore high-order overflow.
    val_ += char(char_code_ % 256);
    state_ = st_in_string;
    if (!handled) {
        in_string(ch);
    }
}

void
Tokenizer::in_inline_image(char ch)
{
    if ((raw_val_.length() + 1) == inline_image_bytes_) {
        type_ = tt::tt_inline_image;
        inline_image_bytes_ = 0;
        state_ = st_token_ready;
    }
}

void
QPDFTokenizer::presentEOF()
{
    m->present_eof();
}

void
Tokenizer::present_eof()
{
    switch (state_) {
    case st_name:
    case st_name_hex1:
    case st_name_hex2:
    case st_number:
    case st_real:
    case st_sign:
    case st_decimal:
    case st_literal:
        // Push any delimiter to the state machine to finish off the final token.
        present_character('\f');
        in_token_ = true;
        break;

    case st_top:
    case st_before_token:
        type_ = tt::tt_eof;
        break;

    case st_in_space:
        type_ = include_ignorable_ ? tt::tt_space : tt::tt_eof;
        break;

    case st_in_comment:
        type_ = include_ignorable_ ? tt::tt_comment : tt::tt_bad;
        break;

    case st_token_ready:
        break;

    default:
        type_ = tt::tt_bad;
        error_message_ = "EOF while reading token";
    }
    state_ = st_token_ready;
}

void
QPDFTokenizer::expectInlineImage(std::shared_ptr<InputSource> input)
{
    m->expect_inline_image(*input);
}

void
QPDFTokenizer::expectInlineImage(InputSource& input)
{
    m->expect_inline_image(input);
}

void
Tokenizer::expect_inline_image(InputSource& input)
{
    if (state_ == st_token_ready) {
        reset();
    } else if (state_ != st_before_token) {
        throw std::logic_error(
            "QPDFTokenizer::expectInlineImage called when tokenizer is in improper state");
    }
    find_ei(input);
    before_token_ = false;
    in_token_ = true;
    state_ = st_inline_image;
}

void
Tokenizer::find_ei(InputSource& input)
{
    qpdf_offset_t last_offset = input.getLastOffset();
    qpdf_offset_t pos = input.tell();

    // Use QPDFWordTokenFinder to find EI surrounded by delimiters. Then read the next several
    // tokens or up to EOF. If we find any suspicious-looking or tokens, this is probably still part
    // of the image data, so keep looking for EI. Stop at the first EI that passes. If we get to the
    // end without finding one, return the last EI we found. Store the number of bytes expected in
    // the inline image including the EI and use that to break out of inline image, falling back to
    // the old method if needed.

    bool okay = false;
    bool first_try = true;
    while (!okay) {
        QPDFWordTokenFinder f(input, "EI");
        if (!input.findFirst("EI", input.tell(), 0, f)) {
            break;
        }
        inline_image_bytes_ = QIntC::to_size(input.tell() - pos - 2);

        Tokenizer check;
        bool found_bad = false;
        // Look at the next 10 tokens or up to EOF. The next inline image's image data would look
        // like bad tokens, but there will always be at least 10 tokens between one inline image's
        // EI and the next valid one's ID since width, height, bits per pixel, and color space are
        // all required as well as a BI and ID. If we get 10 good tokens in a row or hit EOF, we can
        // be pretty sure we've found the actual EI.
        for (int i = 0; i < 10; ++i) {
            check.next_token(input, "checker");
            auto typ = check.get_type();
            if (typ == tt::tt_eof) {
                okay = true;
            } else if (typ == tt::tt_bad) {
                found_bad = true;
            } else if (typ == tt::tt_word) {
                // The qpdf tokenizer lumps alphabetic and otherwise uncategorized characters into
                // "words". We recognize strings of alphabetic characters as potential valid
                // operators for purposes of telling whether we're in valid content or not. It's not
                // perfect, but it should work more reliably than what we used to do, which was
                // already good enough for the vast majority of files.
                bool found_alpha = false;
                bool found_non_printable = false;
                bool found_other = false;
                for (char ch: check.get_value()) {
                    if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch == '*')) {
                        // Treat '*' as alpha since there are valid PDF operators that contain *
                        // along with alphabetic characters.
                        found_alpha = true;
                    } else if (static_cast<signed char>(ch) < 32 && !space(ch)) {
                        // Compare ch as a signed char so characters outside of 7-bit will be < 0.
                        found_non_printable = true;
                        break;
                    } else {
                        found_other = true;
                    }
                }
                if (found_non_printable || (found_alpha && found_other)) {
                    found_bad = true;
                }
            }
            if (okay || found_bad) {
                break;
            }
        }
        if (!found_bad) {
            okay = true;
        }
        if (!okay) {
            first_try = false;
        }
    }
    if (okay && (!first_try)) {
    }

    input.seek(pos, SEEK_SET);
    input.setLastOffset(last_offset);
}

bool
QPDFTokenizer::getToken(Token& token, bool& unread_char, char& ch)
{
    return m->get_token(token, unread_char, ch);
}

bool
Tokenizer::get_token(Token& token, bool& unread_char, char& ch)
{
    bool ready = (state_ == st_token_ready);
    unread_char = !in_token_ && !before_token_;
    ch = char_to_unread_;
    if (ready) {
        token = (!(type_ == tt::tt_name || type_ == tt::tt_string))
            ? Token(type_, raw_val_, raw_val_, error_message_)
            : Token(type_, val_, raw_val_, error_message_);

        reset();
    }
    return ready;
}

bool
QPDFTokenizer::betweenTokens()
{
    return m->between_tokens();
}

bool
Tokenizer::between_tokens()
{
    return before_token_;
}

QPDFTokenizer::Token
QPDFTokenizer::readToken(
    InputSource& input, std::string const& context, bool allow_bad, size_t max_len)
{
    return m->read_token(input, context, allow_bad, max_len);
}

QPDFTokenizer::Token
QPDFTokenizer::readToken(
    std::shared_ptr<InputSource> input, std::string const& context, bool allow_bad, size_t max_len)
{
    return m->read_token(*input, context, allow_bad, max_len);
}

QPDFTokenizer::Token
Tokenizer::read_token(
    InputSource& input, std::string const& context, bool allow_bad, size_t max_len)
{
    next_token(input, context, max_len);

    Token token;
    bool unread_char;
    char char_to_unread;
    get_token(token, unread_char, char_to_unread);

    if (token.getType() == tt::tt_bad) {
        if (allow_bad) {
        } else {
            throw QPDFExc(
                qpdf_e_damaged_pdf,
                input.getName(),
                context.empty() ? "offset " + std::to_string(input.getLastOffset()) : context,
                input.getLastOffset(),
                token.getErrorMessage());
        }
    }
    return token;
}

bool
Tokenizer::next_token(InputSource& input, std::string const& context, size_t max_len)
{
    if (state_ != st_inline_image) {
        reset();
    }
    qpdf_offset_t offset = input.fastTell();

    while (state_ != st_token_ready) {
        char ch;
        if (!input.fastRead(ch)) {
            present_eof();

            if ((type_ == tt::tt_eof) && (!allow_eof_)) {
                // Nothing in the qpdf library calls readToken without allowEOF anymore, so this
                // case is not exercised.
                type_ = tt::tt_bad;
                error_message_ = "unexpected EOF";
                offset = input.getLastOffset();
            }
        } else {
            handle_character(ch);
            if (before_token_) {
                ++offset;
            }
            if (in_token_) {
                raw_val_ += ch;
            }
            if (max_len && (raw_val_.length() >= max_len) && (state_ != st_token_ready)) {
                // terminate this token now
                type_ = tt::tt_bad;
                state_ = st_token_ready;
                error_message_ = "exceeded allowable length while reading token";
            }
        }
    }

    input.fastUnread(!in_token_ && !before_token_);

    if (type_ != tt::tt_eof) {
        input.setLastOffset(offset);
    }

    return error_message_.empty();
}
