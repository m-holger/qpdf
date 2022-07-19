#include <qpdf/QPDFObject.hh>

#include <qpdf/QPDFObjectHandle.hh>

void
QPDFObject::assign2(QPDFObjectHandle& oh)
{
    // Assign a new object oh to the parent
    //
    // TODO : do we want to check oh.obj == this?
    // Yes - we could have to handles pointing to the same object
    // which means that when we unlink this, we also unlink value.obj.

    oh.obj->parent = parent;
    oh.obj->key = key;
    oh.obj->index = index;
    parent->value->assign2(this, oh);
    // Unlink this object from its previous parent
    parent = nullptr;
    key = "";
    index = std::numeric_limits<size_t>::max();
}

QPDFObjectHandle
QPDFObject::at(size_t index)
{
    if (index >= size()) {
        throw std::runtime_error("index error");
    }
    auto tc = value->type_code;
    if (tc == ot_array) {
        auto oh = value->at(index);
        oh.obj->parent = self.lock();
        oh.obj->key = "";
        oh.obj->index = index;
        return oh;
    } else {
        return self.lock();
    }
}
QPDFObjectHandle
QPDFObject::at(std::string const& key)
{
    auto result = value->at(key);
    auto obj = result.obj;
    obj->parent = self.lock();
    obj->key = key;
    obj->index = std::numeric_limits<size_t>::max();
    return result;
}