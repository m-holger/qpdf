#include <qpdf/QPDF_Array.hh>

#include <qpdf/JSON.hh>
#include <qpdf/QPDFObjectHandle.hh>
#include <qpdf/QPDFObject_private.hh>
#include <qpdf/QPDFValue.hh>

static const QPDFObjectHandle null_oh = QPDFObjectHandle::newNull();

inline void
QPDF_Array::checkOwnership(QPDFObjectHandle const& item) const
{
    if (auto obj = item.getObjectPtr()) {
        if (value->qpdf) {
            if (auto item_qpdf = obj->getQPDF()) {
                if (value->qpdf != item_qpdf) {
                    throw std::logic_error(
                        "Attempting to add an object from a different QPDF. Use "
                        "QPDF::copyForeignObject to add objects from another file.");
                }
            }
        }
    } else {
        throw std::logic_error("Attempting to add an uninitialized object to a QPDF_Array.");
    }
}

QPDF_Array::QPDF_Array(std::vector<std::shared_ptr<QPDFObject>>&& v, bool sparse) :
    sparse(sparse)
{
    if (sparse) {
        m = std::make_unique<Members>();
        for (auto&& item: v) {
            if (item->getTypeCode() != ::ot_null || item->getObjGen().isIndirect()) {
                m->sp_elements[m->sp_size] = std::move(item);
            }
            ++m->sp_size;
        }
    } else {
        elements = std::move(v);
    }
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(std::vector<QPDFObjectHandle> const& items)
{
    auto obj = QPDFObject::create_container<QPDF_Array>();
    auto& array = std::get<QPDF_Array>(obj->value.var);
    array.setFromVector(items);
    return obj;
}

std::shared_ptr<QPDFObject>
QPDF_Array::create(std::vector<std::shared_ptr<QPDFObject>>&& items, bool sparse)
{
    return QPDFObject::create_container<QPDF_Array>(std::move(items), sparse);
}

std::shared_ptr<QPDFObject>
QPDF_Array::copy(bool shallow)
{
    if (shallow) {
        return QPDFObject::create_container<QPDF_Array>(*this);
    } else {
        if (sparse) {
            QPDF_Array* result = new QPDF_Array();
            result->m = std::make_unique<Members>();
            result->m->sp_size = m->sp_size;
            for (auto const& element: m->sp_elements) {
                auto const& obj = element.second;
                result->m->sp_elements[element.first] =
                    obj->getObjGen().isIndirect() ? obj : obj->copy();
            }
            return QPDFObject::create_container<QPDF_Array>(*result);
        } else {
            std::vector<std::shared_ptr<QPDFObject>> result;
            result.reserve(elements.size());
            for (auto const& element: elements) {
                result.push_back(
                    element ? (element->getObjGen().isIndirect() ? element : element->copy())
                            : element);
            }
            return create(std::move(result), false);
        }
    }
}

void
QPDF_Array::disconnect()
{
    if (sparse) {
        for (auto& item: m->sp_elements) {
            auto& obj = item.second;
            if (!obj->getObjGen().isIndirect()) {
                obj->disconnect();
            }
        }
    } else {
        for (auto& obj: elements) {
            if (!obj->getObjGen().isIndirect()) {
                obj->disconnect();
            }
        }
    }
}

std::string
QPDF_Array::unparse()
{
    std::string result = "[ ";
    if (sparse) {
        int next = 0;
        for (auto& item: m->sp_elements) {
            int key = item.first;
            for (int j = next; j < key; ++j) {
                result += "null ";
            }
            item.second->resolve();
            auto og = item.second->getObjGen();
            result += og.isIndirect() ? og.unparse(' ') + " R " : item.second->unparse() + " ";
            next = ++key;
        }
        for (int j = next; j < m->sp_size; ++j) {
            result += "null ";
        }
    } else {
        for (auto const& item: elements) {
            item->resolve();
            auto og = item->getObjGen();
            result += og.isIndirect() ? og.unparse(' ') + " R " : item->unparse() + " ";
        }
    }
    result += "]";
    return result;
}

JSON
QPDF_Array::getJSON(int json_version)
{
    static const JSON j_null = JSON::makeNull();
    JSON j_array = JSON::makeArray();
    if (sparse) {
        int next = 0;
        for (auto& item: m->sp_elements) {
            int key = item.first;
            for (int j = next; j < key; ++j) {
                j_array.addArrayElement(j_null);
            }
            auto og = item.second->getObjGen();
            j_array.addArrayElement(
                og.isIndirect() ? JSON::makeString(og.unparse(' ') + " R")
                                : item.second->getJSON(json_version));
            next = ++key;
        }
        for (int j = next; j < m->sp_size; ++j) {
            j_array.addArrayElement(j_null);
        }
    } else {
        for (auto const& item: elements) {
            auto og = item->getObjGen();
            j_array.addArrayElement(
                og.isIndirect() ? JSON::makeString(og.unparse(' ') + " R")
                                : item->getJSON(json_version));
        }
    }
    return j_array;
}

QPDFObjectHandle
QPDF_Array::at(int n) const noexcept
{
    if (n < 0 || n >= size()) {
        return {};
    } else if (sparse) {
        auto const& iter = m->sp_elements.find(n);
        return iter == m->sp_elements.end() ? null_oh : (*iter).second;
    } else {
        return elements[size_t(n)];
    }
}

std::vector<QPDFObjectHandle>
QPDF_Array::getAsVector() const
{
    if (sparse) {
        std::vector<QPDFObjectHandle> v;
        v.reserve(size_t(size()));
        for (auto const& item: m->sp_elements) {
            v.resize(size_t(item.first), null_oh);
            v.emplace_back(item.second);
        }
        v.resize(size_t(size()), null_oh);
        return v;
    } else {
        return {elements.cbegin(), elements.cend()};
    }
}

bool
QPDF_Array::setAt(int at, QPDFObjectHandle const& oh)
{
    if (at < 0 || at >= size()) {
        return false;
    }
    checkOwnership(oh);
    if (sparse) {
        m->sp_elements[at] = oh.getObj();
    } else {
        elements[size_t(at)] = oh.getObj();
    }
    return true;
}

void
QPDF_Array::setFromVector(std::vector<QPDFObjectHandle> const& v)
{
    elements.resize(0);
    elements.reserve(v.size());
    for (auto const& item: v) {
        checkOwnership(item);
        elements.push_back(item.getObj());
    }
}

bool
QPDF_Array::insert(int at, QPDFObjectHandle const& item)
{
    int sz = size();
    if (at < 0 || at > sz) {
        // As special case, also allow insert beyond the end
        return false;
    } else if (at == sz) {
        push_back(item);
    } else {
        checkOwnership(item);
        if (sparse) {
            auto iter = m->sp_elements.crbegin();
            while (iter != m->sp_elements.crend()) {
                auto key = (iter++)->first;
                if (key >= at) {
                    auto nh = m->sp_elements.extract(key);
                    ++nh.key();
                    m->sp_elements.insert(std::move(nh));
                } else {
                    break;
                }
            }
            m->sp_elements[at] = item.getObj();
            ++m->sp_size;
        } else {
            elements.insert(elements.cbegin() + at, item.getObj());
        }
    }
    return true;
}

void
QPDF_Array::push_back(QPDFObjectHandle const& item)
{
    checkOwnership(item);
    if (sparse) {
        m->sp_elements[m->sp_size++] = item.getObj();
    } else {
        elements.push_back(item.getObj());
    }
}

bool
QPDF_Array::erase(int at)
{
    if (at < 0 || at >= size()) {
        return false;
    }
    if (sparse) {
        auto end = m->sp_elements.end();
        if (auto iter = m->sp_elements.lower_bound(at); iter != end) {
            if (iter->first == at) {
                iter++;
                m->sp_elements.erase(at);
            }

            while (iter != end) {
                auto nh = m->sp_elements.extract(iter++);
                --nh.key();
                m->sp_elements.insert(std::move(nh));
            }
        }
        --m->sp_size;
    } else {
        elements.erase(elements.cbegin() + at);
    }
    return true;
}
