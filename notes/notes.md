# Random Notes

## Current PRs

The PRs are listed in the suggested order of review.

### [ Rename README-maintainer to README-maintainer.md and add table of content #987 ](https://github.com/qpdf/qpdf/pull/987)

### [ Change code formatting from clang-format >= 15 to clang-format-16 #986 ](https://github.com/qpdf/qpdf/pull/986)

Two minor housekeeping suggestions. If 987 is accepted it will conflict with 986 and 985.

I would suggest we use for Markdown a default indentation / tab width of 2 characters (rather than the 4 char
default in CLion).

### [ Add FUTURE build option #989 ](https://github.com/qpdf/qpdf/pull/989)

This is my stab at creating a FUTURE option as discussed back in March. My implementation is based on common sense
and your implementation of ENABLE_QTC rather than on any understanding of cmake, so handle with appropriate care.

I have included basic documentation in installation.rst, but if you want to accept this PR it probably would need
tweaking to reflect how you want to present the option.

It also probably would warrant an update to the release process in README-maintainer and consideration of CI (have a
second run of the pikepdf build with the FUTURE option?).

Finally, in light of [#942](https://github.com/qpdf/qpdf/issues/942), would it make sense to also implement a
```CPP_20``` build option?

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

- Should the public part of the header should have individual ```// NOLINT``` annotations or should the whole public
  section be bracketed with
  ```
  // NOLINTBEGIN (modernize-use-nodiscard) ABI
  ...
  // NOLINTEND (modernize-use-nodiscard)
  ```

  pairs as in eg [8dd1929](https://github.com/qpdf/qpdf/pull/972/commits/8dd1929eef323424bb3ab79410649df4a25c013b)
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

### [Change JSONHandler::m to std::unique_ptr and declare Members in implementation file #985](https://github.com/qpdf/qpdf/pull/985)

This provides an example of the change I am suggesting to how we use the member pattern going forward. An update to
the relevant section of README-maintainer is is included.

### [Add const overloads for various QPDFObjectHandle methods #979 ](https://github.com/qpdf/qpdf/pull/979)

WIP - This PR was triggered by the work on Clang-Tidy rule performance-for-range-copy.

As a result of pushing all state from QPDFObjectHandle down to QPDFObject and QPDFValue most of the QPDFObjectHandle
method that in the past had to be non-const because of the possibility that they would trigger object resolution can
now be const. This has significant benefits including allowing range based for loops to be optimized.

Switching to const methods allows us to create a replacement for QPDFObjectHandle::ditems that outperforms both ditems
and getDictAsMap.

(TODO - expand)

## To Do

### Use result unpacking in range based for loop over maps

i.e. use

```cpp
for (auto const& [key, value]: oh.dItems()){
    // ...
}
```

When I looked at this last time it had a noticeable negative performance impact. On Clang-15, using it has a tiny
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