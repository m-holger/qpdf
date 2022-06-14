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

// This example is part of my experiments with a new version of
// QPDFObjectHandle.

// As a first step, it provides a toy implementation of the
// QPDFObjectHandle::at method.

// The cref_demo procedure demonstrates that:
// - the behaviour of at is consistent with the standard library.
// - OH behaves like a reference.
// - at prevents an object from being inserted into a container more than
//     once.

// As part of the experiment, a number of calls to getKey, getArrayItem,
// replaceKey, etc have been replaced with calls to at in QPDFJob and
// various example programs in order to demonstrates that OH can be used
// alongside the existing QPDFObjectHandle and thus allows a smooth and
// gradual migration.

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

    cref_demo();
    return 0;
}
