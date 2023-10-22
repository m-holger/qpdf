#ifndef QPDF_STREAM_HH
#define QPDF_STREAM_HH

#include <qpdf/Types.h>

#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFStreamFilter.hh>

#include <functional>
#include <memory>

class Pipeline;
class QPDF;
class QPDFValue;

class QPDF_Stream
{
    friend class QPDFObject;

  public:
    QPDF_Stream(QPDF_Stream&&) = default;
    QPDF_Stream& operator=(QPDF_Stream&&) = default;
    ~QPDF_Stream() = default;
    static std::shared_ptr<QPDFObject> create(
        QPDF*,
        QPDFObjGen const& og,
        QPDFObjectHandle stream_dict,
        qpdf_offset_t offset,
        size_t length);
    std::shared_ptr<QPDFObject> copy(bool shallow = false);
    std::string unparse();
    JSON getJSON(int json_version);

    void
    disconnect()
    {
        m->stream_provider = nullptr;
        QPDFObjectHandle::DisconnectAccess::disconnect(this->stream_dict);
    }
    QPDFObjectHandle
    getDict() const
    {
        return this->stream_dict;
    }
    bool
    isDataModified() const
    {
        return (!m->token_filters.empty());
    }
    void
    setFilterOnWrite(bool val)
    {
        m->filter_on_write = val;
    }
    bool
    getFilterOnWrite() const
    {
        return m->filter_on_write;
    }
    // Methods to help QPDF copy foreign streams
    size_t
    getLength() const
    {
        return m->length;
    }
    std::shared_ptr<Buffer>
    getStreamDataBuffer() const
    {
        return m->stream_data;
    }
    std::shared_ptr<QPDFObjectHandle::StreamDataProvider>
    getStreamDataProvider() const
    {
        return m->stream_provider;
    }

    // See comments in QPDFObjectHandle.hh for these methods.
    bool pipeStreamData(
        Pipeline*,
        bool* tried_filtering,
        int encode_flags,
        qpdf_stream_decode_level_e decode_level,
        bool suppress_warnings,
        bool will_retry);
    std::shared_ptr<Buffer> getStreamData(qpdf_stream_decode_level_e);
    std::shared_ptr<Buffer> getRawStreamData();
    void replaceStreamData(
        std::shared_ptr<Buffer> data,
        QPDFObjectHandle const& filter,
        QPDFObjectHandle const& decode_parms);
    void replaceStreamData(
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider> provider,
        QPDFObjectHandle const& filter,
        QPDFObjectHandle const& decode_parms);
    void
    addTokenFilter(std::shared_ptr<QPDFObjectHandle::TokenFilter> token_filter)
    {
        m->token_filters.push_back(token_filter);
    }
    JSON getStreamJSON(
        int json_version,
        qpdf_json_stream_data_e json_data,
        qpdf_stream_decode_level_e decode_level,
        Pipeline* p,
        std::string const& data_filename);

    void
    replaceDict(QPDFObjectHandle const& new_dict)
    {
        this->stream_dict = new_dict;
        setDictDescription();
    }

    static void
    registerStreamFilter(
        std::string const& filter_name, std::function<std::shared_ptr<QPDFStreamFilter>()> factory)
    {
        filter_factories[filter_name] = factory;
    }

  private:
    QPDF_Stream(
        QPDF*,
        QPDFObjGen const& og,
        QPDFObjectHandle stream_dict,
        qpdf_offset_t offset,
        size_t length);
    static std::map<std::string, std::string> filter_abbreviations;
    static std::map<std::string, std::function<std::shared_ptr<QPDFStreamFilter>()>>
        filter_factories;

    void replaceFilterData(
        QPDFObjectHandle const& filter, QPDFObjectHandle const& decode_parms, size_t length);
    bool filterable(
        std::vector<std::shared_ptr<QPDFStreamFilter>>& filters,
        bool& specialized_compression,
        bool& lossy_compression);
    void warn(std::string const& message);
    void setDictDescription();

    struct Members
    {
        Members(size_t length) :
            length(length)
        {
        }

        bool filter_on_write{true};
        size_t length;
        std::shared_ptr<Buffer> stream_data;
        std::shared_ptr<QPDFObjectHandle::StreamDataProvider> stream_provider;
        std::vector<std::shared_ptr<QPDFObjectHandle::TokenFilter>> token_filters;
    };

    std::unique_ptr<Members> m;
    QPDFObjectHandle stream_dict;
    QPDFValue* value{nullptr};
};

#endif // QPDF_STREAM_HH
