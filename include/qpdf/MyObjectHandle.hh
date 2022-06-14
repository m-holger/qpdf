#ifndef MYOBJECTHANDLE_HH
#define MYOBJECTHANDLE_HH

#include <qpdf/DLL.h>

#include <string>

#include <qpdf/QPDFObjectHandle.hh>

class OH: public QPDFObjectHandle
{
  public:
    QPDF_DLL
    OH(QPDFObjectHandle oh, QPDFObjectHandle parent, std::string key);

    QPDF_DLL
    OH(QPDFObjectHandle oh, QPDFObjectHandle parent, size_t index);

    QPDF_DLL
    OH(QPDFObjectHandle&);

    QPDF_DLL
    OH(QPDFObjectHandle&&);

    QPDF_DLL
    OH(OH const&);

    QPDF_DLL
    OH(OH&&);

    QPDF_DLL
    OH& operator=(OH&);

    QPDF_DLL
    OH& operator=(OH&&);
};

#endif // MYOBJECTHANDLE_HH
