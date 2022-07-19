#ifndef MYOBJECTHANDLE_HH
#define MYOBJECTHANDLE_HH

#include <qpdf/DLL.h>

#include <string>

#include <qpdf/QPDFObjectHandle.hh>

class OH: public QPDFObjectHandle
{
  public:
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

    // OH inherits the at() method from QPDFObjectHandle

    QPDF_DLL
    size_t size() const;
};

#endif // MYOBJECTHANDLE_HH
