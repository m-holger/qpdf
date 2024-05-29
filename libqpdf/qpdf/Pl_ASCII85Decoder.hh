#ifndef PL_ASCII85DECODER_HH
#define PL_ASCII85DECODER_HH

#include <qpdf/Pipeline.hh>

#include <cstring>

class Pl_ASCII85Decoder final: public Pipeline
{
  public:
    Pl_ASCII85Decoder(std::string_view identifier, Pipeline& next) :
        Pipeline(identifier, &next)
    {
        memset(this->inbuf, 117, 5);
    }
    ~Pl_ASCII85Decoder() final = default;
    void write(unsigned char const* buf, size_t len) final;
    void finish() final;

  private:
    void flush();

    unsigned char inbuf[5];
    size_t pos{0};
    size_t eod{0};
};

#endif // PL_ASCII85DECODER_HH
