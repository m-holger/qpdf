# Random Notes

## Current PRs

### [Code tidy - reflow comments and strings #978 ](https://github.com/qpdf/qpdf/pull/979)

Because of the near certainty of merge commits, allmost all of my current work is based on this PR.

### [Apply various Clang-Tidy rules #982](https://github.com/qpdf/qpdf/pull/982)

This largely replaces [Code tidy classes QPDF and QPDFObjectHandle #972 ](https://github.com/qpdf/qpdf/pull/972).
For the straight-forward rules I have decided to deal with one rule at a time rather than deal with one class at a
time. This should make it easier to review because all essentially identical changes are bunched together.

### [Avoid unnecessary copying of stream dictionary in QPDF::readObject #980 ](https://github.com/qpdf/qpdf/pull/980)

### [Remove redundant loop in QPDFWriter::prepareFileForWrite #981](https://github.com/qpdf/qpdf/pull/981)

The two PRs are minor code tidies of things I spotted while working on other things.

### [Add new Buffer method copy and deprecate copy constructor / assignment operator #983](https://github.com/qpdf/qpdf/pull/983)

As previously discussed by email.

### [Add const overloads for various QPDFObjectHandle methods #979 ](https://github.com/qpdf/qpdf/pull/979)

WIP - This PR was triggered by the work on Clang-Tidy rule performance-for-range-copy.

As a result of pushing all state from QPDFObjectHandle down to QPDFObject and QPDFValue most of the QPDFObjectHandle
method that in the past had to be non-const because of the possibility that they would trigger object resolution can
now be const. This has significant benefits including allowing range based for loops to be optimized.

Switching to const methods allows us to create a replacement for QPDFObjectHandle::ditems that outperforms both ditems
and getDictAsMap.

(TODO - expand)

## To Do

### Change JSONHandler::m to unique_ptr

local branch `members`

### Reconsider result unpacking in range based for loop over maps

i.e. consider using 

```cpp
for (auto const& [key, value]: oh.dItems()){
    // ...
}
```
When I looked at this last time it had a noticeable negative performance impact. On Clang-15, using it in 
QPDFWriter::enqueueObject actually had a tiny but statistically significant performance improvement.

Confirm that the result holds more generally (or that at least it does not have a significant negative impact) and if 
so, tidy existing code.


### Fix comments in example pdf-invert-images 

Some of the comments apply to pointer holder, but not necessarily std managed pointers.

local branch `example`

### Tune QPDFParser

#### Avoid creating QPDF_Name objects for dictionary keys

This has a significant positive performance impact.

Local branch `nparse`

#### Avoid creating QPDF_Integer objects for indirect references

Not checked, but in view of above is likely to have a posiotive performance impact.

#### Create dictionaries on the fly

Rather than pushing object into a std::vector and building a std::map at the end, add a map to the StackFrame and add
entries as we go along.

This has a possitive performance impact, but it is slightly tricky to deal with missing keys.

local branch `nparse`

### Remove double indirection from QPDFObject/QPDFValue

local branch `var4_12b`