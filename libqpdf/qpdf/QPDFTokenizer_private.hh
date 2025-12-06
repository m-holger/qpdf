#ifndef QPDFTOKENIZER_PRIVATE_HH
#define QPDFTOKENIZER_PRIVATE_HH

#include <qpdf/QPDFTokenizer.hh>

namespace qpdf
{

    class Tokenizer
    {
      public:
        Tokenizer();
        Tokenizer(Tokenizer const&) = delete;
        Tokenizer& operator=(Tokenizer const&) = delete;

        // Methods to support QPDFTokenizer. See QPDFTokenizer.hh for detail. Some of these are used
        // by Tokenizer internally but are not accessed directly by the rest of qpdf.

        void allow_eof();
        void include_ignorable();
        void present_character(char ch);
        void present_eof();
        bool between_tokens();

        // If a token is available, return true and initialize token with the token, unread_char
        // with whether or not we have to unread the last character, and if unread_char, ch with the
        // character to unread.
        bool get_token(QPDFTokenizer::Token& token, bool& unread_char, char& ch);

        // Read a token from an input source. Context describes the context in which the token is
        // being read and is used in the exception thrown if there is an error. After a token is
        // read, the position of the input source returned by input->tell() points to just after the
        // token, and the input source's "last offset" as returned by input->getLastOffset() points
        // to the beginning of the token.
        QPDFTokenizer::Token read_token(
            InputSource& input,
            std::string const& context,
            bool allow_bad = false,
            size_t max_len = 0);

        // Calling this method puts the tokenizer in a state for reading inline images. You should
        // call this method after reading the character following the ID operator. In that state, it
        // will return all data up to BUT NOT INCLUDING the next EI token. After you call this
        // method, the next call to read_token (or the token created next time get_token returns
        // true) will either be tt_inline_image or tt_bad. This is the only way read_token returns a
        // tt_inline_image token.
        void expect_inline_image(InputSource& input);

        // Read a token from an input source. Context describes the context in which the token is
        // being read and is used in the exception thrown if there is an error. After a token is
        // read, the position of the input source returned by input->tell() points to just after the
        // token, and the input source's "last offset" as returned by input->getLastOffset() points
        // to the beginning of the token. Returns false if the token is bad or if scanning produced
        // an error message for any reason.
        bool next_token(InputSource& input, std::string const& context, size_t max_len = 0);

        // The following methods are only valid after next_token has been called and until another
        // QPDFTokenizer method is called. They allow the results of calling next_token to be
        // accessed without creating a Token, thus avoiding copying information that may not be
        // needed.

        inline QPDFTokenizer::token_type_e
        get_type() const
        {
            return type_;
        }
        inline std::string const&
        get_value() const
        {
            return (type_ == QPDFTokenizer::tt_name || type_ == QPDFTokenizer::tt_string)
                ? val_
                : raw_val_;
        }
        inline std::string const&
        get_raw_value() const
        {
            return raw_val_;
        }
        inline std::string const&
        get_error_message() const
        {
            return error_message_;
        }

      private:
        void find_ei(InputSource& input);

        enum state_e {
            st_top,
            st_in_hexstring,
            st_in_string,
            st_in_hexstring_2nd,
            st_name,
            st_literal,
            st_in_space,
            st_in_comment,
            st_string_escape,
            st_char_code,
            st_string_after_cr,
            st_lt,
            st_gt,
            st_inline_image,
            st_sign,
            st_number,
            st_real,
            st_decimal,
            st_name_hex1,
            st_name_hex2,
            st_before_token,
            st_token_ready
        };

        void handle_character(char);
        void in_before_token(char);
        void in_top(char);
        void in_space(char);
        void in_comment(char);
        void in_string(char);
        void in_name(char);
        void in_lt(char);
        void in_gt(char);
        void in_string_after_cr(char);
        void in_string_escape(char);
        void in_literal(char);
        void in_char_code(char);
        void in_hexstring(char);
        void in_hexstring_2nd(char);
        void in_inline_image(char);
        void in_token_ready(char);
        void in_name_hex1(char);
        void in_name_hex2(char);
        void in_sign(char);
        void in_decimal(char);
        void in_number(char);
        void in_real(char);
        void reset();

        // Lexer state
        state_e state_;

        bool allow_eof_{false};
        bool include_ignorable_{false};

        // Current token accumulation
        QPDFTokenizer::token_type_e type_;
        std::string val_;
        std::string raw_val_;
        std::string error_message_;
        bool before_token_;
        bool in_token_;
        char char_to_unread_;
        size_t inline_image_bytes_;
        bool bad_;

        // State for strings
        int string_depth_;
        int char_code_;
        char hex_char_;
        int digit_count_;
    };

} // namespace qpdf

#endif // QPDFTOKENIZER_PRIVATE_HH
