#ifndef OBJTABLE_HH
#define OBJTABLE_HH

#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>

#include "qpdf/QIntC.hh"
#include <limits>

// A table of objects indexed by object id. This is intended as a more efficient replacement for
// std::map<QPDFObjGen, T> containers.
//
// The table is implemented using a std::vector for  storage, with the object id implicitly
// represented by the index of the object.
//
// The provided overloads of the access operator[] are safe. For out of bounds access they will
// either extend the table or throw a runtime error.
//
// ObjTable has a map 'sparse_elements' to deal with very sparse / extremely large object tables
// (usually as the result of invalid dangling references). This map may contain objects not found in
// the xref table of the original pdf if there are dangling references with an id significantly
// larger than the largest valid object id found in original pdf.
//
// Operations like resize or emplace_back extend the vector part of the ObjTable. They will move
// sparse elements as necessary.
template <class T>
class ObjTable
{
  public:
    using reference = T&;
    ObjTable() = default;
    ObjTable(const ObjTable&) = delete;
    ObjTable(ObjTable&&) = delete;

    inline T const&
    operator[](int idx) const
    {
        return element(static_cast<size_t>(idx));
    }

    inline T const&
    operator[](unsigned int idx) const
    {
        return element(idx);
    }

    inline T const&
    operator[](QPDFObjGen og) const
    {
        return element(static_cast<size_t>(og.getObj()));
    }

    inline T const&
    operator[](QPDFObjectHandle oh) const
    {
        return element(static_cast<size_t>(oh.getObjectID()));
    }

    inline bool
    contains(size_t idx) const
    {
        return idx < elements.size() || sparse_elements.count(idx);
    }

    inline bool
    contains(QPDFObjGen og) const
    {
        return contains(static_cast<size_t>(og.getObj()));
    }

    inline bool
    contains(QPDFObjectHandle oh) const
    {
        return contains(static_cast<size_t>(oh.getObjectID()));
    }

    // Size of the vector part of ObjTable.
    inline size_t
    size() const noexcept
    {
        return elements.size();
    }

    inline void
    reserve(size_t new_cap)
    {
        elements.reserve(new_cap);
    }

  protected:
    inline T&
    operator[](int id)
    {
        return element(static_cast<size_t>(id));
    }

    inline T&
    operator[](QPDFObjGen og)
    {
        return element(static_cast<size_t>(og.getObj()));
    }

    inline T&
    operator[](QPDFObjectHandle oh)
    {
        return element(static_cast<size_t>(oh.getObjectID()));
    }

    inline T&
    operator[](unsigned int id)
    {
        return element(id);
    }

    // Return pointer to element id. If not found, return a nullptr.
    // NB The element pointed to may not be valid (i.e. default constructed).
    inline T const*
    find(size_t id) const noexcept
    {
        if (id < elements.size()) {
            return &elements[id];
        }
        auto it = sparse_elements.find(id);
        if (it == sparse_elements.end()) {
            return nullptr;
        }
        return &it->second;
    }

    // Return pointer to element id. If not found, return a nullptr.
    // NB The element pointed to may not be valid (i.e. default constructed).
    inline T*
    find(size_t id) noexcept
    {
        if (id < elements.size()) {
            return &elements[id];
        }
        auto it = sparse_elements.find(id);
        if (it == sparse_elements.end()) {
            return nullptr;
        }
        return &it->second;
    }

    // emplace_back to the end of the vector. If there are any conflicting sparse elements, emplace
    // them to the back of the vector before adding the current element.
    template <class... Args>
    inline T&
    emplace_back(Args&&... args)
    {
        if (min_sparse == elements.size()) {
            return emplace_back_large(std::forward<Args&&...>(args...));
        }
        return elements.emplace_back(std::forward<Args&&...>(args...));
    }

    // Try to emplace an element to the end of the vector. If there is a conflicting (non-default)
    // sparse element, emplace it to the end of the vector instead. Return a reference to the
    // emplaced element and true if a new element has been inserted.
    template <class... Args>
    inline std::pair<T&, bool>
    try_emplace_back(Args&&... args)
    {
        if (min_sparse == elements.size()) {
            auto it = sparse_elements.begin();

            bool insert = it->second == T();
            auto& result = !insert ? elements.emplace_back(std::move(it->second))
                                   : elements.emplace_back(std::forward<Args&&...>(args...));
            it = sparse_elements.erase(it);
            min_sparse =
                it == sparse_elements.end() ? std::numeric_limits<size_t>::max() : it->first;
            return {result, insert};
        }
        return {elements.emplace_back(std::forward<Args&&...>(args...)), true};
    }

    // Try to emplace an element to the end of the vector. If there is a conflicting sparse element,
    // emplace it to the end of the vector instead. Return a reference to the emplaced element and
    // true if a new element has been inserted.
    template <class... Args>
    inline std::pair<T&, bool>
    try_emplace_back(Args&&... args)
    {
        if (min_sparse == std::vector<T>::size()) {
            auto it = sparse_elements.begin();
            auto& result = std::vector<T>::emplace_back(std::move(it->second));
            it = sparse_elements.erase(it);
            min_sparse =
                it == sparse_elements.end() ? std::numeric_limits<size_t>::max() : it->first;
            return {result, false};
        }
        return {std::vector<T>::emplace_back(std::forward<Args&&...>(args...)), true};
    }

    void
    resize(size_t a_size)
    {
        elements.resize(a_size);
        if (a_size > min_sparse) {
            auto it = sparse_elements.begin();
            auto end = sparse_elements.end();
            while (it != end && it->first < a_size) {
                elements[it->first] = std::move(it->second);
                it = sparse_elements.erase(it);
            }
            min_sparse = (it == end) ? std::numeric_limits<size_t>::max() : it->first;
        }
    }

    inline void
    forEach(std::function<void(int, T const&)> fn) const
    {
        int i = 0;
        for (auto const& item: elements) {
            fn(i++, item);
        }
        for (auto const& [id, item]: sparse_elements) {
            fn(QIntC::to_int(id), item);
        }
    }

    inline void
    forEach(std::function<void(int, T&)> fn)
    {
        int i = 0;
        for (auto& item: elements) {
            fn(i++, item);
        }
        for (auto& [id, item]: sparse_elements) {
            fn(QIntC::to_int(id), item);
        }
    }

  private:
    inline T&
    element(size_t idx)
    {
        if (idx < elements.size()) {
            return elements[idx];
        }
        return large_element(idx);
    }

    // Must only be called by element. Separated out from element to keep inlined code tight.
    T&
    large_element(size_t idx)
    {
        static const size_t max_size = elements.max_size();
        if (idx < min_sparse) {
            min_sparse = idx;
        }
        if (idx < max_size) {
            return sparse_elements[idx];
        }
        throw std::runtime_error("Impossibly large object id encountered accessing ObjTable");
        return element(0); // doesn't return
    }

    inline T const&
    element(size_t idx) const
    {
        static const size_t max_size = elements.max_size();
        if (idx < elements.size()) {
            return elements[idx];
        }
        if (idx < max_size) {
            return sparse_elements.at(idx);
        }
        throw std::runtime_error("Impossibly large object id encountered accessing ObjTable");
        return element(0); // doesn't return
    }

    // Must only be called by emplace_back. Separated out from emplace_back to keep inlined code
    // tight.
    template <class... Args>
    T&
    emplace_back_large(Args&&... args)
    {
        auto it = sparse_elements.begin();
        auto end = sparse_elements.end();
        while (it != end && it->first == elements.size()) {
            elements.emplace_back(std::move(it->second));
            it = sparse_elements.erase(it);
        }
        min_sparse = (it == end) ? std::numeric_limits<size_t>::max() : it->first;
        return elements.emplace_back(std::forward<Args&&...>(args...));
    }

    std::vector<T> elements;
    std::map<size_t, T> sparse_elements;
    // Smallest id present in sparse_elements.
    size_t min_sparse{std::numeric_limits<size_t>::max()};
};

#endif // OBJTABLE_HH
