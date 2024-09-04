#include <qpdf/qpdf-config.h> // include first for large file support

#include <qpdf/QPDF_private.hh>

#include <array>
#include <atomic>
#include <cstring>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

#include <qpdf/BufferInputSource.hh>
#include <qpdf/FileInputSource.hh>
#include <qpdf/OffsetInputSource.hh>
#include <qpdf/Pipeline.hh>
#include <qpdf/QPDFExc.hh>
#include <qpdf/QPDFLogger.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFParser.hh>
#include <qpdf/QPDF_Array.hh>
#include <qpdf/QPDF_Dictionary.hh>
#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Reserved.hh>
#include <qpdf/QPDF_Stream.hh>
#include <qpdf/QPDF_Unresolved.hh>
#include <qpdf/QTC.hh>
#include <qpdf/QUtil.hh>

// This must be a fixed value. This API returns a const reference to it, and the C API relies on its
// being static as well.
std::string const QPDF::qpdf_version(QPDF_VERSION);

namespace
{
    class InvalidInputSource final: public InputSource
    {
      public:
        InvalidInputSource(std::string const& name) :
            name(name)
        {
        }
        ~InvalidInputSource() final = default;
        qpdf_offset_t
        findAndSkipNextEOL() final
        {
            throwException();
            return 0;
        }
        std::string const&
        getName() const final
        {
            return name;
        }
        qpdf_offset_t
        tell() final
        {
            throwException();
            return 0;
        }
        void
        seek(qpdf_offset_t offset, int whence) final
        {
            throwException();
        }
        void
        rewind() final
        {
            throwException();
        }
        size_t
        read(char* buffer, size_t length) final
        {
            throwException();
            return 0;
        }
        void
        unreadCh(char ch) final
        {
            throwException();
        }

      private:
        void
        throwException()
        {
            throw std::logic_error("QPDF operation attempted on a QPDF object with no input "
                                   "source. QPDF operations are invalid before processFile (or "
                                   "another process method) or after closeInputSource");
        }

        std::string const& name;
    };
} // namespace

QPDF::ForeignStreamData::ForeignStreamData(
    std::shared_ptr<EncryptionParameters> encp,
    std::shared_ptr<InputSource> file,
    QPDFObjGen const& foreign_og,
    qpdf_offset_t offset,
    size_t length,
    QPDFObjectHandle local_dict) :
    encp(encp),
    file(file),
    foreign_og(foreign_og),
    offset(offset),
    length(length),
    local_dict(local_dict)
{
}

QPDF::CopiedStreamDataProvider::CopiedStreamDataProvider(QPDF& destination_qpdf) :
    QPDFObjectHandle::StreamDataProvider(true),
    destination_qpdf(destination_qpdf)
{
}

bool
QPDF::CopiedStreamDataProvider::provideStreamData(
    QPDFObjGen const& og, Pipeline* pipeline, bool suppress_warnings, bool will_retry)
{
    std::shared_ptr<ForeignStreamData> foreign_data = foreign_stream_data[og];
    bool result = false;
    if (foreign_data.get()) {
        result = destination_qpdf.pipeForeignStreamData(
            foreign_data, pipeline, suppress_warnings, will_retry);
        QTC::TC("qpdf", "QPDF copy foreign with data", result ? 0 : 1);
    } else {
        auto foreign_stream = foreign_streams[og];
        result = foreign_stream.pipeStreamData(
            pipeline, nullptr, 0, qpdf_dl_none, suppress_warnings, will_retry);
        QTC::TC("qpdf", "QPDF copy foreign with foreign_stream", result ? 0 : 1);
    }
    return result;
}

void
QPDF::CopiedStreamDataProvider::registerForeignStream(
    QPDFObjGen const& local_og, QPDFObjectHandle foreign_stream)
{
    this->foreign_streams[local_og] = foreign_stream;
}

void
QPDF::CopiedStreamDataProvider::registerForeignStream(
    QPDFObjGen const& local_og, std::shared_ptr<ForeignStreamData> foreign_stream)
{
    this->foreign_stream_data[local_og] = foreign_stream;
}

QPDF::StringDecrypter::StringDecrypter(QPDF* qpdf, QPDFObjGen const& og) :
    qpdf(qpdf),
    og(og)
{
}

void
QPDF::StringDecrypter::decryptString(std::string& val)
{
    qpdf->decryptString(val, og);
}

std::string const&
QPDF::QPDFVersion()
{
    // The C API relies on this being a static value.
    return QPDF::qpdf_version;
}

QPDF::EncryptionParameters::EncryptionParameters() :
    encrypted(false),
    encryption_initialized(false),
    encryption_V(0),
    encryption_R(0),
    encrypt_metadata(true),
    cf_stream(e_none),
    cf_string(e_none),
    cf_file(e_none),
    user_password_matched(false),
    owner_password_matched(false)
{
}

QPDF::Members::Members(QPDF& qpdf) :
    log(QPDFLogger::defaultLogger()),
    file_sp(new InvalidInputSource(no_input_name)),
    file(file_sp.get()),
    encp(new EncryptionParameters),
    objects(qpdf, file)
{
}

QPDF::QPDF() :
    m(new Members(*this))
{
    m->tokenizer.allowEOF();
    // Generate a unique ID. It just has to be unique among all QPDF objects allocated throughout
    // the lifetime of this running application.
    static std::atomic<unsigned long long> unique_id{0};
    m->unique_id = unique_id.fetch_add(1ULL);
}

QPDF::~QPDF() = default;

std::shared_ptr<QPDF>
QPDF::create()
{
    return std::make_shared<QPDF>();
}

void
QPDF::processFile(char const* filename, char const* password)
{
    auto* fi = new FileInputSource(filename);
    processInputSource(std::shared_ptr<InputSource>(fi), password);
}

void
QPDF::processFile(char const* description, FILE* filep, bool close_file, char const* password)
{
    auto* fi = new FileInputSource(description, filep, close_file);
    processInputSource(std::shared_ptr<InputSource>(fi), password);
}

void
QPDF::processMemoryFile(
    char const* description, char const* buf, size_t length, char const* password)
{
    processInputSource(
        std::shared_ptr<InputSource>(
            // line-break
            new BufferInputSource(
                description, new Buffer(QUtil::unsigned_char_pointer(buf), length), true)),
        password);
}

void
QPDF::processInputSource(std::shared_ptr<InputSource> source, char const* password)
{
    m->file_sp = source;
    m->file = source.get();
    parse(password);
}

void
QPDF::closeInputSource()
{
    m->no_input_name = "closed input source";
    m->file_sp = std::shared_ptr<InputSource>(new InvalidInputSource(m->no_input_name));
    m->file = m->file_sp.get();
}

void
QPDF::setPasswordIsHexKey(bool val)
{
    m->provided_password_is_hex_key = val;
}

void
QPDF::emptyPDF()
{
    m->pdf_version = "1.3";
    m->no_input_name = "empty PDF";
    m->objects.xref_table().initialize_empty();
}

void
QPDF::registerStreamFilter(
    std::string const& filter_name, std::function<std::shared_ptr<QPDFStreamFilter>()> factory)
{
    QPDF_Stream::registerStreamFilter(filter_name, factory);
}

void
QPDF::setIgnoreXRefStreams(bool val)
{
    m->objects.xref_table().ignore_streams(val);
}

std::shared_ptr<QPDFLogger>
QPDF::getLogger()
{
    return m->log;
}

void
QPDF::setLogger(std::shared_ptr<QPDFLogger> l)
{
    m->log = l;
}

void
QPDF::setOutputStreams(std::ostream* out, std::ostream* err)
{
    setLogger(QPDFLogger::create());
    m->log->setOutputStreams(out, err);
}

void
QPDF::setSuppressWarnings(bool val)
{
    m->suppress_warnings = val;
}

void
QPDF::setMaxWarnings(size_t val)
{
    m->max_warnings = val;
}

void
QPDF::setAttemptRecovery(bool val)
{
    m->attempt_recovery = val;
    m->objects.xref_table().attempt_recovery(val);
}

void
QPDF::setImmediateCopyFrom(bool val)
{
    m->immediate_copy_from = val;
}

std::vector<QPDFExc>
QPDF::getWarnings()
{
    std::vector<QPDFExc> result = m->warnings;
    m->warnings.clear();
    return result;
}

bool
QPDF::anyWarnings() const
{
    return !m->warnings.empty();
}

size_t
QPDF::numWarnings() const
{
    return m->warnings.size();
}

bool
QPDF::validatePDFVersion(char const*& p, std::string& version)
{
    bool valid = QUtil::is_digit(*p);
    if (valid) {
        while (QUtil::is_digit(*p)) {
            version.append(1, *p++);
        }
        if ((*p == '.') && QUtil::is_digit(*(p + 1))) {
            version.append(1, *p++);
            while (QUtil::is_digit(*p)) {
                version.append(1, *p++);
            }
        } else {
            valid = false;
        }
    }
    return valid;
}

bool
QPDF::findHeader()
{
    qpdf_offset_t global_offset = m->file->tell();
    std::string line = m->file->readLine(1024);
    char const* p = line.c_str();
    if (strncmp(p, "%PDF-", 5) != 0) {
        throw std::logic_error("findHeader is not looking at %PDF-");
    }
    p += 5;
    std::string version;
    // Note: The string returned by line.c_str() is always null-terminated. The code below never
    // overruns the buffer because a null character always short-circuits further advancement.
    bool valid = validatePDFVersion(p, version);
    if (valid) {
        m->pdf_version = version;
        if (global_offset != 0) {
            // Empirical evidence strongly suggests that when there is leading material prior to the
            // PDF header, all explicit offsets in the file are such that 0 points to the beginning
            // of the header.
            QTC::TC("qpdf", "QPDF global offset");
            m->file_sp =
                std::shared_ptr<InputSource>(new OffsetInputSource(m->file_sp, global_offset));
            m->file = m->file_sp.get();
        }
    }
    return valid;
}

bool
QPDF::findStartxref()
{
    if (readToken(*m->file).isWord("startxref") && readToken(*m->file).isInteger()) {
        // Position in front of offset token
        m->file->seek(m->file->getLastOffset(), SEEK_SET);
        return true;
    }
    return false;
}

void
QPDF::parse(char const* password)
{
    if (password) {
        m->encp->provided_password = password;
    }

    // Find the header anywhere in the first 1024 bytes of the file.
    PatternFinder hf(*this, &QPDF::findHeader);
    if (!m->file->findFirst("%PDF-", 0, 1024, hf)) {
        QTC::TC("qpdf", "QPDF not a pdf file");
        warn(damagedPDF("", 0, "can't find PDF header"));
        // QPDFWriter writes files that usually require at least version 1.2 for /FlateDecode
        m->pdf_version = "1.2";
    }

    m->objects.xref_table().initialize();
    initializeEncryption();
    if (m->objects.xref_table().size() > 0 && !getRoot().getKey("/Pages").isDictionary()) {
        // QPDFs created from JSON have an empty xref table and no root object yet.
        throw damagedPDF("", 0, "unable to find page tree");
    }
}

void
QPDF::inParse(bool v)
{
    if (m->in_parse == v) {
        // This happens if QPDFParser::parse tries to resolve an indirect object while it is
        // parsing.
        throw std::logic_error("QPDF: re-entrant parsing detected. This is a qpdf bug."
                               " Please report at https://github.com/qpdf/qpdf/issues.");
    }
    m->in_parse = v;
}

void
QPDF::warn(QPDFExc const& e)
{
    if (m->max_warnings > 0 && m->warnings.size() >= m->max_warnings) {
        stopOnError("Too many warnings - file is too badly damaged");
    }
    m->warnings.push_back(e);
    if (!m->suppress_warnings) {
        *m->log->getWarn() << "WARNING: " << m->warnings.back().what() << "\n";
    }
}

void
QPDF::warn(
    qpdf_error_code_e error_code,
    std::string const& object,
    qpdf_offset_t offset,
    std::string const& message)
{
    warn(QPDFExc(error_code, getFilename(), object, offset, message));
}

void
QPDF::Objects::Xref_table::initialize_empty()
{
    initialized_ = true;
    trailer_ = QPDFObjectHandle::newDictionary();
    auto rt = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    auto pgs = qpdf.makeIndirectObject(QPDFObjectHandle::newDictionary());
    pgs.replaceKey("/Type", QPDFObjectHandle::newName("/Pages"));
    pgs.replaceKey("/Kids", QPDFObjectHandle::newArray());
    pgs.replaceKey("/Count", QPDFObjectHandle::newInteger(0));
    rt.replaceKey("/Type", QPDFObjectHandle::newName("/Catalog"));
    rt.replaceKey("/Pages", pgs);
    trailer_.replaceKey("/Root", rt);
    trailer_.replaceKey("/Size", QPDFObjectHandle::newInteger(3));
}

void
QPDF::Objects::Xref_table::initialize_json()
{
    initialized_ = true;
    table.resize(1);
    trailer_ = QPDFObjectHandle::newDictionary();
    trailer_.replaceKey("/Size", QPDFObjectHandle::newInteger(1));
}

void
QPDF::Objects::Xref_table::initialize()
{
    // PDF spec says %%EOF must be found within the last 1024 bytes of/ the file.  We add an extra
    // 30 characters to leave room for the startxref stuff.
    file->seek(0, SEEK_END);
    qpdf_offset_t end_offset = file->tell();
    // Sanity check on object ids. All objects must appear in xref table / stream. In all realistic
    // scenarios at least 3 bytes are required.
    if (max_id_ > end_offset / 3) {
        max_id_ = static_cast<int>(end_offset / 3);
    }
    qpdf_offset_t start_offset = (end_offset > 1054 ? end_offset - 1054 : 0);
    PatternFinder sf(qpdf, &QPDF::findStartxref);
    qpdf_offset_t xref_offset = 0;
    if (file->findLast("startxref", start_offset, 0, sf)) {
        xref_offset = QUtil::string_to_ll(read_token().getValue().c_str());
    }

    try {
        if (xref_offset == 0) {
            QTC::TC("qpdf", "QPDF can't find startxref");
            throw damaged_pdf("can't find startxref");
        }
        try {
            read(xref_offset);
        } catch (QPDFExc&) {
            throw;
        } catch (std::exception& e) {
            throw damaged_pdf(std::string("error reading xref: ") + e.what());
        }
    } catch (QPDFExc& e) {
        if (attempt_recovery_) {
            reconstruct(e);
            QTC::TC("qpdf", "QPDF reconstructed xref table");
        } else {
            throw;
        }
    }

    initialized_ = true;
}

void
QPDF::Objects::Xref_table::reconstruct(QPDFExc& e)
{
    if (reconstructed_) {
        // Avoid xref reconstruction infinite loops. This is getting very hard to reproduce because
        // qpdf is throwing many fewer exceptions while parsing. Most situations are warnings now.
        throw e;
    }

    // If recovery generates more than 1000 warnings, the file is so severely damaged that there
    // probably is no point trying to continue.
    const auto max_warnings = qpdf.m->warnings.size() + 1000U;
    auto check_warnings = [this, max_warnings]() {
        if (qpdf.m->warnings.size() > max_warnings) {
            throw damaged_pdf("too many errors while reconstructing cross-reference table");
        }
    };

    reconstructed_ = true;
    // We may find more objects, which may contain dangling references.

    warn_damaged("file is damaged");
    qpdf.warn(e);
    warn_damaged("Attempting to reconstruct cross-reference table");

    // Delete all references to type 1 (uncompressed) objects
    for (auto& iter: table) {
        if (iter.type() == 1) {
            iter = {};
        }
    }

    std::vector<std::tuple<int, int, qpdf_offset_t>> objects;
    std::vector<qpdf_offset_t> trailers;
    int max_found = 0;

    file->seek(0, SEEK_END);
    qpdf_offset_t eof = file->tell();
    file->seek(0, SEEK_SET);
    // Don't allow very long tokens here during recovery. All the interesting tokens are covered.
    static size_t const MAX_LEN = 10;
    while (file->tell() < eof) {
        QPDFTokenizer::Token t1 = read_token(MAX_LEN);
        qpdf_offset_t token_start = file->tell() - toO(t1.getValue().length());
        if (t1.isInteger()) {
            auto pos = file->tell();
            QPDFTokenizer::Token t2 = read_token(MAX_LEN);
            if (t2.isInteger() && read_token(MAX_LEN).isWord("obj")) {
                int obj = QUtil::string_to_int(t1.getValue().c_str());
                int gen = QUtil::string_to_int(t2.getValue().c_str());
                if (obj <= max_id_) {
                    objects.emplace_back(obj, gen, token_start);
                    if (obj > max_found) {
                        max_found = obj;
                    }
                } else {
                    warn_damaged("ignoring object with impossibly large id " + std::to_string(obj));
                }
            }
            file->seek(pos, SEEK_SET);
        } else if (!trailer_ && t1.isWord("trailer")) {
            trailers.emplace_back(file->tell());
        }
        file->findAndSkipNextEOL();
    }

    table.resize(toS(max_found) + 1);

    for (auto tr: trailers) {
        file->seek(tr, SEEK_SET);
        auto t = read_trailer();
        if (!t.isDictionary()) {
            // Oh well.  It was worth a try.
        } else {
            trailer_ = t;
            break;
        }
        check_warnings();
    }

    auto rend = objects.rend();
    for (auto it = objects.rbegin(); it != rend; it++) {
        auto [obj, gen, token_start] = *it;
        insert(obj, 1, token_start, gen);
        check_warnings();
    }

    if (!trailer_) {
        qpdf_offset_t max_offset{0};
        // If there are any xref streams, take the last one to appear.
        int i = -1;
        for (auto const& item: table) {
            ++i;
            if (item.type() != 1) {
                continue;
            }
            auto oh = qpdf.getObject(i, item.gen());
            try {
                if (!oh.isStreamOfType("/XRef")) {
                    continue;
                }
            } catch (std::exception&) {
                continue;
            }
            auto offset = item.offset();
            if (offset > max_offset) {
                max_offset = offset;
                trailer_ = oh.getDict();
            }
            check_warnings();
        }
        if (max_offset > 0) {
            try {
                read(max_offset);
            } catch (std::exception&) {
                throw damaged_pdf(
                    "error decoding candidate xref stream while recovering damaged file");
            }
            QTC::TC("qpdf", "QPDF recover xref stream");
        }
    }

    if (!trailer_) {
        // We could check the last encountered object to see if it was an xref stream.  If so, we
        // could try to get the trailer from there.  This may make it possible to recover files with
        // bad startxref pointers even when they have object streams.

        throw damaged_pdf("unable to find trailer dictionary while recovering damaged file");
    }
    if (table.empty()) {
        // We cannot check for an empty xref table in parse because empty tables are valid when
        // creating QPDF objects from JSON.
        throw damaged_pdf("unable to find objects while recovering damaged file");
    }
    check_warnings();
    if (!initialized_) {
        initialized_ = true;
        qpdf.getAllPages();
        check_warnings();
        if (qpdf.m->all_pages.empty()) {
            initialized_ = false;
            throw damaged_pdf("unable to find any pages while recovering damaged file");
        }
    }
    // We could iterate through the objects looking for streams and try to find objects inside of
    // them, but it's probably not worth the trouble.  Acrobat can't recover files with any errors
    // in an xref stream, and this would be a real long shot anyway.  If we wanted to do anything
    // that involved looking at stream contents, we'd also have to call initializeEncryption() here.
    // It's safe to call it more than once.
}

void
QPDF::Objects::Xref_table::read(qpdf_offset_t xref_offset)
{
    std::map<int, int> free_table;
    std::set<qpdf_offset_t> visited;
    while (xref_offset) {
        visited.insert(xref_offset);
        char buf[7];
        memset(buf, 0, sizeof(buf));
        file->seek(xref_offset, SEEK_SET);
        // Some files miss the mark a little with startxref. We could do a better job of searching
        // in the neighborhood for something that looks like either an xref table or stream, but the
        // simple heuristic of skipping whitespace can help with the xref table case and is harmless
        // with the stream case.
        bool done = false;
        bool skipped_space = false;
        while (!done) {
            char ch;
            if (1 == file->read(&ch, 1)) {
                if (QUtil::is_space(ch)) {
                    skipped_space = true;
                } else {
                    file->unreadCh(ch);
                    done = true;
                }
            } else {
                QTC::TC("qpdf", "QPDF eof skipping spaces before xref", skipped_space ? 0 : 1);
                done = true;
            }
        }

        file->read(buf, sizeof(buf) - 1);
        // The PDF spec says xref must be followed by a line terminator, but files exist in the wild
        // where it is terminated by arbitrary whitespace.
        if ((strncmp(buf, "xref", 4) == 0) && QUtil::is_space(buf[4])) {
            if (skipped_space) {
                QTC::TC("qpdf", "QPDF xref skipped space");
                warn_damaged("extraneous whitespace seen before xref");
            }
            QTC::TC(
                "qpdf",
                "QPDF xref space",
                ((buf[4] == '\n')       ? 0
                     : (buf[4] == '\r') ? 1
                     : (buf[4] == ' ')  ? 2
                                        : 9999));
            int skip = 4;
            // buf is null-terminated, and QUtil::is_space('\0') is false, so this won't overrun.
            while (QUtil::is_space(buf[skip])) {
                ++skip;
            }
            xref_offset = process_section(xref_offset + skip);
        } else {
            xref_offset = read_stream(xref_offset);
        }
        if (visited.count(xref_offset) != 0) {
            QTC::TC("qpdf", "QPDF xref loop");
            throw damaged_pdf("loop detected following xref tables");
        }
    }

    if (!trailer_) {
        throw damaged_pdf("unable to find trailer while reading xref");
    }
    int size = trailer_.getKey("/Size").getIntValueAsInt();

    if (size < 3) {
        throw damaged_pdf("too few objects - file can't have a page tree");
    }

    // We are no longer reporting what the highest id in the xref table is. I don't think it adds
    // anything. If we want to report more detail, we should report the total number of missing
    // entries, including missing entries before the last actual entry.
}

QPDF::Objects::Xref_table::Subsection
QPDF::Objects::Xref_table::subsection(std::string const& line)
{
    auto terminate = [this]() -> void {
        QTC::TC("qpdf", "QPDF invalid xref");
        throw damaged_table("xref syntax invalid");
    };

    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.
    char const* p = line.c_str();
    char const* start = line.c_str();

    // Skip zero or more spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        terminate();
    }
    // Gather digits
    std::string obj_str;
    while (QUtil::is_digit(*p)) {
        obj_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        terminate();
    }
    // Skip spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        terminate();
    }
    // Gather digits
    std::string num_str;
    while (QUtil::is_digit(*p)) {
        num_str.append(1, *p++);
    }
    // Skip any space including line terminators
    while (QUtil::is_space(*p)) {
        ++p;
    }
    auto obj = QUtil::string_to_int(obj_str.c_str());
    auto count = QUtil::string_to_int(num_str.c_str());
    if (obj > max_id() || count > max_id() || (obj + count) > max_id()) {
        throw damaged_table("xref table subsection header contains impossibly large entry");
    }
    return {obj, count, file->getLastOffset() + toI(p - start)};
}

std::vector<QPDF::Objects::Xref_table::Subsection>
QPDF::Objects::Xref_table::bad_subsections(std::string& line, qpdf_offset_t start)
{
    std::vector<QPDF::Objects::Xref_table::Subsection> result;
    file->seek(start, SEEK_SET);

    while (true) {
        line.assign(50, '\0');
        file->read(line.data(), line.size());
        auto [obj, num, offset] = result.emplace_back(subsection(line));
        file->seek(offset, SEEK_SET);
        for (qpdf_offset_t i = obj; i - num < obj; ++i) {
            if (!std::get<0>(read_entry())) {
                QTC::TC("qpdf", "QPDF invalid xref entry");
                throw damaged_table("invalid xref entry (obj=" + std::to_string(i) + ")");
            }
        }
        qpdf_offset_t pos = file->tell();
        if (read_token().isWord("trailer")) {
            return result;
        } else {
            file->seek(pos, SEEK_SET);
        }
    }
}

// Optimistically read and parse all subsection headers. If an error is encountered return the
// result of bad_subsections.
std::vector<QPDF::Objects::Xref_table::Subsection>
QPDF::Objects::Xref_table::subsections(std::string& line)
{
    auto recovery_offset = file->tell();
    try {
        std::vector<QPDF::Objects::Xref_table::Subsection> result;

        while (true) {
            line.assign(50, '\0');
            file->read(line.data(), line.size());
            auto& sub = result.emplace_back(subsection(line));
            auto count = std::get<1>(sub);
            auto offset = std::get<2>(sub);
            file->seek(offset + 20 * toO(count) - 1, SEEK_SET);
            file->read(line.data(), 1);
            if (!(line[0] == '\n' || line[0] == '\n')) {
                return bad_subsections(line, recovery_offset);
            }
            qpdf_offset_t pos = file->tell();
            if (read_token().isWord("trailer")) {
                return result;
            } else {
                file->seek(pos, SEEK_SET);
            }
        }
    } catch (...) {
        return bad_subsections(line, recovery_offset);
    }
}

// Returns (success, f1, f2, type).
std::tuple<bool, qpdf_offset_t, int, char>
QPDF::Objects::Xref_table::read_bad_entry()
{
    qpdf_offset_t f1{0};
    int f2{0};
    char type{'\0'};
    // Reposition after initial read attempt and reread.
    file->seek(file->getLastOffset(), SEEK_SET);
    auto line = file->readLine(30);

    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.
    char const* p = line.data();

    // Skip zero or more spaces. There aren't supposed to be any.
    bool invalid = false;
    while (QUtil::is_space(*p)) {
        ++p;
        QTC::TC("qpdf", "QPDF ignore first space in xref entry");
        invalid = true;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        return {false, 0, 0, '\0'};
    }
    // Gather digits
    std::string f1_str;
    while (QUtil::is_digit(*p)) {
        f1_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        return {false, 0, 0, '\0'};
    }
    if (QUtil::is_space(*(p + 1))) {
        QTC::TC("qpdf", "QPDF ignore first extra space in xref entry");
        invalid = true;
    }
    // Skip spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    // Require digit
    if (!QUtil::is_digit(*p)) {
        return {false, 0, 0, '\0'};
    }
    // Gather digits
    std::string f2_str;
    while (QUtil::is_digit(*p)) {
        f2_str.append(1, *p++);
    }
    // Require space
    if (!QUtil::is_space(*p)) {
        return {false, 0, 0, '\0'};
    }
    if (QUtil::is_space(*(p + 1))) {
        QTC::TC("qpdf", "QPDF ignore second extra space in xref entry");
        invalid = true;
    }
    // Skip spaces
    while (QUtil::is_space(*p)) {
        ++p;
    }
    if ((*p == 'f') || (*p == 'n')) {
        type = *p;
    } else {
        return {false, 0, 0, '\0'};
    }
    if ((f1_str.length() != 10) || (f2_str.length() != 5)) {
        QTC::TC("qpdf", "QPDF ignore length error xref entry");
        invalid = true;
    }

    if (invalid) {
        qpdf.warn(damaged_table("accepting invalid xref table entry"));
    }

    f1 = QUtil::string_to_ll(f1_str.c_str());
    f2 = QUtil::string_to_int(f2_str.c_str());

    return {true, f1, f2, type};
}

// Optimistically read and parse xref entry. If entry is bad, call read_bad_xrefEntry and return
// result. Returns (success, f1, f2, type).
std::tuple<bool, qpdf_offset_t, int, char>
QPDF::Objects::Xref_table::read_entry()
{
    qpdf_offset_t f1{0};
    int f2{0};
    char type{'\0'};
    std::array<char, 21> line;
    if (file->read(line.data(), 20) != 20) {
        // C++20: [[unlikely]]
        return {false, 0, 0, '\0'};
    }
    line[20] = '\0';
    char const* p = line.data();

    int f1_len = 0;
    int f2_len = 0;

    // is_space and is_digit both return false on '\0', so this will not overrun the null-terminated
    // buffer.

    // Gather f1 digits. NB No risk of overflow as 9'999'999'999 < max long long.
    while (*p == '0') {
        ++f1_len;
        ++p;
    }
    while (QUtil::is_digit(*p) && f1_len++ < 10) {
        f1 *= 10;
        f1 += *p++ - '0';
    }
    // Require space
    if (!QUtil::is_space(*p++)) {
        // Entry doesn't start with space or digit.
        // C++20: [[unlikely]]
        return {false, 0, 0, '\0'};
    }
    // Gather digits. NB No risk of overflow as 99'999 < max int.
    while (*p == '0') {
        ++f2_len;
        ++p;
    }
    while (QUtil::is_digit(*p) && f2_len++ < 5) {
        f2 *= 10;
        f2 += static_cast<int>(*p++ - '0');
    }
    if (QUtil::is_space(*p++) && (*p == 'f' || *p == 'n')) {
        // C++20: [[likely]]
        type = *p;
        // No test for valid line[19].
        if (*(++p) && *(++p) && (*p == '\n' || *p == '\r') && f1_len == 10 && f2_len == 5) {
            // C++20: [[likely]]
            return {true, f1, f2, type};
        }
    }
    return read_bad_entry();
}

// Read a single cross-reference table section and associated trailer.
qpdf_offset_t
QPDF::Objects::Xref_table::process_section(qpdf_offset_t xref_offset)
{
    file->seek(xref_offset, SEEK_SET);
    std::string line;
    auto subs = subsections(line);

    auto cur_trailer_offset = file->tell();
    auto cur_trailer = read_trailer();
    if (!cur_trailer.isDictionary()) {
        QTC::TC("qpdf", "QPDF missing trailer");
        throw qpdf.damagedPDF("", "expected trailer dictionary");
    }

    if (!trailer_) {
        unsigned int sz;
        trailer_ = cur_trailer;

        if (!trailer_.hasKey("/Size")) {
            QTC::TC("qpdf", "QPDF trailer lacks size");
            throw qpdf.damagedPDF("trailer", "trailer dictionary lacks /Size key");
        }
        if (!trailer_.getKey("/Size").getValueAsUInt(sz)) {
            QTC::TC("qpdf", "QPDF trailer size not integer");
            throw qpdf.damagedPDF("trailer", "/Size key in trailer dictionary is not an integer");
        }
        if (sz >= static_cast<unsigned int>(max_id_)) {
            QTC::TC("qpdf", "QPDF trailer size impossibly large");
            throw qpdf.damagedPDF("trailer", "/Size key in trailer dictionary is impossibly large");
        }
        table.resize(sz);
    }

    for (auto [obj, num, offset]: subs) {
        file->seek(offset, SEEK_SET);
        for (qpdf_offset_t i = obj; i - num < obj; ++i) {
            if (i == 0) {
                // This is needed by checkLinearization()
                first_item_offset_ = file->tell();
            }
            // For xref_table, these will always be small enough to be ints
            auto [success, f1, f2, type] = read_entry();
            if (!success) {
                throw damaged_table("invalid xref entry (obj=" + std::to_string(i) + ")");
            }
            if (type == 'f') {
                insert_free(QPDFObjGen(toI(i), f2));
            } else {
                insert(toI(i), 1, f1, f2);
            }
        }
        qpdf_offset_t pos = file->tell();
        if (read_token().isWord("trailer")) {
            break;
        } else {
            file->seek(pos, SEEK_SET);
        }
    }

    if (cur_trailer.hasKey("/XRefStm")) {
        if (ignore_streams_) {
            QTC::TC("qpdf", "QPDF ignoring XRefStm in trailer");
        } else {
            if (cur_trailer.getKey("/XRefStm").isInteger()) {
                // Read the xref stream but disregard any return value -- we'll use our trailer's
                // /Prev key instead of the xref stream's.
                (void)read_stream(cur_trailer.getKey("/XRefStm").getIntValue());
            } else {
                throw qpdf.damagedPDF("xref stream", cur_trailer_offset, "invalid /XRefStm");
            }
        }
    }

    if (cur_trailer.hasKey("/Prev")) {
        if (!cur_trailer.getKey("/Prev").isInteger()) {
            QTC::TC("qpdf", "QPDF trailer prev not integer");
            throw qpdf.damagedPDF(
                "trailer", cur_trailer_offset, "/Prev key in trailer dictionary is not an integer");
        }
        QTC::TC("qpdf", "QPDF prev key in trailer dictionary");
        return cur_trailer.getKey("/Prev").getIntValue();
    }

    return 0;
}

// Read a single cross-reference stream.
qpdf_offset_t
QPDF::Objects::Xref_table::read_stream(qpdf_offset_t xref_offset)
{
    if (!ignore_streams_) {
        QPDFObjGen x_og;
        QPDFObjectHandle xref_obj;
        try {
            xref_obj =
                objects.read(false, xref_offset, "xref stream", QPDFObjGen(0, 0), x_og, true);
        } catch (QPDFExc&) {
            // ignore -- report error below
        }
        if (xref_obj.isStreamOfType("/XRef")) {
            QTC::TC("qpdf", "QPDF found xref stream");
            return process_stream(xref_offset, xref_obj);
        }
    }

    QTC::TC("qpdf", "QPDF can't find xref");
    throw qpdf.damagedPDF("", xref_offset, "xref not found");
    return 0; // unreachable
}

// Return the entry size of the xref stream and the processed W array.
std::pair<int, std::array<int, 3>>
QPDF::Objects::Xref_table::process_W(
    QPDFObjectHandle& dict, std::function<QPDFExc(std::string_view)> damaged)
{
    auto W_obj = dict.getKey("/W");
    if (!(W_obj.isArray() && W_obj.getArrayNItems() >= 3 && W_obj.getArrayItem(0).isInteger() &&
          W_obj.getArrayItem(1).isInteger() && W_obj.getArrayItem(2).isInteger())) {
        throw damaged("Cross-reference stream does not have a proper /W key");
    }

    std::array<int, 3> W;
    int entry_size = 0;
    auto w_vector = W_obj.getArrayAsVector();
    int max_bytes = sizeof(qpdf_offset_t);
    for (size_t i = 0; i < 3; ++i) {
        W[i] = w_vector[i].getIntValueAsInt();
        if (W[i] > max_bytes) {
            throw damaged("Cross-reference stream's /W contains impossibly large values");
        }
        if (W[i] < 0) {
            throw damaged("Cross-reference stream's /W contains negative values");
        }
        entry_size += W[i];
    }
    if (entry_size == 0) {
        throw damaged("Cross-reference stream's /W indicates entry size of 0");
    }
    return {entry_size, W};
}

// Validate Size entry and return the maximum number of entries that the xref stream can contain and
// the value of the Size entry.
std::pair<int, size_t>
QPDF::Objects::Xref_table::process_Size(
    QPDFObjectHandle& dict, int entry_size, std::function<QPDFExc(std::string_view)> damaged)
{
    // Number of entries is limited by the highest possible object id and stream size.
    auto max_num_entries = std::numeric_limits<int>::max();
    if (max_num_entries > (std::numeric_limits<qpdf_offset_t>::max() / entry_size)) {
        max_num_entries = toI(std::numeric_limits<qpdf_offset_t>::max() / entry_size);
    }

    auto Size_obj = dict.getKey("/Size");
    long long size;
    if (!dict.getKey("/Size").getValueAsInt(size)) {
        throw damaged("Cross-reference stream does not have a proper /Size key");
    } else if (size < 0) {
        throw damaged("Cross-reference stream has a negative /Size key");
    } else if (size >= max_num_entries) {
        throw damaged("Cross-reference stream has an impossibly large /Size key");
    }
    // We are not validating that Size <= (Size key of parent xref / trailer).
    return {max_num_entries, toS(size)};
}

// Return the number of entries of the xref stream and the processed Index array.
std::pair<int, std::vector<std::pair<int, int>>>
QPDF::Objects::Xref_table::process_Index(
    QPDFObjectHandle& dict, int max_num_entries, std::function<QPDFExc(std::string_view)> damaged)
{
    auto size = dict.getKey("/Size").getIntValueAsInt();
    auto Index_obj = dict.getKey("/Index");

    if (Index_obj.isArray()) {
        std::vector<std::pair<int, int>> indx;
        int num_entries = 0;
        auto index_vec = Index_obj.getArrayAsVector();
        if ((index_vec.size() % 2) || index_vec.size() < 2) {
            throw damaged("Cross-reference stream's /Index has an invalid number of values");
        }

        int i = 0;
        long long first = 0;
        for (auto& val: index_vec) {
            if (val.isInteger()) {
                if (i % 2) {
                    auto count = val.getIntValue();
                    if (count <= 0) {
                        throw damaged(
                            "Cross-reference stream section claims to contain " +
                            std::to_string(count) + " entries");
                    }
                    // We are guarding against the possibility of num_entries * entry_size
                    // overflowing. We are not checking that entries are in ascending order as
                    // required by the spec, which probably should generate a warning. We are also
                    // not checking that for each subsection first object number + number of entries
                    // <= /Size. The spec requires us to ignore object number > /Size.
                    if (first > (max_num_entries - count) ||
                        count > (max_num_entries - num_entries)) {
                        throw damaged(
                            "Cross-reference stream claims to contain too many entries: " +
                            std::to_string(first) + " " + std::to_string(max_num_entries) + " " +
                            std::to_string(num_entries));
                    }
                    indx.emplace_back(static_cast<int>(first), static_cast<int>(count));
                    num_entries += static_cast<int>(count);
                } else {
                    first = val.getIntValue();
                    if (first < 0) {
                        throw damaged(
                            "Cross-reference stream's /Index contains a negative object id");
                    } else if (first > max_num_entries) {
                        throw damaged("Cross-reference stream's /Index contains an impossibly "
                                      "large object id");
                    }
                }
            } else {
                throw damaged(
                    "Cross-reference stream's /Index's item " + std::to_string(i) +
                    " is not an integer");
            }
            i++;
        }
        QTC::TC("qpdf", "QPDF xref /Index is array", index_vec.size() == 2 ? 0 : 1);
        return {num_entries, indx};
    } else if (Index_obj.isNull()) {
        QTC::TC("qpdf", "QPDF xref /Index is null");
        return {size, {{0, size}}};
    } else {
        throw damaged("Cross-reference stream does not have a proper /Index key");
    }
}

qpdf_offset_t
QPDF::Objects::Xref_table::process_stream(qpdf_offset_t xref_offset, QPDFObjectHandle& xref_obj)
{
    auto damaged = [this, xref_offset](std::string_view msg) -> QPDFExc {
        return qpdf.damagedPDF("xref stream", xref_offset, msg.data());
    };

    auto dict = xref_obj.getDict();

    auto [entry_size, W] = process_W(dict, damaged);
    auto [max_num_entries, size] = process_Size(dict, entry_size, damaged);
    auto [num_entries, indx] = process_Index(dict, max_num_entries, damaged);

    std::shared_ptr<Buffer> bp = xref_obj.getStreamData(qpdf_dl_specialized);
    size_t actual_size = bp->getSize();
    auto expected_size = toS(entry_size) * toS(num_entries);

    if (expected_size != actual_size) {
        QPDFExc x = damaged(
            "Cross-reference stream data has the wrong size; expected = " +
            std::to_string(expected_size) + "; actual = " + std::to_string(actual_size));
        if (expected_size > actual_size) {
            throw x;
        } else {
            qpdf.warn(x);
        }
    }

    if (!trailer_) {
        trailer_ = dict;
        table.resize(size);
    }

    bool saw_first_compressed_object = false;

    // Actual size vs. expected size check above ensures that we will not overflow any buffers here.
    // We know that entry_size * num_entries is less or equal to the size of the buffer.
    auto p = bp->getBuffer();
    for (auto [obj, sec_entries]: indx) {
        // Process a subsection.
        for (int i = 0; i < sec_entries; ++i) {
            // Read this entry
            std::array<qpdf_offset_t, 3> fields{};
            if (W[0] == 0) {
                QTC::TC("qpdf", "QPDF default for xref stream field 0");
                fields[0] = 1;
            }
            for (size_t j = 0; j < 3; ++j) {
                for (int k = 0; k < W[j]; ++k) {
                    fields[j] <<= 8;
                    fields[j] |= *p++;
                }
            }

            // Get the generation number.  The generation number is 0 unless this is an uncompressed
            // object record, in which case the generation number appears as the third field.
            if (saw_first_compressed_object) {
                if (fields[0] != 2) {
                    uncompressed_after_compressed_ = true;
                }
            } else if (fields[0] == 2) {
                saw_first_compressed_object = true;
            }
            if (obj == 0) {
                // This is needed by checkLinearization()
                first_item_offset_ = xref_offset;
            } else if (fields[0] == 0) {
                // Ignore fields[2], which we don't care about in this case. This works around the
                // issue of some PDF files that put invalid values, like -1, here for deleted
                // objects.
                insert_free(QPDFObjGen(obj, 0));
            } else {
                insert(obj, toI(fields[0]), fields[1], toI(fields[2]));
            }
            ++obj;
        }
    }

    if (dict.hasKey("/Prev")) {
        if (!dict.getKey("/Prev").isInteger()) {
            throw qpdf.damagedPDF(
                "xref stream", "/Prev key in xref stream dictionary is not an integer");
        }
        QTC::TC("qpdf", "QPDF prev key in xref stream dictionary");
        return dict.getKey("/Prev").getIntValue();
    } else {
        return 0;
    }
}

void
QPDF::Objects::Xref_table::insert(int obj, int f0, qpdf_offset_t f1, int f2)
{
    // Populate the xref table in such a way that the first reference to an object that we see,
    // which is the one in the latest xref table in which it appears, is the one that gets stored.
    // This works because we are reading more recent appends before older ones.

    // If there is already an entry for this object and generation in the table, it means that a
    // later xref table has registered this object.  Disregard this one.

    int new_gen = f0 == 2 ? 0 : f2;

    if (!(obj > 0 && static_cast<size_t>(obj) < table.size() && 0 <= f2 && new_gen < 65535)) {
        // We are ignoring invalid objgens. Most will arrive here from xref reconstruction. There
        // is probably no point having another warning but we could count invalid items in order to
        // decide when to give up.
        QTC::TC("qpdf", "QPDF xref overwrite invalid objgen");
        return;
    }

    auto& entry = table[static_cast<size_t>(obj)];
    auto old_type = entry.type();

    if (!old_type && entry.gen() > 0) {
        // At the moment we are processing the updates last to first and therefore the gen doesn't
        // matter as long as it > 0 to distinguish it from an uninitialized entry. This will need
        // to be revisited when we want to support incremental updates or more comprhensive
        // checking.
        QTC::TC("qpdf", "QPDF xref deleted object");
        return;
    }

    if (f0 == 2 && static_cast<int>(f1) == obj) {
        qpdf.warn(qpdf.damagedPDF(
            "xref stream", "self-referential object stream " + std::to_string(obj)));
        return;
    }

    if (old_type && entry.gen() >= new_gen) {
        QTC::TC("qpdf", "QPDF xref reused object");
        return;
    }

    switch (f0) {
    case 1:
        // f2 is generation
        QTC::TC("qpdf", "QPDF xref gen > 0", (f2 > 0) ? 1 : 0);
        entry = {f2, Uncompressed(f1)};
        break;

    case 2:
        entry = {0, Compressed(toI(f1), f2)};
        object_streams_ = true;
        break;

    default:
        throw qpdf.damagedPDF(
            "xref stream", "unknown xref stream entry type " + std::to_string(f0));
        break;
    }
}

void
QPDF::Objects::Xref_table::insert_free(QPDFObjGen og)
{
    // At the moment we are processing the updates last to first and therefore the gen doesn't
    // matter as long as it > 0 to distinguish it from an uninitialized entry. This will need to be
    // revisited when we want to support incremental updates or more comprhensive checking.
    if (og.getObj() < 1) {
        return;
    }
    size_t id = static_cast<size_t>(og.getObj());
    if (id < table.size() && !type(id)) {
        table[id] = {1, {}};
    }
}

QPDFObjGen
QPDF::Objects::Xref_table::at_offset(qpdf_offset_t offset) const noexcept
{
    int id = 0;
    int gen = 0;
    qpdf_offset_t start = 0;

    int i = 0;
    for (auto const& item: table) {
        auto o = item.offset();
        if (start < o && o <= offset) {
            start = o;
            id = i;
            gen = item.gen();
        }
        ++i;
    }
    return QPDFObjGen(id, gen);
}

std::map<QPDFObjGen, QPDFXRefEntry>
QPDF::Objects::Xref_table::as_map() const
{
    std::map<QPDFObjGen, QPDFXRefEntry> result;
    int i{0};
    for (auto const& item: table) {
        switch (item.type()) {
        case 0:
            break;
        case 1:
            result.emplace(QPDFObjGen(i, item.gen()), item.offset());
            break;
        case 2:
            result.emplace(
                QPDFObjGen(i, 0), QPDFXRefEntry(item.stream_number(), item.stream_index()));
            break;
        default:
            throw std::logic_error("Xref_table: invalid entry type");
        }
        ++i;
    }
    return result;
}

void
QPDF::showXRefTable()
{
    m->objects.xref_table().show();
}

void
QPDF::Objects::Xref_table::show()
{
    auto& cout = *qpdf.m->log->getInfo();
    int i = -1;
    for (auto const& item: table) {
        ++i;
        if (item.type()) {
            cout << std::to_string(i) << "/" << std::to_string(item.gen()) << ": ";
            switch (item.type()) {
            case 1:
                cout << "uncompressed; offset = " << item.offset() << "\n";
                break;

            case 2:
                cout << "compressed; stream = " << item.stream_number()
                     << ", index = " << item.stream_index() << "\n";
                break;

            default:
                throw std::logic_error(
                    "unknown cross-reference table type while showing xref_table");
            }
        }
    }
}

void
QPDF::Objects::Xref_table::resolve()
{
    bool may_change = !reconstructed_;
    int i = -1;
    for (auto& item: table) {
        ++i;
        if (item.type()) {
            if (objects.unresolved(QPDFObjGen(i, item.gen()))) {
                objects.resolve(QPDFObjGen(i, item.gen()));
                if (may_change && reconstructed_) {
                    QTC::TC("qpdf", "QPDF fix dangling triggered xref reconstruction");
                    return resolve();
                }
            }
        }
    }
}

// Ensure all objects in the pdf file, including those in indirect references, appear in the object
// cache.
void
QPDF::fixDanglingReferences(bool force)
{
    (void)m->objects.next_id();
}

size_t
QPDF::getObjectCount()
{
    // This method returns the highest id in use.
    return toS(m->objects.next_id() - 1);
}

std::vector<QPDFObjectHandle>
QPDF::Objects::all()
{
    // After next_id is called, all objects are in the object cache.
    next_id();
    std::vector<QPDFObjectHandle> result;
    for (auto const& iter: table) {
        result.push_back({iter.second.object});
    }
    return result;
}

std::vector<QPDFObjectHandle>
QPDF::getAllObjects()
{
    return m->objects.all();
}

void
QPDF::setLastObjectDescription(std::string const& description, QPDFObjGen const& og)
{
    m->last_object_description.clear();
    if (!description.empty()) {
        m->last_object_description += description;
        if (og.isIndirect()) {
            m->last_object_description += ": ";
        }
    }
    if (og.isIndirect()) {
        m->last_object_description += "object " + og.unparse(' ');
    }
}

QPDFObjectHandle
QPDF::Objects::Xref_table::read_trailer()
{
    qpdf_offset_t offset = file->tell();
    bool empty = false;
    auto object =
        QPDFParser(*file, "trailer", objects.tokenizer, nullptr, &qpdf, true).parse(empty, false);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        qpdf.warn(qpdf.damagedPDF("trailer", "empty object treated as null"));
    } else if (object.isDictionary() && read_token().isWord("stream")) {
        qpdf.warn(qpdf.damagedPDF("trailer", file->tell(), "stream keyword found in trailer"));
    }
    // Override last_offset so that it points to the beginning of the object we just read
    file->setLastOffset(offset);
    return object;
}

QPDFObjectHandle
QPDF::Objects::read_object(QPDFObjGen og)
{
    qpdf_offset_t offset = file->tell();

    StringDecrypter decrypter{&qpdf, og};
    StringDecrypter* decrypter_ptr = qpdf.m->encp->encrypted ? &decrypter : nullptr;
    auto [object, empty] = parse(*file, qpdf.m->last_object_description, decrypter_ptr);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        qpdf.warn(qpdf.damagedPDF("empty object treated as null"));
        return object;
    }
    auto token = read_token();
    if (object.isDictionary() && token.isWord("stream")) {
        read_stream(object, og, offset);
        token = read_token();
    }
    if (!token.isWord("endobj")) {
        QTC::TC("qpdf", "QPDF err expected endobj");
        qpdf.warn(qpdf.damagedPDF("expected endobj"));
    }
    return object;
}

// After reading stream dictionary and stream keyword, read rest of stream.
void
QPDF::Objects::read_stream(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset)
{
    validate_stream_line_end(object, og, offset);

    // Must get offset before accessing any additional objects since resolving a previously
    // unresolved indirect object will change file position.
    qpdf_offset_t stream_offset = file->tell();
    size_t length = 0;

    try {
        auto length_obj = object.getKey("/Length");

        if (!length_obj.isInteger()) {
            if (length_obj.isNull()) {
                QTC::TC("qpdf", "QPDF stream without length");
                throw qpdf.damagedPDF(offset, "stream dictionary lacks /Length key");
            }
            QTC::TC("qpdf", "QPDF stream length not integer");
            throw qpdf.damagedPDF(offset, "/Length key in stream dictionary is not an integer");
        }

        length = toS(length_obj.getUIntValue());
        // Seek in two steps to avoid potential integer overflow
        file->seek(stream_offset, SEEK_SET);
        file->seek(toO(length), SEEK_CUR);
        if (!read_token().isWord("endstream")) {
            QTC::TC("qpdf", "QPDF missing endstream");
            throw qpdf.damagedPDF("expected endstream");
        }
    } catch (QPDFExc& e) {
        if (qpdf.m->attempt_recovery) {
            qpdf.warn(e);
            length = recover_stream_length(og, stream_offset);
        } else {
            throw;
        }
    }
    object = {QPDF_Stream::create(&qpdf, og, object, stream_offset, length)};
}

void
QPDF::Objects::validate_stream_line_end(
    QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset)
{
    // The PDF specification states that the word "stream" should be followed by either a carriage
    // return and a newline or by a newline alone.  It specifically disallowed following it by a
    // carriage return alone since, in that case, there would be no way to tell whether the NL in a
    // CR NL sequence was part of the stream data.  However, some readers, including Adobe reader,
    // accept a carriage return by itself when followed by a non-newline character, so that's what
    // we do here. We have also seen files that have extraneous whitespace between the stream
    // keyword and the newline.
    while (true) {
        char ch;
        if (file->read(&ch, 1) == 0) {
            // A premature EOF here will result in some other problem that will get reported at
            // another time.
            return;
        }
        if (ch == '\n') {
            // ready to read stream data
            QTC::TC("qpdf", "QPDF stream with NL only");
            return;
        }
        if (ch == '\r') {
            // Read another character
            if (file->read(&ch, 1) != 0) {
                if (ch == '\n') {
                    // Ready to read stream data
                    QTC::TC("qpdf", "QPDF stream with CRNL");
                } else {
                    // Treat the \r by itself as the whitespace after endstream and start reading
                    // stream data in spite of not having seen a newline.
                    QTC::TC("qpdf", "QPDF stream with CR only");
                    file->unreadCh(ch);
                    qpdf.warn(qpdf.damagedPDF(
                        file->tell(), "stream keyword followed by carriage return only"));
                }
            }
            return;
        }
        if (!QUtil::is_space(ch)) {
            QTC::TC("qpdf", "QPDF stream without newline");
            file->unreadCh(ch);
            qpdf.warn(qpdf.damagedPDF(
                file->tell(), "stream keyword not followed by proper line terminator"));
            return;
        }
        qpdf.warn(
            qpdf.damagedPDF(file->tell(), "stream keyword followed by extraneous whitespace"));
    }
}

QPDFObjectHandle
QPDF::Objects::Xref_table::read_compressed_object(InputSource& input, int obj)
{
    qpdf.m->last_object_description.erase(7); // last_object_description starts with "object "
    qpdf.m->last_object_description += std::to_string(obj);
    qpdf.m->last_object_description += " 0";

    auto [object, empty] = objects.parse(input, qpdf.m->last_object_description);
    if (empty) {
        // Nothing in the PDF spec appears to allow empty objects, but they have been encountered in
        // actual PDF files and Adobe Reader appears to ignore them.
        qpdf.warn(qpdf.damagedPDF(input, input.getLastOffset(), "empty object treated as null"));
    }
    return object;
}

bool
QPDF::findEndstream()
{
    // Find endstream or endobj. Position the input at that token.
    auto t = readToken(*m->file, 20);
    if (t.isWord("endobj") || t.isWord("endstream")) {
        m->file->seek(m->file->getLastOffset(), SEEK_SET);
        return true;
    }
    return false;
}

size_t
QPDF::Objects::recover_stream_length(QPDFObjGen og, qpdf_offset_t stream_offset)
{
    // Try to reconstruct stream length by looking for endstream or endobj
    qpdf.warn(qpdf.damagedPDF(stream_offset, "attempting to recover stream length"));

    QPDF::PatternFinder ef(qpdf, &QPDF::findEndstream);
    size_t length = 0;
    if (file->findFirst("end", stream_offset, 0, ef)) {
        length = toS(file->tell() - stream_offset);
        // Reread endstream but, if it was endobj, don't skip that.
        auto t = read_token();
        if (t.getValue() == "endobj") {
            file->seek(file->getLastOffset(), SEEK_SET);
        }
    }

    if (length) {
        // Make sure this is inside this object
        auto found = xref.at_offset(stream_offset + toO(length));
        if (found == QPDFObjGen() || found == og) {
            // If we are trying to recover an XRef stream the xref table may not contain any entries
            // or may be missing an entry for the stream, therefore we cannot check the found
            // length. Otherwise we found endstream\endobj within the space allowed for this object,
            // so we're probably in good shape.
        } else {
            QTC::TC("qpdf", "QPDF found wrong endstream in recovery");
            length = 0;
        }
    }

    if (length == 0) {
        qpdf.warn(qpdf.damagedPDF(
            stream_offset, "unable to recover stream data; treating stream as empty"));
    } else {
        qpdf.warn(
            qpdf.damagedPDF(stream_offset, "recovered stream length: " + std::to_string(length)));
    }

    QTC::TC("qpdf", "QPDF recovered stream length");
    return length;
}

QPDFTokenizer::Token
QPDF::readToken(InputSource& input, size_t max_len)
{
    return m->tokenizer.readToken(input, m->last_object_description, true, max_len);
}

std::pair<QPDFObjectHandle, bool>
QPDF::Objects::parse(
    InputSource& input,
    std::string const& description,
    QPDFObjectHandle::StringDecrypter* decrypter_ptr)
{
    bool empty = false;
    auto oh =
        QPDFParser(input, description, tokenizer, decrypter_ptr, &qpdf, true).parse(empty, false);
    return {oh, empty};
}

QPDFObjectHandle
QPDF::Objects::read(
    bool try_recovery,
    qpdf_offset_t offset,
    std::string const& description,
    QPDFObjGen exp_og,
    QPDFObjGen& og,
    bool skip_cache_if_in_xref)
{
    bool check_og = true;
    if (exp_og.getObj() == 0) {
        // This method uses an expect object ID of 0 to indicate that we don't know or don't care
        // what the actual object ID is at this offset. This is true when we read the xref stream
        // and linearization hint streams. In this case, we don't verify the expect object
        // ID/generation against what was read from the file. There is also no reason to attempt
        // xref recovery if we get a failure in this case since the read attempt was not triggered
        // by an xref lookup.
        check_og = false;
        try_recovery = false;
    }
    qpdf.setLastObjectDescription(description, exp_og);

    if (!qpdf.m->attempt_recovery) {
        try_recovery = false;
    }

    // Special case: if offset is 0, just return null.  Some PDF writers, in particular
    // "Mac OS X 10.7.5 Quartz PDFContext", may store deleted objects in the xref table as
    // "0000000000 00000 n", which is not correct, but it won't hurt anything for us to ignore
    // these.
    if (offset == 0) {
        QTC::TC("qpdf", "QPDF bogus 0 offset", 0);
        qpdf.warn(qpdf.damagedPDF(0, "object has offset 0"));
        return QPDFObjectHandle::newNull();
    }

    file->seek(offset, SEEK_SET);
    try {
        auto tobjid = read_token();
        bool objidok = tobjid.isInteger();
        QTC::TC("qpdf", "QPDF check objid", objidok ? 1 : 0);
        if (!objidok) {
            QTC::TC("qpdf", "QPDF expected n n obj");
            throw qpdf.damagedPDF(offset, "expected n n obj");
        }
        auto tgen = read_token();
        bool genok = tgen.isInteger();
        QTC::TC("qpdf", "QPDF check generation", genok ? 1 : 0);
        if (!genok) {
            throw qpdf.damagedPDF(offset, "expected n n obj");
        }
        auto tobj = read_token();

        bool objok = tobj.isWord("obj");
        QTC::TC("qpdf", "QPDF check obj", objok ? 1 : 0);

        if (!objok) {
            throw qpdf.damagedPDF(offset, "expected n n obj");
        }
        int objid = QUtil::string_to_int(tobjid.getValue().c_str());
        int generation = QUtil::string_to_int(tgen.getValue().c_str());
        og = QPDFObjGen(objid, generation);
        if (objid == 0) {
            QTC::TC("qpdf", "QPDF object id 0");
            throw qpdf.damagedPDF(offset, "object with ID 0");
        }
        if (check_og && (exp_og != og)) {
            QTC::TC("qpdf", "QPDF err wrong objid/generation");
            QPDFExc e = qpdf.damagedPDF(offset, "expected " + exp_og.unparse(' ') + " obj");
            if (try_recovery) {
                // Will be retried below
                throw e;
            } else {
                // We can try reading the object anyway even if the ID doesn't match.
                qpdf.warn(e);
            }
        }
    } catch (QPDFExc& e) {
        if (try_recovery) {
            // Try again after reconstructing xref table
            xref.reconstruct(e);
            if (xref.type(exp_og) == 1) {
                QTC::TC("qpdf", "QPDF recovered in readObjectAtOffset");
                return read(false, xref.offset(exp_og), description, exp_og, og, false);
            } else {
                QTC::TC("qpdf", "QPDF object gone after xref reconstruction");
                qpdf.warn(qpdf.damagedPDF(
                    "",
                    0,
                    ("object " + exp_og.unparse(' ') +
                     " not found in file after regenerating cross reference table")));
                return QPDFObjectHandle::newNull();
            }
        } else {
            throw;
        }
    }

    qpdf.setLastObjectDescription(description, og);
    auto oh = read_object(og);

    if (unresolved(og)) {
        // Store the object in the cache here so it gets cached whether we first know the offset or
        // whether we first know the object ID and generation (in which we case we would get here
        // through resolve).

        // Determine the end offset of this object before and after white space.  We use these
        // numbers to validate linearization hint tables.  Offsets and lengths of objects may imply
        // the end of an object to be anywhere between these values.
        qpdf_offset_t end_before_space = file->tell();

        // skip over spaces
        while (true) {
            char ch;
            if (file->read(&ch, 1)) {
                if (!isspace(static_cast<unsigned char>(ch))) {
                    file->seek(-1, SEEK_CUR);
                    break;
                }
            } else {
                throw qpdf.damagedPDF(file->tell(), "EOF after endobj");
            }
        }
        qpdf_offset_t end_after_space = file->tell();
        if (skip_cache_if_in_xref && xref.type(og)) {
            // Ordinarily, an object gets read here when resolved through xref table or stream. In
            // the special case of the xref stream and linearization hint tables, the offset comes
            // from another source. For the specific case of xref streams, the xref stream is read
            // and loaded into the object cache very early in parsing. Ordinarily, when a file is
            // updated by appending, items inserted into the xref table in later updates take
            // precedence over earlier items. In the special case of reusing the object number
            // previously used as the xref stream, we have the following order of events:
            //
            // * reused object gets loaded into the xref table
            // * old object is read here while reading xref streams
            // * original xref entry is ignored (since already in xref table)
            //
            // It is the second step that causes a problem. Even though the xref table is correct in
            // this case, the old object is already in the cache and so effectively prevails over
            // the reused object. To work around this issue, we have a special case for the xref
            // stream (via the skip_cache_if_in_xref): if the object is already in the xref stream,
            // don't cache what we read here.
            //
            // It is likely that the same bug may exist for linearization hint tables, but the
            // existing code uses end_before_space and end_after_space from the cache, so fixing
            // that would require more significant rework. The chances of a linearization hint
            // stream being reused seems smaller because the xref stream is probably the highest
            // object in the file and the linearization hint stream would be some random place in
            // the middle, so I'm leaving that bug unfixed for now. If the bug were to be fixed, we
            // could use !check_og in place of skip_cache_if_in_xref.
            QTC::TC("qpdf", "QPDF skipping cache for known unchecked object");
        } else {
            xref.linearization_offsets(toS(og.getObj()), end_before_space, end_after_space);
            update_table(og, oh.getObj());
        }
    }

    return oh;
}

void
QPDF::Objects::Xref_table::resolve(int id, int gen)
{
    auto& e = entry(id, gen);

    if (e.resolved) {
        // This can happen if an object references itself directly or indirectly in some key that
        // has to be resolved during object parsing, such as stream length.
        QTC::TC("qpdf", "QPDF recursion loop in resolve");
        qpdf.warn(qpdf.damagedPDF(
            "",
            "loop detected resolving object " + std::to_string(id) + ' ' + std::to_string(gen)));
        return;
    }
    e.resolved = true;

    try {
        switch (e.type()) {
        case 0:
            return;
        case 1:
            {
                // Object will be stored in object table by readObjectAtOffset
                QPDFObjGen a_og;
                auto oh = objects.read(true, e.offset(), "", QPDFObjGen(id, gen), a_og, false);
            }
            return;

        case 2:
            resolve_stream(e.stream_number());
            return;

        default:
            throw qpdf.damagedPDF(
                "", 0, ("object " + std::to_string(id) + " has unexpected xref entry type"));
        }
    } catch (QPDFExc& ex) {
        e.resolved = false;
        qpdf.warn(ex);
    } catch (std::exception& ex) {
        e.resolved = false;
        qpdf.warn(qpdf.damagedPDF(
            "",
            0,
            ("object " + QPDFObjGen(id, gen).unparse('/') +
             ": error reading object: " + ex.what())));
    }
}

void
QPDF::Objects::Xref_table::resolve_stream(int obj_stream_number)
{
    auto& e = entry(toS(obj_stream_number));

    if (e.resolved_stream) {
        return;
    }
    e.resolved_stream = true;

    // Force resolution of object stream
    auto obj_stream = objects.get(obj_stream_number, 0);
    if (!obj_stream.isStream()) {
        throw qpdf.damagedPDF(
            "supposed object stream " + std::to_string(obj_stream_number) + " is not a stream");
    }

    auto dict = obj_stream.getDict();
    if (!dict.isDictionaryOfType("/ObjStm")) {
        QTC::TC("qpdf", "QPDF ERR object stream with wrong type");
        qpdf.warn(qpdf.damagedPDF(
            "supposed object stream " + std::to_string(obj_stream_number) + " has wrong type"));
    }

    if (!(dict.getKey("/N").isInteger() && dict.getKey("/First").isInteger())) {
        throw qpdf.damagedPDF(
            ("object stream " + std::to_string(obj_stream_number) + " has incorrect keys"));
    }

    int n = dict.getKey("/N").getIntValueAsInt();
    int first = dict.getKey("/First").getIntValueAsInt();

    std::map<int, int> offsets;

    auto bp = obj_stream.getStreamData(qpdf_dl_specialized);
    auto input = BufferInputSource(
        (file->getName() + " object stream " + std::to_string(obj_stream_number)), bp.get());

    for (int i = 0; i < n; ++i) {
        auto tnum = read_token(input);
        auto toffset = read_token(input);
        if (!(tnum.isInteger() && toffset.isInteger())) {
            throw qpdf.damagedPDF(
                input,
                qpdf.m->last_object_description,
                input.getLastOffset(),
                "expected integer in object stream header");
        }

        int num = QUtil::string_to_int(tnum.getValue().c_str());
        long long offset = QUtil::string_to_int(toffset.getValue().c_str());
        if (num > max_id()) {
            continue;
        }
        if (num == obj_stream_number) {
            QTC::TC("qpdf", "QPDF ignore self-referential object stream");
            qpdf.warn(qpdf.damagedPDF(
                input,
                qpdf.m->last_object_description,
                input.getLastOffset(),
                "object stream claims to contain itself"));
            continue;
        }
        offsets[num] = toI(offset + first);
    }

    // To avoid having to read the object stream multiple times, store all objects that would be
    // found here in the cache.  Remember that some objects stored here might have been overridden
    // by new objects appended to the file, so it is necessary to recheck the xref table and only
    // cache what would actually be resolved here.
    qpdf.m->last_object_description.clear();
    qpdf.m->last_object_description += "object ";
    for (auto const& [id, offset]: offsets) {
        if (type(toS(id)) == 2 && stream_number(id) == obj_stream_number) {
            input.seek(offset, SEEK_SET);
            auto oh = read_compressed_object(input, id);
            objects.update_table(QPDFObjGen(id, 0), oh.getObj());
        } else {
            QTC::TC("qpdf", "QPDF not caching overridden objstm object");
        }
    }
}

QPDF::Objects::~Objects()
{
    // If two objects are mutually referential (through each object having an array or dictionary
    // that contains an indirect reference to the other), the circular references in the
    // std::shared_ptr objects will prevent the objects from being deleted. Walk through all objects
    // in the object cache, which is those objects that we read from the file, and break all
    // resolved indirect references by replacing them with an internal object type representing that
    // they have been destroyed. Note that we can't break references like this at any time when the
    // QPDF object is active. The call to reset also causes all direct QPDFObjectHandle objects that
    // are reachable from this object to release their association with this QPDF. Direct objects
    // are not destroyed since they can be moved to other QPDF objects safely.

    for (auto const& iter: table) {
        iter.second.object->disconnect();
        if (iter.second.object->getTypeCode() != ::ot_null) {
            iter.second.object->destroy();
        }
    }
}

QPDFObject*
QPDF::Objects::resolve(QPDFObjGen og)
{
    if (!unresolved(og)) {
        return table[og].object.get();
    }

    xref.resolve(og.getObj(), og.getGen());

    if (unresolved(og)) {
        // PDF spec says unknown objects resolve to the null object.
        QTC::TC("qpdf", "QPDF resolve failure to null");
        update_table(og, QPDF_Null::create());
    }

    auto& result = table[og].object;
    result->setDefaultDescription(&qpdf, og);
    return result.get();
}

void
QPDF::Objects::update_table(QPDFObjGen og, std::shared_ptr<QPDFObject> const& object)
{
    object->setObjGen(&qpdf, og);
    if (contains(og)) {
        auto& cache = table[og];
        cache.object->assign(object);
    } else {
        table[og] = Entry(object);
    }
}

bool
QPDF::Objects::unresolved(QPDFObjGen og) const noexcept
{
    auto it = table.find(og);
    return it == table.end() || it->second.object->isUnresolved();
}

std::shared_ptr<QPDFObject>
QPDF::Objects::make_indirect(std::shared_ptr<QPDFObject> const& obj)
{
    QPDFObjGen next{next_id(), 0};
    obj->setDefaultDescription(&qpdf, next);
    table[next] = Entry(obj);
    ++next_id_;
    return {obj};
}

void
QPDF::Objects::initialize()
{
    if (initialized) {
        throw std::logic_error("Obj_table::initialize called twice");
    }
    // Does it really make sense to allow the creation of objects if the xref table wasn't
    // successfully read?
    initialized = true;
    // We need to resolve the xref table because during recovery the size of the xref table may
    // increase. Arguably, if we read the xref table cleanly initially, then we should stick with
    // /Size.
    xref.resolve();

    auto xref_size = toI(xref.size());
    if (next_id_ < xref_size) {
        next_id_ = toI(xref.size());
    }
}

QPDFObjectHandle
QPDF::makeIndirectObject(QPDFObjectHandle oh)
{
    if (!oh) {
        throw std::logic_error("attempted to make an uninitialized QPDFObjectHandle indirect");
    }
    return m->objects.make_indirect(oh.getObj());
}

QPDFObjectHandle
QPDF::newReserved()
{
    return m->objects.make_indirect(QPDF_Reserved::create());
}

QPDFObjectHandle
QPDF::newIndirectNull()
{
    return m->objects.make_indirect(QPDF_Null::create());
}

QPDFObjectHandle
QPDF::newStream()
{
    return m->objects.make_indirect(QPDF_Stream::create(
        this, QPDFObjGen(m->objects.next_id(), 1), QPDFObjectHandle::newDictionary(), 0, 0));
}

QPDFObjectHandle
QPDF::newStream(std::shared_ptr<Buffer> data)
{
    auto result = newStream();
    result.replaceStreamData(data, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
    return result;
}

QPDFObjectHandle
QPDF::newStream(std::string const& data)
{
    auto result = newStream();
    result.replaceStreamData(data, QPDFObjectHandle::newNull(), QPDFObjectHandle::newNull());
    return result;
}

QPDFObjectHandle
QPDF::getObject(QPDFObjGen const& og)
{
    return m->objects.get(og);
}

QPDFObjectHandle
QPDF::getObject(int objid, int generation)
{
    return m->objects.get(objid, generation);
}

QPDFObjectHandle
QPDF::getObjectByObjGen(QPDFObjGen const& og)
{
    return m->objects.get(og);
}

QPDFObjectHandle
QPDF::getObjectByID(int objid, int generation)
{
    return m->objects.get(objid, generation);
}

void
QPDF::replaceObject(int objid, int generation, QPDFObjectHandle oh)
{
    m->objects.replace(QPDFObjGen(objid, generation), oh);
}

void
QPDF::Objects::replace(QPDFObjGen og, QPDFObjectHandle oh)
{
    if (!oh || (oh.isIndirect() && !(oh.isStream() && oh.getObjGen() == og))) {
        QTC::TC("qpdf", "QPDF replaceObject called with indirect object");
        throw std::logic_error("QPDF::replaceObject called with indirect object handle");
    }
    update_table(og, oh.getObj());
}

void
QPDF::replaceObject(QPDFObjGen const& og, QPDFObjectHandle oh)
{
    m->objects.replace(og, oh);
}

void
QPDF::Objects::erase(QPDFObjGen og)
{
    if (auto cached = table.find(og); cached != table.end()) {
        // Take care of any object handles that may be floating around.
        cached->second.object->assign(QPDF_Null::create());
        cached->second.object->setObjGen(nullptr, QPDFObjGen());
        table.erase(cached);
    }
}

void
QPDF::replaceReserved(QPDFObjectHandle reserved, QPDFObjectHandle replacement)
{
    QTC::TC("qpdf", "QPDF replaceReserved");
    auto tc = reserved.getTypeCode();
    if (!(tc == ::ot_reserved || tc == ::ot_null)) {
        throw std::logic_error("replaceReserved called with non-reserved object");
    }
    m->objects.replace(reserved.getObjGen(), replacement);
}

QPDFObjectHandle
QPDF::copyForeignObject(QPDFObjectHandle foreign)
{
    // Here's an explanation of what's going on here.
    //
    // A QPDFObjectHandle that is an indirect object has an owning QPDF. The object ID and
    // generation refers to an object in the owning QPDF. When we copy the QPDFObjectHandle from a
    // foreign QPDF into the local QPDF, we have to replace all indirect object references with
    // references to the corresponding object in the local file.
    //
    // To do this, we maintain mappings from foreign object IDs to local object IDs for each foreign
    // QPDF that we are copying from. The mapping is stored in an ObjCopier, which contains a
    // mapping from the foreign ObjGen to the local QPDFObjectHandle.
    //
    // To copy, we do a deep traversal of the foreign object with loop detection to discover all
    // indirect objects that are encountered, stopping at page boundaries. Whenever we encounter an
    // indirect object, we check to see if we have already created a local copy of it. If not, we
    // allocate a "reserved" object (or, for a stream, just a new stream) and store in the map the
    // mapping from the foreign object ID to the new object. While we
    // do this, we keep a list of objects to copy.
    //
    // Once we are done with the traversal, we copy all the objects that we need to copy. However,
    // the copies will contain indirect object IDs that refer to objects in the foreign file. We
    // need to replace them with references to objects in the local file. This is what
    // replaceForeignIndirectObjects does. Once we have created a copy of the foreign object with
    // all the indirect references replaced with new ones in the local context, we can replace the
    // local reserved object with the copy. This mechanism allows us to copy objects with circular
    // references in any order.

    // For streams, rather than copying the objects, we set up the stream data to pull from the
    // original stream by using a stream data provider. This is done in a manner that doesn't
    // require the original QPDF object but may require the original source of the stream data with
    // special handling for immediate_copy_from. This logic is also in
    // replaceForeignIndirectObjects.

    // Note that we explicitly allow use of copyForeignObject on page objects. It is a documented
    // use case to copy pages this way if the intention is to not update the pages tree.
    if (!foreign.isIndirect()) {
        QTC::TC("qpdf", "QPDF copyForeign direct");
        throw std::logic_error("QPDF::copyForeign called with direct object handle");
    }
    QPDF& other = foreign.getQPDF();
    if (&other == this) {
        QTC::TC("qpdf", "QPDF copyForeign not foreign");
        throw std::logic_error("QPDF::copyForeign called with object from this QPDF");
    }

    ObjCopier& obj_copier = m->object_copiers[other.m->unique_id];
    if (!obj_copier.visiting.empty()) {
        throw std::logic_error("obj_copier.visiting is not empty"
                               " at the beginning of copyForeignObject");
    }

    // Make sure we have an object in this file for every referenced object in the old file.
    // obj_copier.object_map maps foreign QPDFObjGen to local objects.  For everything new that we
    // have to copy, the local object will be a reservation, unless it is a stream, in which case
    // the local object will already be a stream.
    reserveObjects(foreign, obj_copier, true);

    if (!obj_copier.visiting.empty()) {
        throw std::logic_error("obj_copier.visiting is not empty after reserving objects");
    }

    // Copy any new objects and replace the reservations.
    for (auto& to_copy: obj_copier.to_copy) {
        QPDFObjectHandle copy = replaceForeignIndirectObjects(to_copy, obj_copier, true);
        if (!to_copy.isStream()) {
            QPDFObjGen og(to_copy.getObjGen());
            replaceReserved(obj_copier.object_map[og], copy);
        }
    }
    obj_copier.to_copy.clear();

    auto og = foreign.getObjGen();
    if (!obj_copier.object_map.count(og)) {
        warn(damagedPDF("unexpected reference to /Pages object while copying foreign object; "
                        "replacing with null"));
        return QPDFObjectHandle::newNull();
    }
    return obj_copier.object_map[foreign.getObjGen()];
}

void
QPDF::reserveObjects(QPDFObjectHandle foreign, ObjCopier& obj_copier, bool top)
{
    auto foreign_tc = foreign.getTypeCode();
    if (foreign_tc == ::ot_reserved) {
        throw std::logic_error("QPDF: attempting to copy a foreign reserved object");
    }

    if (foreign.isPagesObject()) {
        QTC::TC("qpdf", "QPDF not copying pages object");
        return;
    }

    if (foreign.isIndirect()) {
        QPDFObjGen foreign_og(foreign.getObjGen());
        if (!obj_copier.visiting.add(foreign_og)) {
            QTC::TC("qpdf", "QPDF loop reserving objects");
            return;
        }
        if (obj_copier.object_map.count(foreign_og) > 0) {
            QTC::TC("qpdf", "QPDF already reserved object");
            if (!(top && foreign.isPageObject() && obj_copier.object_map[foreign_og].isNull())) {
                obj_copier.visiting.erase(foreign);
                return;
            }
        } else {
            QTC::TC("qpdf", "QPDF copy indirect");
            obj_copier.object_map[foreign_og] =
                foreign.isStream() ? newStream() : newIndirectNull();
            if ((!top) && foreign.isPageObject()) {
                QTC::TC("qpdf", "QPDF not crossing page boundary");
                obj_copier.visiting.erase(foreign_og);
                return;
            }
        }
        obj_copier.to_copy.push_back(foreign);
    }

    if (foreign_tc == ::ot_array) {
        QTC::TC("qpdf", "QPDF reserve array");
        int n = foreign.getArrayNItems();
        for (int i = 0; i < n; ++i) {
            reserveObjects(foreign.getArrayItem(i), obj_copier, false);
        }
    } else if (foreign_tc == ::ot_dictionary) {
        QTC::TC("qpdf", "QPDF reserve dictionary");
        for (auto const& key: foreign.getKeys()) {
            reserveObjects(foreign.getKey(key), obj_copier, false);
        }
    } else if (foreign_tc == ::ot_stream) {
        QTC::TC("qpdf", "QPDF reserve stream");
        reserveObjects(foreign.getDict(), obj_copier, false);
    }

    obj_copier.visiting.erase(foreign);
}

QPDFObjectHandle
QPDF::replaceForeignIndirectObjects(QPDFObjectHandle foreign, ObjCopier& obj_copier, bool top)
{
    auto foreign_tc = foreign.getTypeCode();
    QPDFObjectHandle result;
    if ((!top) && foreign.isIndirect()) {
        QTC::TC("qpdf", "QPDF replace indirect");
        auto mapping = obj_copier.object_map.find(foreign.getObjGen());
        if (mapping == obj_copier.object_map.end()) {
            // This case would occur if this is a reference to a Pages object that we didn't
            // traverse into.
            QTC::TC("qpdf", "QPDF replace foreign indirect with null");
            result = QPDFObjectHandle::newNull();
        } else {
            result = mapping->second;
        }
    } else if (foreign_tc == ::ot_array) {
        QTC::TC("qpdf", "QPDF replace array");
        result = QPDFObjectHandle::newArray();
        int n = foreign.getArrayNItems();
        for (int i = 0; i < n; ++i) {
            result.appendItem(
                // line-break
                replaceForeignIndirectObjects(foreign.getArrayItem(i), obj_copier, false));
        }
    } else if (foreign_tc == ::ot_dictionary) {
        QTC::TC("qpdf", "QPDF replace dictionary");
        result = QPDFObjectHandle::newDictionary();
        std::set<std::string> keys = foreign.getKeys();
        for (auto const& iter: keys) {
            result.replaceKey(
                iter, replaceForeignIndirectObjects(foreign.getKey(iter), obj_copier, false));
        }
    } else if (foreign_tc == ::ot_stream) {
        QTC::TC("qpdf", "QPDF replace stream");
        result = obj_copier.object_map[foreign.getObjGen()];
        result.assertStream();
        QPDFObjectHandle dict = result.getDict();
        QPDFObjectHandle old_dict = foreign.getDict();
        std::set<std::string> keys = old_dict.getKeys();
        for (auto const& iter: keys) {
            dict.replaceKey(
                iter, replaceForeignIndirectObjects(old_dict.getKey(iter), obj_copier, false));
        }
        copyStreamData(result, foreign);
    } else {
        foreign.assertScalar();
        result = foreign;
        result.makeDirect();
    }

    if (top && (!result.isStream()) && result.isIndirect()) {
        throw std::logic_error("replacement for foreign object is indirect");
    }

    return result;
}

void
QPDF::copyStreamData(QPDFObjectHandle result, QPDFObjectHandle foreign)
{
    // This method was originally written for copying foreign streams, but it is used by
    // QPDFObjectHandle to copy streams from the same QPDF object as well.

    QPDFObjectHandle dict = result.getDict();
    QPDFObjectHandle old_dict = foreign.getDict();
    if (m->copied_stream_data_provider == nullptr) {
        m->copied_stream_data_provider = new CopiedStreamDataProvider(*this);
        m->copied_streams =
            std::shared_ptr<QPDFObjectHandle::StreamDataProvider>(m->copied_stream_data_provider);
    }
    QPDFObjGen local_og(result.getObjGen());
    // Copy information from the foreign stream so we can pipe its data later without keeping the
    // original QPDF object around.

    QPDF& foreign_stream_qpdf =
        foreign.getQPDF("unable to retrieve owning qpdf from foreign stream");

    auto stream = foreign.getObjectPtr()->as<QPDF_Stream>();
    if (stream == nullptr) {
        throw std::logic_error("unable to retrieve underlying"
                               " stream object from foreign stream");
    }
    std::shared_ptr<Buffer> stream_buffer = stream->getStreamDataBuffer();
    if ((foreign_stream_qpdf.m->immediate_copy_from) && (stream_buffer == nullptr)) {
        // Pull the stream data into a buffer before attempting the copy operation. Do it on the
        // source stream so that if the source stream is copied multiple times, we don't have to
        // keep duplicating the memory.
        QTC::TC("qpdf", "QPDF immediate copy stream data");
        foreign.replaceStreamData(
            foreign.getRawStreamData(),
            old_dict.getKey("/Filter"),
            old_dict.getKey("/DecodeParms"));
        stream_buffer = stream->getStreamDataBuffer();
    }
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider> stream_provider =
        stream->getStreamDataProvider();
    if (stream_buffer.get()) {
        QTC::TC("qpdf", "QPDF copy foreign stream with buffer");
        result.replaceStreamData(
            stream_buffer, dict.getKey("/Filter"), dict.getKey("/DecodeParms"));
    } else if (stream_provider.get()) {
        // In this case, the remote stream's QPDF must stay in scope.
        QTC::TC("qpdf", "QPDF copy foreign stream with provider");
        m->copied_stream_data_provider->registerForeignStream(local_og, foreign);
        result.replaceStreamData(
            m->copied_streams, dict.getKey("/Filter"), dict.getKey("/DecodeParms"));
    } else {
        auto foreign_stream_data = std::make_shared<ForeignStreamData>(
            foreign_stream_qpdf.m->encp,
            foreign_stream_qpdf.m->file_sp,
            foreign.getObjGen(),
            stream->getParsedOffset(),
            stream->getLength(),
            dict);
        m->copied_stream_data_provider->registerForeignStream(local_og, foreign_stream_data);
        result.replaceStreamData(
            m->copied_streams, dict.getKey("/Filter"), dict.getKey("/DecodeParms"));
    }
}

void
QPDF::swapObjects(int objid1, int generation1, int objid2, int generation2)
{
    m->objects.swap(QPDFObjGen(objid1, generation1), QPDFObjGen(objid2, generation2));
}

void
QPDF::swapObjects(QPDFObjGen const& og1, QPDFObjGen const& og2)
{
    m->objects.swap(og1, og2);
}

void
QPDF::Objects::swap(QPDFObjGen og1, QPDFObjGen og2)
{
    // Force objects to be read from the input source if needed, then swap them in the cache.
    resolve(og1);
    resolve(og2);
    table[og1].object->swapWith(table[og2].object);
}

unsigned long long
QPDF::getUniqueId() const
{
    return m->unique_id;
}

std::string
QPDF::getFilename() const
{
    return m->file->getName();
}

PDFVersion
QPDF::getVersionAsPDFVersion()
{
    int major = 1;
    int minor = 3;
    int extension_level = getExtensionLevel();

    std::regex v("^[[:space:]]*([0-9]+)\\.([0-9]+)");
    std::smatch match;
    if (std::regex_search(m->pdf_version, match, v)) {
        major = QUtil::string_to_int(match[1].str().c_str());
        minor = QUtil::string_to_int(match[2].str().c_str());
    }

    return {major, minor, extension_level};
}

std::string
QPDF::getPDFVersion() const
{
    return m->pdf_version;
}

int
QPDF::getExtensionLevel()
{
    int result = 0;
    QPDFObjectHandle obj = getRoot();
    if (obj.hasKey("/Extensions")) {
        obj = obj.getKey("/Extensions");
        if (obj.isDictionary() && obj.hasKey("/ADBE")) {
            obj = obj.getKey("/ADBE");
            if (obj.isDictionary() && obj.hasKey("/ExtensionLevel")) {
                obj = obj.getKey("/ExtensionLevel");
                if (obj.isInteger()) {
                    result = obj.getIntValueAsInt();
                }
            }
        }
    }
    return result;
}

QPDFObjectHandle
QPDF::getTrailer()
{
    return m->objects.xref_table().trailer();
}

QPDFObjectHandle
QPDF::getRoot()
{
    QPDFObjectHandle root = m->objects.xref_table().trailer().getKey("/Root");
    if (!root.isDictionary()) {
        throw damagedPDF("", 0, "unable to find /Root dictionary");
    } else if (
        // Check_mode is an interim solution to request #810 pending a more comprehensive review of
        // the approach to more extensive checks and warning levels.
        m->check_mode && !root.getKey("/Type").isNameAndEquals("/Catalog")) {
        warn(damagedPDF("", 0, "catalog /Type entry missing or invalid"));
        root.replaceKey("/Type", "/Catalog"_qpdf);
    }
    return root;
}

std::map<QPDFObjGen, QPDFXRefEntry>
QPDF::getXRefTable()
{
    if (!m->objects.xref_table().initialized()) {
        throw std::logic_error("QPDF::getXRefTable called before parsing.");
    }
    return m->objects.xref_table().as_map();
}

size_t
QPDF::Objects::table_size()
{
    // If obj_cache is dense, accommodate all object in tables, else accommodate only original
    // objects.
    if (next_id() >= std::numeric_limits<int>::max() - 1) {
        // Temporary fix. Long-term solution is
        // - QPDFObjGen to enforce objgens are valid and sensible
        // - xref table and obj cache to protect against insertion of impossibly large obj ids
        qpdf.stopOnError("Impossibly large object id encountered.");
    }
    return next_id_ < 1.1 * toI(table.size()) ? toS(++next_id_) : table.size();
}

std::vector<QPDFObjGen>
QPDF::getCompressibleObjVector()
{
    return m->objects.compressible<QPDFObjGen>();
}

std::vector<bool>
QPDF::getCompressibleObjSet()
{
    return m->objects.compressible<bool>();
}

template <typename T>
std::vector<T>
QPDF::Objects::compressible()
{
    // Return a list of objects that are allowed to be in object streams.  Walk through the objects
    // by traversing the document from the root, including a traversal of the pages tree.  This
    // makes that objects that are on the same page are more likely to be in the same object stream,
    // which is slightly more efficient, particularly with linearized files.  This is better than
    // iterating through the xref table since it avoids preserving orphaned items.

    // Exclude encryption dictionary, if any
    QPDFObjectHandle encryption_dict = xref.trailer().getKey("/Encrypt");
    QPDFObjGen encryption_dict_og = encryption_dict.getObjGen();

    const size_t max_obj = toS(next_id());
    std::vector<bool> visited(max_obj, false);
    std::vector<QPDFObjectHandle> queue;
    queue.reserve(512);
    queue.push_back(xref.trailer());
    std::vector<T> result;
    if constexpr (std::is_same_v<T, QPDFObjGen>) {
        result.reserve(table.size());
    } else if constexpr (std::is_same_v<T, bool>) {
        result.resize(max_obj + 1U, false);
    } else {
        throw std::logic_error("Unsupported type in QPDF::getCompressibleObjGens");
    }
    while (!queue.empty()) {
        auto obj = queue.back();
        queue.pop_back();
        if (obj.getObjectID() > 0) {
            QPDFObjGen og = obj.getObjGen();
            const size_t id = toS(og.getObj() - 1);
            if (id >= max_obj) {
                throw std::logic_error(
                    "unexpected object id encountered in getCompressibleObjGens");
            }
            if (visited[id]) {
                QTC::TC("qpdf", "QPDF loop detected traversing objects");
                continue;
            }

            // Check whether this is the current object. If not, remove it (which changes it into a
            // direct null and therefore stops us from revisiting it) and move on to the next object
            // in the queue.
            auto upper = table.upper_bound(og);
            if (upper != table.end() && upper->first.getObj() == og.getObj()) {
                erase(og);
                continue;
            }

            visited[id] = true;

            if (og == encryption_dict_og) {
                QTC::TC("qpdf", "QPDF exclude encryption dictionary");
            } else if (!(obj.isStream() ||
                         (obj.isDictionaryOfType("/Sig") && obj.hasKey("/ByteRange") &&
                          obj.hasKey("/Contents")))) {
                if constexpr (std::is_same_v<T, QPDFObjGen>) {
                    result.push_back(og);
                } else if constexpr (std::is_same_v<T, bool>) {
                    result[id + 1U] = true;
                }
            }
        }
        if (obj.isStream()) {
            QPDFObjectHandle dict = obj.getDict();
            std::set<std::string> keys = dict.getKeys();
            for (auto iter = keys.rbegin(); iter != keys.rend(); ++iter) {
                std::string const& key = *iter;
                QPDFObjectHandle value = dict.getKey(key);
                if (key == "/Length") {
                    // omit stream lengths
                    if (value.isIndirect()) {
                        QTC::TC("qpdf", "QPDF exclude indirect length");
                    }
                } else {
                    queue.push_back(value);
                }
            }
        } else if (obj.isDictionary()) {
            std::set<std::string> keys = obj.getKeys();
            for (auto iter = keys.rbegin(); iter != keys.rend(); ++iter) {
                queue.push_back(obj.getKey(*iter));
            }
        } else if (obj.isArray()) {
            int n = obj.getArrayNItems();
            for (int i = 1; i <= n; ++i) {
                queue.push_back(obj.getArrayItem(n - i));
            }
        }
    }

    return result;
}

bool
QPDF::pipeStreamData(
    std::shared_ptr<EncryptionParameters> encp,
    std::shared_ptr<InputSource> file,
    QPDF& qpdf_for_warning,
    QPDFObjGen const& og,
    qpdf_offset_t offset,
    size_t length,
    QPDFObjectHandle stream_dict,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    std::unique_ptr<Pipeline> to_delete;
    if (encp->encrypted) {
        decryptStream(encp, file, qpdf_for_warning, pipeline, og, stream_dict, to_delete);
    }

    bool attempted_finish = false;
    try {
        file->seek(offset, SEEK_SET);
        auto buf = std::make_unique<char[]>(length);
        if (auto read = file->read(buf.get(), length); read != length) {
            throw damagedPDF(*file, "", offset + toO(read), "unexpected EOF reading stream data");
        }
        pipeline->write(buf.get(), length);
        attempted_finish = true;
        pipeline->finish();
        return true;
    } catch (QPDFExc& e) {
        if (!suppress_warnings) {
            qpdf_for_warning.warn(e);
        }
    } catch (std::exception& e) {
        if (!suppress_warnings) {
            QTC::TC("qpdf", "QPDF decoding error warning");
            qpdf_for_warning.warn(
                // line-break
                damagedPDF(
                    *file,
                    "",
                    file->getLastOffset(),
                    ("error decoding stream data for object " + og.unparse(' ') + ": " +
                     e.what())));
            if (will_retry) {
                qpdf_for_warning.warn(
                    // line-break
                    damagedPDF(
                        *file,
                        "",
                        file->getLastOffset(),
                        "stream will be re-processed without filtering to avoid data loss"));
            }
        }
    }
    if (!attempted_finish) {
        try {
            pipeline->finish();
        } catch (std::exception&) {
            // ignore
        }
    }
    return false;
}

bool
QPDF::pipeStreamData(
    QPDFObjGen const& og,
    qpdf_offset_t offset,
    size_t length,
    QPDFObjectHandle stream_dict,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    return pipeStreamData(
        m->encp,
        m->file_sp,
        *this,
        og,
        offset,
        length,
        stream_dict,
        pipeline,
        suppress_warnings,
        will_retry);
}

bool
QPDF::pipeForeignStreamData(
    std::shared_ptr<ForeignStreamData> foreign,
    Pipeline* pipeline,
    bool suppress_warnings,
    bool will_retry)
{
    if (foreign->encp->encrypted) {
        QTC::TC("qpdf", "QPDF pipe foreign encrypted stream");
    }
    return pipeStreamData(
        foreign->encp,
        foreign->file,
        *this,
        foreign->foreign_og,
        foreign->offset,
        foreign->length,
        foreign->local_dict,
        pipeline,
        suppress_warnings,
        will_retry);
}

// Throw a generic exception when we lack context for something more specific. New code should not
// use this. This method exists to improve somewhat from calling assert in very old code.
void
QPDF::stopOnError(std::string const& message)
{
    throw damagedPDF("", message);
}

// Return an exception of type qpdf_e_damaged_pdf.
QPDFExc
QPDF::damagedPDF(
    InputSource& input, std::string const& object, qpdf_offset_t offset, std::string const& message)
{
    return {qpdf_e_damaged_pdf, input.getName(), object, offset, message};
}

// Return an exception of type qpdf_e_damaged_pdf.  The object is taken from
// m->last_object_description.
QPDFExc
QPDF::damagedPDF(InputSource& input, qpdf_offset_t offset, std::string const& message)
{
    return damagedPDF(input, m->last_object_description, offset, message);
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file.
QPDFExc
QPDF::damagedPDF(std::string const& object, qpdf_offset_t offset, std::string const& message)
{
    return {qpdf_e_damaged_pdf, m->file->getName(), object, offset, message};
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file and the
// offset from .m->file->getLastOffset().
QPDFExc
QPDF::damagedPDF(std::string const& object, std::string const& message)
{
    return damagedPDF(object, m->file->getLastOffset(), message);
}

// Return an exception of type qpdf_e_damaged_pdf. The filename is taken from m->file and the object
// from .m->last_object_description.
QPDFExc
QPDF::damagedPDF(qpdf_offset_t offset, std::string const& message)
{
    return damagedPDF(m->last_object_description, offset, message);
}

// Return an exception of type qpdf_e_damaged_pdf.  The filename is taken from m->file, the object
// from m->last_object_description and the offset from m->file->getLastOffset().
QPDFExc
QPDF::damagedPDF(std::string const& message)
{
    return damagedPDF(m->last_object_description, m->file->getLastOffset(), message);
}

bool
QPDF::everCalledGetAllPages() const
{
    return m->ever_called_get_all_pages;
}

bool
QPDF::everPushedInheritedAttributesToPages() const
{
    return m->ever_pushed_inherited_attributes_to_pages;
}

void
QPDF::removeSecurityRestrictions()
{
    auto root = getRoot();
    root.removeKey("/Perms");
    auto acroform = root.getKey("/AcroForm");
    if (acroform.isDictionary() && acroform.hasKey("/SigFlags")) {
        acroform.replaceKey("/SigFlags", QPDFObjectHandle::newInteger(0));
    }
}
