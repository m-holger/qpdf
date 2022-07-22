#include <qpdf/QUtil.hh>

#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include <qpdf/MyObjectHandle.hh>

static char const* whoami = 0;

/*
This example is part of my experiments with a new version of QPDFObjectHandle.
In this version I am trying to investigate ways to address a number of what I
perceive to be issues with the existing version in the context of todays QPDF,
which has become a tool to manipulate pdf files in addition to the original
purpose of exploring existing pdf files.


The issues
----------

- A fundamental concept in the pdf specification is that a direct object has
  a single parent. This is not enforced by the existing implementation. This
  failure can lead to subtle errors that may only become an issue a long time
  after the error occurred and may be difficult to trace.

- The existing implementation does neither behave like a value nor a
  reference type. This increases the risk of programmer confusion and as a
  result programmer errors.

- The existing API is, quite understandably given the history of QPDF, heavily
  geared to the low-level syntax of pdf files, making it unnecessarily verbose
  and therefore error-prone to express the more high-level semantics of a pdf
  file.

- As the result of the point above, there is very limited scope for compile
  time type checking, deferring error detection to run time. The problem is
  aggravated by the fact that many existing pdf files are faulty and as a
  result loads of warnings are routinely issued and errors are quietly fixed.
  This introduces a significant risk that programming errors are not detected
  and fixed in a timely manner.

- #664

In addition to the main issues above, the API has become quite large. It
probably would make the API easier to use if it could be shrunk and ideally if
it could leverage familiarity with the standard library.


The constraints
---------------

There is a large existing code base. Any changes must not alter the behaviour
of any existing programs.

Given the existing code base, in order for any new API to be practical it must
work seamlessly with the existing API in order to allow a gradual and
incremental migration.

Given that QPDF is used on some very large files, any new API cannot add
significant overhead.


My general approach
-------------------

I am implementing my version of object handle as a subclass of
QPDFObjectHandle. This ensures that handles can be used with the existing API,
and when used with the existing API, behave exactly like a normal
QPDFObjectHandle.

To ensure that the first two constraints are satisfied I am using QPDFJob and
the example programs as a test bed. The aim is to ensure that the qpdf test
suite passes even when parts of the programs are updated to use my version
of object handle. (I am not suggesting that those changes should actually be
implemented for the life version)

For the last constrain I have in the past relied on judging the runtime of
the test suite. I have recently switched to using abi-perf-test /
performance_check. Some of the results obtained using those tools have been
somewhat surprising to me. Monitoring of the last constraint will need to
be tightened.


Early conclusions
-----------------

In early experiments I implemented my object handle as a value type (i.e.
    auto result = some_handle.at(...
behaves exactly like the standard library equivalent) and as a reference
type (i.e. my version of

    auto result = some_handle.at(...

behaves like

    auto& result = some_handle.at(...

in the standard library equivalent). I decided to investigate the reference
type option further on the basis that it required fewer changes to QPDFObject
and therefore was the safer option.

My second conclusion is that to cleanly implement a version of object handle
that behaves consistently like a reference type, QPDFObjectHandle needs to
behave like a constant shared_ptr to a mutable QPDFObject. This requires
QPDFObject to be able to dynamically change its pdf object type.

TODO: expand on last paragraph


My experiment sofar
-------------------

As a first step, it provides a toy implementation of the QPDFObjectHandle::at
method.

The cref_demo procedure at the bottom of this file demonstrates that:
- the behaviour of at is consistent with the standard library.
- OH behaves like a reference.
- at prevents an object from being inserted into a container more than
  once.

As part of the experiment, a number of calls to getKey, getArrayItem,
replaceKey, etc have been replaced with calls to at in QPDFJob and various
example programs in order to demonstrates that OH can be used alongside the
existing QPDFObjectHandle and thus allows a smooth and gradual migration.

QPDFObject has been modified to keep track of an objects parent. This is purely
for the purposes of this experiment and as it stands does not work reliably in
code that uses a mixture of OH and QPDFObjectHandle methods. It only  works
with direct objects. The experimental code makes extensive use of friend
classes.

At the moment, the at method does not yet make use of the QPDFValue type.


at(index) and size methods
--------------------------

The behavior of these methods is based on two observations:

- It is common for entries in PDF dictionary objects to be optional.

- It is common for entries that allow for a list of (e.g Action) objects to
  also allow a single object of the (same) type, as well as maybe a null
  object.

To allow for this, the at and size methods are implemented for all object
types. For the purpose of these methods, the null object is treated as an empty
array, and all other non-array types, including dictionary, are treated as a
single element array.
*/

void
demo_at_and_size()
{
    auto array = "[1 /Two << /A 3 /B 4 >>  null [5 6]]"_qpdf;
    assert(array.at(0).size() == 1);
    assert(array.at(0).at(0).unparse() == "1");
    assert(array.at(1).size() == 1);
    assert(array.at(1).at(0).unparse() == "/Two");
    assert(array.at(2).size() == 1);
    assert(array.at(2).getKeys().size() == 2);
    assert(array.at(2).at(0).unparse() == "<< /A 3 /B 4 >>");
    assert(array.at(3).size() == 0);
    // assert( array.at(3).at(0).unparse() == "1"); // "index error"
    assert(array.at(4).size() == 2);
    assert(array.at(4).at(0).unparse() == "5");
}

/*
Having at and size methods makes it straight forward to implement an iterator
that permits looping eg over an optional Action or array of Actions.
*/

void
demo_loops()
{
    OH array = "[1 /Two << /A 3 /B 4 >>  null [5 6]]"_qpdf;
    int i = 0;
    for (auto item: array) {
        std::cout << i++ << " : ";
        for (auto subitem: item) {
            std::cout << " " << subitem.unparse() << " ; ";
        }
        std::cout << std::endl;
    }
}

/*
Output:
0 :  1 ;
1 :  /Two ;
2 :  << /A 3 /B 4 >> ;
3 :
4 :  5 ;  6 ;


The next steps are:

- Remove QPDFObject::key and index as obsoleted by QPDFValue.

- Remove use QPDFValue::parent in at.

- Review implementation of "single parent rule"

- Review use of shared_ptr vs weak_ptr.

- Implement OH::Int as an example of a typesafe object handle. A preview of
  what I am contemplating is given below.

OH::Int
-------

For the purpose of this section let us pretend that rectangles are arrays
of integers rather than of numbers.

There will be a method asInt to cast a suitable object handle as an OH::Int.

    auto good = "42"_qpdf.asInt();
    auto bad = "42.7"_qpdf.asInt(); // throws exception

OH::Int will be convertible to and from integer types.

    unsigned int n = good;
    assert(n == good.getUIntValueAsUInt());
    good = 66;
    good = "66"; // compilation error
    good = "66"_qpdf;
    good = "/Sixty"_qpdf; // throws exception

Given suitable parameters, asInt will be able to handle more complex integer
types.

    auto page = "<< /MediaBox [ 0 0 612 792 ] >>";
    auto rotate = page.at("/Rotate").asInt(...);
    int r = rotate; // r == 0
    rotate = 90; // page.at("/Rotate").getIntValue() == 90

    auto crop = page.at("/CropBox").asInt(...);
    int c3 = crop.at(3); // c3 == 792

    page.at("/MediaBox").at(0) = 100;
    int c0 = crop.at(0); // c0 == 100

    auto s1 = page.unparse(); // s1 == "<< /MediaBox [ 100 0 612 792 ] /Rotate
90 >>"

    crop = 42; // throws exception
    crop.at(1) = 100;
    auto s2 = page.unparse(); // s2 == "<< /CropBox [ 100 100 612 792 ]"
                                       " /MediaBox [ 100 0 612 792 ] /Rotate 90
>>"

OH::Int will support for loops.

    for (auto coord : page.at("/MediaBox")) {
        coord = 2 * coord;
    }
    auto s3 = page.at("/MediaBox").unparse(); // s3 == "[ 200 0 1224 1584 ]"
*/

typedef std::vector<int> Box;
typedef std::map<std::string, Box> Page;
typedef std::vector<Page> Kids;

void
cref_demo()
{
    Kids c_kids0{
        {{"/Media", {0, 0, 600, 800}}, {"/Trim", {100, 100, 500, 700}}}};
    c_kids0.reserve(20);

    auto& c_kids = c_kids0;
    auto& c_p0 = c_kids.at(0);
    c_kids.push_back(c_p0);
    c_kids.push_back(c_p0);

    auto q_kids =
        "[<< /Media [0 0 600 800] /Trim [100 100 500 700]>> 1 2 3]"_qpdf;
    // push_back is not yet implemented. Use placeholders and at() instead
    auto q_p0 = q_kids.at(0);
    q_kids.at(1) = q_p0;
    q_kids.at(2) = q_p0;

    auto& c_p0_media = c_p0.at("/Media");
    auto q_p0_media = q_p0.at("/Media");
    c_p0_media.at(0) = 1;
    q_p0_media.at(0) = "1"_qpdf;
    c_p0_media.at(1) = 1;
    q_p0_media.at(1) = "1"_qpdf;
    auto& c_p1 = c_kids.at(1);
    auto q_p1 = q_kids.at(1);
    c_p1.at("/Media").at(0) = 2;
    q_p1.at("/Media").at(0) = "2"_qpdf;
    auto& c_p2 = c_kids.at(2);
    auto q_p2 = q_kids.at(2);
    c_p2["/Media"].at(0) = 3;
    q_p2.at("/Media").at(0) = "3"_qpdf;
    auto& c_media = c_p0.at("/Media");
    auto q_media = q_p0.at("/Media");
    c_p0["/Crop"] = c_media;
    q_p0.at("/Crop") = q_media;
    c_media.at(2) = 602;
    q_media.at(2) = "602"_qpdf;
    c_p1["/Crop"] = c_media;
    q_p1.at("/Crop") = q_media;
    auto& c_p3 = c_p2;
    auto q_p3 = q_p2;
    c_kids.push_back(c_p3);
    q_kids.at(3) = q_p3;
    c_p2.at("/Trim").at(2) = 503;
    q_p2.at("/Trim").at(2) = "503"_qpdf;

    Kids c_expected{
        {{"/Crop", {1, 1, 600, 800}},
         {"/Media", {1, 1, 602, 800}},
         {"/Trim", {100, 100, 500, 700}}},
        {{"/Crop", {1, 1, 602, 800}},
         {"/Media", {2, 0, 600, 800}},
         {"/Trim", {100, 100, 500, 700}}},
        {{"/Media", {3, 0, 600, 800}}, {"/Trim", {100, 100, 503, 700}}},
        {{"/Media", {3, 0, 600, 800}}, {"/Trim", {100, 100, 500, 700}}}};

    assert(c_kids.at(0) == c_expected.at(0));
    assert(
        q_kids.at(0).unparse() ==
        "<< /Crop [ 1 1 600 800 ] "
        "/Media [ 1 1 602 800 ] /Trim [ 100 100 500 700 ] >>");
    assert(c_kids.at(1) == c_expected.at(1));
    assert(
        q_kids.at(1).unparse() ==
        "<< /Crop [ 1 1 602 800 ] "
        "/Media [ 2 0 600 800 ] /Trim [ 100 100 500 700 ] >>");
    assert(c_kids.at(2) == c_expected.at(2));
    assert(
        q_kids.at(2).unparse() ==
        "<< /Media [ 3 0 600 800 ] /Trim [ 100 100 503 700 ] >>");
    assert(c_kids.at(3) == c_expected.at(3));
    assert(
        q_kids.at(3).unparse() ==
        "<< /Media [ 3 0 600 800 ] /Trim [ 100 100 500 700 ] >>");
}

void
usage()
{
    std::cerr << "Usage: " << whoami << "" << std::endl
              << "Basic features of OH" << std::endl;
    exit(2);
}

int
main(int argc, char* argv[])
{
    whoami = QUtil::getWhoami(argv[0]);

    if (!(argc == 1)) {
        usage();
    }

    demo_at_and_size();
    demo_loops();
    cref_demo();
    return 0;
}
