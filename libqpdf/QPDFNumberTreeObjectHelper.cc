#include <qpdf/QPDFNumberTreeObjectHelper.hh>

#include <qpdf/NNTree.hh>
#include <qpdf/QIntC.hh>

namespace
{
    class NumberTreeDetails: public NNTreeDetails
    {
      public:
        std::string const&
        itemsKey() const override
        {
            static std::string k("/Nums");
            return k;
        }
        bool
        keyValid(QPDFObjectHandle oh) const override
        {
            return oh.isInteger();
        }
        int
        compareKeys(QPDFObjectHandle a, QPDFObjectHandle b) const override
        {
            if (!(keyValid(a) && keyValid(b))) {
                // We don't call this without calling keyValid first
                throw std::logic_error("comparing invalid keys");
            }
            auto as = a.getIntValue();
            auto bs = b.getIntValue();
            return ((as < bs) ? -1 : (as > bs) ? 1 : 0);
        }
    };
} // namespace

static NumberTreeDetails number_tree_details;

QPDFNumberTreeObjectHelper::~QPDFNumberTreeObjectHelper() // NOLINT (modernize-use-equals-default)
{
    // Must be explicit and not inline -- see QPDF_DLL_CLASS in README-maintainer. For this specific
    // class, see github issue #745.
}

QPDFNumberTreeObjectHelper::Members::Members(QPDFObjectHandle& oh, QPDF& q, bool auto_repair) :
    impl(std::make_shared<NNTreeImpl>(number_tree_details, q, oh, auto_repair))
{
}

QPDFNumberTreeObjectHelper::QPDFNumberTreeObjectHelper(
    QPDFObjectHandle oh, QPDF& q, bool auto_repair) :
    QPDFObjectHelper(oh),
    m(new Members(oh, q, auto_repair))
{
}

QPDFNumberTreeObjectHelper
QPDFNumberTreeObjectHelper::newEmpty(QPDF& qpdf, bool auto_repair)
{
    return {qpdf.makeIndirectObject("<< /Nums [] >>"_qpdf), qpdf, auto_repair};
}

QPDFNumberTreeObjectHelper::iterator::iterator(std::shared_ptr<NNTreeIterator> const& i) :
    impl(i)
{
}

bool
QPDFNumberTreeObjectHelper::iterator::valid() const
{
    return impl->valid();
}

QPDFNumberTreeObjectHelper::iterator&
QPDFNumberTreeObjectHelper::iterator::operator++()
{
    ++(*impl);
    updateIValue();
    return *this;
}

QPDFNumberTreeObjectHelper::iterator&
QPDFNumberTreeObjectHelper::iterator::operator--()
{
    --(*impl);
    updateIValue();
    return *this;
}

void
QPDFNumberTreeObjectHelper::iterator::updateIValue()
{
    if (impl->valid()) {
        auto p = *impl;
        this->ivalue.first = p->first.getIntValue();
        this->ivalue.second = p->second;
    } else {
        this->ivalue.first = 0;
        this->ivalue.second = QPDFObjectHandle();
    }
}

QPDFNumberTreeObjectHelper::iterator::reference
QPDFNumberTreeObjectHelper::iterator::operator*()
{
    updateIValue();
    return this->ivalue;
}

QPDFNumberTreeObjectHelper::iterator::pointer
QPDFNumberTreeObjectHelper::iterator::operator->()
{
    updateIValue();
    return &this->ivalue;
}

bool
QPDFNumberTreeObjectHelper::iterator::operator==(iterator const& other) const
{
    return *(impl) == *(other.impl);
}

void
QPDFNumberTreeObjectHelper::iterator::insertAfter(numtree_number key, QPDFObjectHandle value)
{
    impl->insertAfter(QPDFObjectHandle::newInteger(key), value);
    updateIValue();
}

void
QPDFNumberTreeObjectHelper::iterator::remove()
{
    impl->remove();
    updateIValue();
}

QPDFNumberTreeObjectHelper::iterator
QPDFNumberTreeObjectHelper::begin() const
{
    return {std::make_shared<NNTreeIterator>(m->impl->begin())};
}

QPDFNumberTreeObjectHelper::iterator
QPDFNumberTreeObjectHelper::end() const
{
    return {std::make_shared<NNTreeIterator>(m->impl->end())};
}

QPDFNumberTreeObjectHelper::iterator
QPDFNumberTreeObjectHelper::last() const
{
    return {std::make_shared<NNTreeIterator>(m->impl->last())};
}

QPDFNumberTreeObjectHelper::iterator
QPDFNumberTreeObjectHelper::find(numtree_number key, bool return_prev_if_not_found)
{
    auto i = m->impl->find(QPDFObjectHandle::newInteger(key), return_prev_if_not_found);
    return {std::make_shared<NNTreeIterator>(i)};
}

QPDFNumberTreeObjectHelper::iterator
QPDFNumberTreeObjectHelper::insert(numtree_number key, QPDFObjectHandle value)
{
    auto i = m->impl->insert(QPDFObjectHandle::newInteger(key), value);
    return {std::make_shared<NNTreeIterator>(i)};
}

bool
QPDFNumberTreeObjectHelper::remove(numtree_number key, QPDFObjectHandle* value)
{
    return m->impl->remove(QPDFObjectHandle::newInteger(key), value);
}

QPDFNumberTreeObjectHelper::numtree_number
QPDFNumberTreeObjectHelper::getMin()
{
    auto i = begin();
    if (i == end()) {
        return 0;
    }
    return i->first;
}

QPDFNumberTreeObjectHelper::numtree_number
QPDFNumberTreeObjectHelper::getMax()
{
    auto i = last();
    if (i == end()) {
        return 0;
    }
    return i->first;
}

bool
QPDFNumberTreeObjectHelper::hasIndex(numtree_number idx)
{
    auto i = find(idx);
    return (i != this->end());
}

bool
QPDFNumberTreeObjectHelper::findObject(numtree_number idx, QPDFObjectHandle& oh)
{
    auto i = find(idx);
    if (i == end()) {
        return false;
    }
    oh = i->second;
    return true;
}

bool
QPDFNumberTreeObjectHelper::findObjectAtOrBelow(
    numtree_number idx, QPDFObjectHandle& oh, numtree_number& offset)
{
    auto i = find(idx, true);
    if (i == end()) {
        return false;
    }
    oh = i->second;
    QIntC::range_check_subtract(idx, i->first);
    offset = idx - i->first;
    return true;
}

void
QPDFNumberTreeObjectHelper::setSplitThreshold(int t)
{
    m->impl->setSplitThreshold(t);
}

std::map<QPDFNumberTreeObjectHelper::numtree_number, QPDFObjectHandle>
QPDFNumberTreeObjectHelper::getAsMap() const
{
    std::map<numtree_number, QPDFObjectHandle> result;
    result.insert(begin(), end());
    return result;
}
