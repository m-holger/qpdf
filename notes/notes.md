# Random Notes

## Current PRs

### [Code tidy - reflow comments and strings #978 ](https://github.com/qpdf/qpdf/pull/979)

Because of the near certainty of merge conflicts, almost all of my current work is based on this PR.

### [Apply various Clang-Tidy rules #982](https://github.com/qpdf/qpdf/pull/982)

This largely replaces [Code tidy classes QPDF and QPDFObjectHandle #972 ](https://github.com/qpdf/qpdf/pull/972)
For the straight-forward rules I have decided to deal with one rule at a time rather than deal with one class at a
time. This should make it easier to review because all essentially identical changes are bunched together.

### [Code tidy classes QPDF and QPDFObjectHandle #972 ](https://github.com/qpdf/qpdf/pull/972)

As mentioned in #982 this is essentially on hold while I am dealing with rules that can be resolved with a
single, more or less mechanical solution.

For the moment the only points worth looking at are the open questions raised earlier:

- Should the public part of the header should have individual ```// NOLINT``` annotations or should the whole public section
  be bracketed with
  ```
  // NOLINTBEGIN (modernize-use-nodiscard) ABI
  ...
  // NOLINTEND (modernize-use-nodiscard)
  ```

  pairs as in
  eg [8dd1929](https://github.com/qpdf/qpdf/pull/972/commits/8dd1929eef323424bb3ab79410649df4a25c013b)
  lines 53 and 721. I would lean towards the latter as I don't see any significant benefit in the former.

- Do we want to do any clean-up of declarations of exported methods at the moment. While things likely
  adding attribute [[nodiscard]] should not cause problems is the benefit worth the risk? How confident are we that
  abi-perf-test will detect all possible issues. I would lean towards suppressing warnings for the moment and
  dealing with those parts of the header files when we are ready to break ABI.


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

### Use result unpacking in range based for loop over maps

i.e. use 

```cpp
for (auto const& [key, value]: oh.dItems()){
    // ...
}
```
When I looked at this last time it had a noticeable negative performance impact. On Clang-15, using it  has a tiny 
but statistically significant performance improvement. For GCC-12 there is a greater improvement (but also still 
very small)

(Timings 10 Jun 2023 18:49:42, 10 Jun 2023 20:28:02)


### Fix comments in example pdf-invert-images 

Some of the comments apply to pointer holder, but not necessarily std managed pointers.

local branch `example`

### Tune QPDFParser

#### Avoid creating QPDF_Name objects for dictionary keys

This has a significant positive performance impact.

Local branch `nparse`

#### Avoid creating QPDF_Integer objects for indirect references

Not checked, but in view of above is likely to have a positive performance impact.

#### Create dictionaries on the fly

Rather than pushing object into a std::vector and building a std::map at the end, add a map to the StackFrame and add
entries as we go along.

This has a positive performance impact, but it is slightly tricky to deal with missing keys.

local branch `nparse`

### Remove double indirection from QPDFObject/QPDFValue

local branch `var4_12b`