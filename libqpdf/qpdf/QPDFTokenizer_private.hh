#ifndef QPDFTOKENIZER_PRIVATE_HH
#define QPDFTOKENIZER_PRIVATE_HH

#include <qpdf/QPDFTokenizer.hh>

namespace qpdf::impl
{

    /// @brief Internal tokenizer implementation for PDF content and objects.
    ///
    /// This class provides the core tokenization functionality for parsing PDF files. It maintains
    /// state for lexical analysis and produces tokens that represent PDF objects, operators, and
    /// other syntactic elements. The tokenizer operates as a state machine that processes input
    /// character by character.
    class Tokenizer
    {
      public:
        /// @brief Construct a new Tokenizer instance.
        Tokenizer();
        Tokenizer(Tokenizer const&) = delete;
        Tokenizer& operator=(Tokenizer const&) = delete;

        /// @brief Configure the tokenizer to treat EOF as a valid token instead of an error.
        ///
        /// This is used when tokenizing content streams where EOF is a normal termination condition.
        void allow_eof();

        /// @brief Configure the tokenizer to return ignorable tokens (spaces and comments).
        ///
        /// By default, whitespace and comments are consumed but not returned. When this is enabled,
        /// they are returned as separate tokens.
        void include_ignorable();

        /// @brief Present a single character to the tokenizer (push mode).
        ///
        /// @param ch The character to process.
        void present_character(char ch);

        /// @brief Signal end-of-file to the tokenizer (push mode).
        ///
        /// This finalizes any partially-read token and transitions to the token-ready state.
        void present_eof();

        /// @brief Check if the tokenizer is between tokens.
        ///
        /// @return true if the current position is whitespace or comment that is not part of a
        /// token.
        bool between_tokens();

        /// @brief Retrieve a completed token (push mode).
        ///
        /// @param[out] token The completed token.
        /// @param[out] unread_char true if a character needs to be unread.
        /// @param[out] ch The character to unread if unread_char is true.
        /// @return true if a token is available.
        bool get_token(QPDFTokenizer::Token& token, bool& unread_char, char& ch);

        /// @brief Read a complete token from an input source (pull mode).
        ///
        /// @param input The input source to read from.
        /// @param context Description of the reading context for error messages.
        /// @param allow_bad If true, return bad tokens instead of throwing exceptions.
        /// @param max_len Maximum token length (0 = unlimited).
        /// @return The completed token.
        QPDFTokenizer::Token read_token(
            InputSource& input,
            std::string const& context,
            bool allow_bad = false,
            size_t max_len = 0);

        /// @brief Prepare to read an inline image.
        ///
        /// After calling this method, the next token read will be tt_inline_image containing all
        /// data up to (but not including) the EI operator. This must be called immediately after
        /// reading the ID operator in a content stream.
        ///
        /// @param input The input source positioned after the ID operator and its trailing space.
        void expect_inline_image(InputSource& input);

        /// @brief Read the next token from an input source.
        ///
        /// This is a lower-level alternative to read_token that doesn't create a Token object. Use
        /// type(), value(), raw_value(), and error_message() to access the results.
        ///
        /// @param input The input source to read from.
        /// @param context Description of the reading context for error messages.
        /// @param max_len Maximum token length (0 = unlimited).
        /// @return true if successful, false if the token is bad or an error occurred.
        bool next_token(InputSource& input, std::string const& context, size_t max_len = 0);

        /// @brief Get the type of the most recently read token.
        ///
        /// Valid only after next_token() has been called.
        ///
        /// @return The token type.
        QPDFTokenizer::token_type_e
        type() const
        {
            return type_;
        }

        /// @brief Get the value of the most recently read token.
        ///
        /// Valid only after next_token() has been called. For strings and names, returns the
        /// decoded value. For other types, returns the raw value.
        ///
        /// @return The token value.
        std::string const&
        value() const
        {
            return (type_ == QPDFTokenizer::tt_name || type_ == QPDFTokenizer::tt_string)
                ? val_
                : raw_val_;
        }

        /// @brief Get the raw (undecoded) value of the most recently read token.
        ///
        /// Valid only after next_token() has been called.
        ///
        /// @return The raw token value as it appears in the input.
        std::string const&
        raw_value() const
        {
            return raw_val_;
        }

        /// @brief Get any error message from the most recently read token.
        ///
        /// Valid only after next_token() has been called.
        ///
        /// @return The error message, or empty string if no error occurred.
        std::string const&
        error_message() const
        {
            return error_message_;
        }

      private:
        /// @brief Locate the EI operator that terminates an inline image.
        ///
        /// @param input The input source to search.
        void find_ei(InputSource& input);

        /// @brief Tokenizer state machine states.
        enum state_e {
            st_top,               ///< Initial state, no token being read
            st_in_hexstring,      ///< Reading hexadecimal string
            st_in_string,         ///< Reading literal string
            st_in_hexstring_2nd,  ///< Reading second hex digit in hex string
            st_name,              ///< Reading name object
            st_literal,           ///< Reading literal token (word/number/keyword)
            st_in_space,          ///< Consuming whitespace
            st_in_comment,        ///< Consuming comment
            st_string_escape,     ///< Processing escape sequence in string
            st_char_code,         ///< Reading octal character code in string
            st_string_after_cr,   ///< After CR in string (handling line endings)
            st_lt,                ///< After '<' (could be hex string or dict)
            st_gt,                ///< After '>' (could be end of hex string or dict)
            st_inline_image,      ///< Reading inline image data
            st_sign,              ///< After '+' or '-' sign
            st_number,            ///< Reading integer
            st_real,              ///< Reading real number (after decimal point)
            st_decimal,           ///< After decimal point
            st_name_hex1,         ///< Reading first hex digit in name escape
            st_name_hex2,         ///< Reading second hex digit in name escape
            st_before_token,      ///< Before a token (for ignorable content)
            st_token_ready        ///< Token is complete and ready
        };

        /// @brief Dispatch character to appropriate state handler.
        ///
        /// @param ch The character to process.
        void handle_character(char);

        /// @brief Process character in st_before_token state.
        void in_before_token(char);

        /// @brief Process character in st_top state.
        void in_top(char);

        /// @brief Process character in st_in_space state.
        void in_space(char);

        /// @brief Process character in st_in_comment state.
        void in_comment(char);

        /// @brief Process character in st_in_string state.
        void in_string(char);

        /// @brief Process character in st_name state.
        void in_name(char);

        /// @brief Process character in st_lt state.
        void in_lt(char);

        /// @brief Process character in st_gt state.
        void in_gt(char);

        /// @brief Process character in st_string_after_cr state.
        void in_string_after_cr(char);

        /// @brief Process character in st_string_escape state.
        void in_string_escape(char);

        /// @brief Process character in st_literal state.
        void in_literal(char);

        /// @brief Process character in st_char_code state.
        void in_char_code(char);

        /// @brief Process character in st_in_hexstring state.
        void in_hexstring(char);

        /// @brief Process character in st_in_hexstring_2nd state.
        void in_hexstring_2nd(char);

        /// @brief Process character in st_inline_image state.
        void in_inline_image(char);

        /// @brief Process character in st_token_ready state (should not happen).
        void in_token_ready(char);

        /// @brief Process character in st_name_hex1 state.
        void in_name_hex1(char);

        /// @brief Process character in st_name_hex2 state.
        void in_name_hex2(char);

        /// @brief Process character in st_sign state.
        void in_sign(char);

        /// @brief Process character in st_decimal state.
        void in_decimal(char);

        /// @brief Process character in st_number state.
        void in_number(char);

        /// @brief Process character in st_real state.
        void in_real(char);

        /// @brief Reset the tokenizer to initial state.
        void reset();

        // Lexer state
        state_e state_;  ///< Current state machine state

        bool allow_eof_{false};         ///< If true, treat EOF as a token instead of an error
        bool include_ignorable_{false};  ///< If true, return whitespace and comment tokens

        // Current token accumulation
        QPDFTokenizer::token_type_e type_;  ///< Type of the current token
        std::string val_;                   ///< Decoded value (for strings and names)
        std::string raw_val_;               ///< Raw value as it appears in input
        std::string error_message_;         ///< Error message if token is bad
        bool before_token_;                 ///< True if in ignorable content before a token
        bool in_token_;                     ///< True if currently reading a token
        char char_to_unread_;               ///< Character that should be unread (push mode)
        size_t inline_image_bytes_;         ///< Expected byte count for inline image
        bool bad_;                          ///< True if the current token is malformed

        // State for strings
        int string_depth_;   ///< Nesting depth of parentheses in literal strings
        int char_code_;      ///< Accumulated octal character code in string escapes
        char hex_char_;      ///< First hex digit in hexadecimal string
        int digit_count_;    ///< Number of octal digits read in character code
    };

} // namespace qpdf::impl

namespace qpdf
{
    using Tokenizer = qpdf::impl::Tokenizer;
}

#endif // QPDFTOKENIZER_PRIVATE_HH
