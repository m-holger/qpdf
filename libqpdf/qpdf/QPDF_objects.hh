#ifndef QPDF_OBJECTS_HH
#define QPDF_OBJECTS_HH

#include <qpdf/QPDF.hh>

#include <qpdf/QPDF_Null.hh>
#include <qpdf/QPDF_Unresolved.hh>

#include <variant>

class QPDF::Objects
{
  public:
    // Xref_table encapsulates the pdf's xref table and trailer.
    class Xref_table
    {
      public:
        Xref_table(Objects& objects) :
            qpdf(objects.qpdf),
            objects(objects),
            file(objects.file)
        {
        }

        void initialize();
        void initialize_empty();
        void initialize_json();
        void reconstruct(QPDFExc& e);
        void show();
        // Resolve all object in the xref table.
        void resolve();

        QPDFObjectHandle
        trailer() const
        {
            return trailer_;
        }

        void
        trailer(QPDFObjectHandle&& oh)
        {
            trailer_ = std::move(oh);
        }

        // Returns 0 if og is not in table.
        size_t
        type(QPDFObjGen og) const
        {
            int id = og.getObj();
            if (id < 1 || static_cast<size_t>(id) >= table.size()) {
                return 0;
            }
            auto& e = table[static_cast<size_t>(id)];
            return e.gen() == og.getGen() ? e.type() : 0;
        }

        // Returns 0 if og is not in table.
        size_t
        type(size_t id) const noexcept
        {
            if (id >= table.size()) {
                return 0;
            }
            return table[id].type();
        }

        // Returns 0 if og is not in table.
        qpdf_offset_t
        offset(QPDFObjGen og) const noexcept
        {
            int id = og.getObj();
            if (id < 1 || static_cast<size_t>(id) >= table.size()) {
                return 0;
            }
            return table[static_cast<size_t>(id)].offset();
        }

        // Returns 0 if id is not in table.
        int
        stream_number(int id) const noexcept
        {
            if (id < 1 || static_cast<size_t>(id) >= table.size()) {
                return 0;
            }
            return table[static_cast<size_t>(id)].stream_number();
        }

        int
        stream_index(int id) const noexcept
        {
            if (id < 1 || static_cast<size_t>(id) >= table.size()) {
                return 0;
            }
            return table[static_cast<size_t>(id)].stream_index();
        }

        QPDFObjGen at_offset(qpdf_offset_t offset) const noexcept;

        std::map<QPDFObjGen, QPDFXRefEntry> as_map() const;

        bool
        object_streams() const noexcept
        {
            return object_streams_;
        }

        // Return a vector of object id and stream number for each compressed object.
        std::vector<std::pair<unsigned int, int>>
        compressed_objects() const
        {
            if (!initialized()) {
                throw std::logic_error("Xref_table::compressed_objects called before parsing.");
            }

            std::vector<std::pair<unsigned int, int>> result;
            result.reserve(table.size());

            unsigned int i{0};
            for (auto const& item: table) {
                if (item.type() == 2) {
                    result.emplace_back(i, item.stream_number());
                }
                ++i;
            }
            return result;
        }

        // Temporary access to underlying table size
        size_t
        size() const noexcept
        {
            return table.size();
        }

        void
        ignore_streams(bool val) noexcept
        {
            ignore_streams_ = val;
        }

        bool
        initialized() const noexcept
        {
            return initialized_;
        }

        void
        attempt_recovery(bool val) noexcept
        {
            attempt_recovery_ = val;
        }

        int
        max_id() const noexcept
        {
            return max_id_;
        }

        void resolve(int id, int gen);

        // For Linearization

        qpdf_offset_t
        end_after_space(QPDFObjGen og)
        {
            auto& e = entry(toS(og.getObj()));
            switch (e.type()) {
            case 1:
                return e.end_after_space_;
            case 2:
                {
                    auto es = entry(toS(e.stream_number()));
                    return es.type() == 1 ? es.end_after_space_ : 0;
                }
            default:
                return 0;
            }
        }

        qpdf_offset_t
        end_before_space(QPDFObjGen og)
        {
            auto& e = entry(toS(og.getObj()));
            switch (e.type()) {
            case 1:
                return e.end_before_space_;
            case 2:
                {
                    auto es = entry(toS(e.stream_number()));
                    return es.type() == 1 ? es.end_before_space_ : 0;
                }
            default:
                return 0;
            }
        }

        void
        linearization_offsets(size_t id, qpdf_offset_t before, qpdf_offset_t after)
        {
            if (type(id)) {
                table[id].end_before_space_ = before;
                table[id].end_after_space_ = after;
            }
        }

        bool
        uncompressed_after_compressed() const noexcept
        {
            return uncompressed_after_compressed_;
        }

        // Actual value from file
        qpdf_offset_t
        first_item_offset() const noexcept
        {
            return first_item_offset_;
        }

      private:
        // Object, count, offset of first entry
        typedef std::tuple<int, int, qpdf_offset_t> Subsection;

        struct Uncompressed
        {
            Uncompressed(qpdf_offset_t offset) :
                offset(offset)
            {
            }
            qpdf_offset_t offset;
        };

        struct Compressed
        {
            Compressed(int stream_number, int stream_index) :
                stream_number(stream_number),
                stream_index(stream_index)
            {
            }
            int stream_number{0};
            int stream_index{0};
        };

        typedef std::variant<std::monostate, Uncompressed, Compressed> Xref;

        struct Entry
        {
            Entry() = default;

            Entry(int gen, Xref entry) :
                gen_(gen),
                entry(entry)
            {
            }

            int
            gen() const noexcept
            {
                return gen_;
            }

            size_t
            type() const noexcept
            {
                return entry.index();
            }

            qpdf_offset_t
            offset() const noexcept
            {
                return type() == 1 ? std::get<1>(entry).offset : 0;
            }

            int
            stream_number() const noexcept
            {
                return type() == 2 ? std::get<2>(entry).stream_number : 0;
            }

            int
            stream_index() const noexcept
            {
                return type() == 2 ? std::get<2>(entry).stream_index : 0;
            }

            int gen_{0};
            bool resolved{false};
            bool resolved_stream{false};
            Xref entry;
            qpdf_offset_t end_before_space_{0};
            qpdf_offset_t end_after_space_{0};
        };

        Entry&
        entry(size_t id)
        {
            return id < table.size() ? table[id] : table[0];
        }

        Entry&
        entry(int id, int gen)
        {
            auto& e = entry(toS(id));
            return e.gen_ == gen ? e : table[0];
        }

        void read(qpdf_offset_t offset);

        // Methods to parse tables
        qpdf_offset_t process_section(qpdf_offset_t offset);
        std::vector<Subsection> subsections(std::string& line);
        std::vector<Subsection> bad_subsections(std::string& line, qpdf_offset_t offset);
        Subsection subsection(std::string const& line);
        std::tuple<bool, qpdf_offset_t, int, char> read_entry();
        std::tuple<bool, qpdf_offset_t, int, char> read_bad_entry();

        // Methods to parse streams
        qpdf_offset_t read_stream(qpdf_offset_t offset);
        qpdf_offset_t process_stream(qpdf_offset_t offset, QPDFObjectHandle& xref_stream);
        std::pair<int, std::array<int, 3>>
        process_W(QPDFObjectHandle& dict, std::function<QPDFExc(std::string_view)> damaged);
        std::pair<int, size_t> process_Size(
            QPDFObjectHandle& dict,
            int entry_size,
            std::function<QPDFExc(std::string_view)> damaged);
        std::pair<int, std::vector<std::pair<int, int>>> process_Index(
            QPDFObjectHandle& dict,
            int max_num_entries,
            std::function<QPDFExc(std::string_view)> damaged);

        void resolve_stream(int obj_stream_number);
        QPDFObjectHandle read_compressed_object(InputSource& input, int obj);

        QPDFObjectHandle read_trailer();

        QPDFTokenizer::Token
        read_token(size_t max_len = 0)
        {
            return objects.tokenizer.readToken(*file, "", true, max_len);
        }

        QPDFTokenizer::Token
        read_token(InputSource& input)
        {
            return objects.tokenizer.readToken(input, "", true);
        }

        // Methods to insert table entries
        void insert(int obj, int f0, qpdf_offset_t f1, int f2);
        void insert_free(QPDFObjGen);

        QPDFExc
        damaged_pdf(std::string const& msg)
        {
            return qpdf.damagedPDF("", 0, msg);
        }

        QPDFExc
        damaged_table(std::string const& msg)
        {
            return qpdf.damagedPDF("xref table", msg);
        }

        void
        warn_damaged(std::string const& msg)
        {
            qpdf.warn(damaged_pdf(msg));
        }

        QPDF& qpdf;
        Objects& objects;
        InputSource* const& file;

        std::vector<Entry> table;
        QPDFObjectHandle trailer_;

        bool attempt_recovery_{true};
        bool initialized_{false};
        bool ignore_streams_{false};
        bool reconstructed_{false};
        bool object_streams_{false};
        // Before the xref table is initialized, max_id_ is an upper bound on the possible object
        // ids that could be present in the PDF file. Once the trailer has been read, max_id_ is set
        // to the value of /Size. If the file is damaged, max_id_ becomes the maximum object id in
        // the xref table after reconstruction.
        int max_id_{std::numeric_limits<int>::max() - 1};

        // Linearization data
        bool uncompressed_after_compressed_{false};
        qpdf_offset_t first_item_offset_{0}; // actual value from file
    }; // Xref_table

    Objects(QPDF& qpdf, InputSource* const& file) :
        qpdf(qpdf),
        file(file),
        xref(*this)
    {
        tokenizer.allowEOF();
    }

    ~Objects();

    Xref_table&
    xref_table() noexcept
    {
        return xref;
    }

    Xref_table const&
    xref_table() const noexcept
    {
        return xref;
    }

    bool
    contains(QPDFObjGen og) const noexcept
    {
        auto it = table.find(og);
        return it != table.end() && it->second.gen == og.getGen();
    }
    bool unresolved(QPDFObjGen og) const noexcept;

    QPDFObjectHandle
    get(int id, int gen)
    {
        return get(QPDFObjGen(id, gen));
    }

    QPDFObjectHandle
    get(QPDFObjGen og)
    {
        auto it = table.find(og);
        if (it != table.end() && it->second.gen == og.getGen()) {
            return {it->second.object};
        }
        if (it != table.end()) {
            return {QPDF_Null::create()};
        }
        if (xref.initialized() && !xref.type(og)) {
            return {QPDF_Null::create()};
        }
        return {table.try_emplace(og, og.getGen(), QPDF_Unresolved::create(&qpdf, og))
                    .first->second.object};
    }

    std::shared_ptr<QPDFObject>
    get_for_json(int id, int gen)
    {
        auto og = QPDFObjGen(id, gen);
        auto [it, inserted] = table.try_emplace(og);
        auto& obj = it->second.object;
        if (inserted) {
            it->second.gen = gen;
            if (id >= next_id_) {
                next_id_ = id + 1;
            }
            obj =
                !xref.type(og) ? QPDF_Null::create(&qpdf, og) : QPDF_Unresolved::create(&qpdf, og);
        } else {
            if (it->second.gen != gen) {
                return QPDF_Null::create();
            }
        }
        return obj;
    }

    std::shared_ptr<QPDFObject>
    get_for_parser(int id, int gen, bool parse_pdf)
    {
        // This method is called by the parser and therefore must not resolve any objects.
        auto og = QPDFObjGen(id, gen);
        auto iter = table.find(og);
        if (iter != table.end() && iter->second.gen == gen) {
            return iter->second.object;
        }
        if (iter != table.end()) {
            return QPDF_Null::create();
        }
        if (xref.type(og) || !xref.initialized()) {
            return table.insert({og, {gen, QPDF_Unresolved::create(&qpdf, og)}})
                .first->second.object;
        }
        if (parse_pdf) {
            return QPDF_Null::create();
        }
        return table.insert({og, {gen, QPDF_Null::create(&qpdf, og)}}).first->second.object;
    }

    std::vector<QPDFObjectHandle> all();

    void erase(QPDFObjGen og);

    void replace(QPDFObjGen og, QPDFObjectHandle oh);

    void swap(QPDFObjGen og1, QPDFObjGen og2);

    std::shared_ptr<QPDFObject> make_indirect(std::shared_ptr<QPDFObject> const& obj);

    QPDFObjectHandle read(
        bool try_recovery,
        qpdf_offset_t offset,
        std::string const& description,
        QPDFObjGen exp_og,
        QPDFObjGen& og,
        bool skip_cache_if_in_xref);

    int
    next_id()
    {
        if (!initialized) {
            initialize();
        }
        return next_id_;
    }

    // QPDFWriter 'normal' ObjTable size (i.e. part of the ObjTables implemented as std::vector).
    size_t table_size();

    QPDFObject* resolve(QPDFObjGen og);

    // Get a list of objects that would be permitted in an object stream.
    template <typename T>
    std::vector<T> compressible();

    QPDFTokenizer::Token
    read_token(size_t max_len = 0)
    {
        return tokenizer.readToken(*file, "", true, max_len);
    }

    std::pair<QPDFObjectHandle, bool> parse(
        InputSource& input,
        std::string const& description,
        QPDFObjectHandle::StringDecrypter* decrypter_ptr = nullptr);

  private:
    struct Entry
    {
        Entry() = default;

        Entry(int gen, std::shared_ptr<QPDFObject>&& object) :
            gen(gen),
            object(std::move(object))
        {
        }

        Entry(int gen, std::shared_ptr<QPDFObject> const& object) :
            gen(gen),
            object(object)
        {
        }

        int gen{0};
        std::shared_ptr<QPDFObject> object;
    }; // Entry

    void initialize();
    void update_table(QPDFObjGen og, std::shared_ptr<QPDFObject> const& object);
    void update_table(int id, int gen, std::shared_ptr<QPDFObject> const& a_object);

    QPDFObjectHandle read_object(QPDFObjGen og);
    void read_stream(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset);
    void validate_stream_line_end(QPDFObjectHandle& object, QPDFObjGen og, qpdf_offset_t offset);
    size_t recover_stream_length(QPDFObjGen og, qpdf_offset_t stream_offset);

    void current_object(std::string const& description, QPDFObjGen og);

    QPDF& qpdf;
    InputSource* const& file;

    QPDFTokenizer tokenizer;
    Xref_table xref;
    std::map<QPDFObjGen, Entry> table;
    int next_id_{1};
    bool initialized{false};

}; // Objects

#endif // QPDF_OBJECTS_HH
