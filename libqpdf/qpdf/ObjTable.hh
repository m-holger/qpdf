#ifndef OBJTABLE_HH
#define OBJTABLE_HH

#include <qpdf/QPDFObjGen.hh>
#include <qpdf/QPDFObjectHandle.hh>

#include "qpdf/QIntC.hh"
#include <limits>

// A table of objects indexed by object id. This is intended as a more efficient replacement for
// std::map<QPDFObjGen, T> containers.
//
// The table is implemented as a std::vector, with the object id implicitly represented by the index
// of the object. This has a number of implications, including:
// - operations that change the index of existing elements such as insertion and deletions are not
//   permitted.
// - operations that extend the table may invalidate iterators and references to objects.
//
// The provided overloads of the access operator[] are safe. For out of bounds access they will
// either extend the table or throw a runtime error.
//
// ObjTable has a map 'sparse_elements' to deal with very sparse / extremely large object tables
// (usually as the result of invalid dangling references). This map may contain objects not found in
// the xref table of the original pdf if there are dangling references with an id significantly
// larger than the largest valid object id found in original pdf.

template <class T>
class ObjTable: public std::vector<T>
{
  public:
    using reference = T&;
    ObjTable() = default;
    ObjTable(const ObjTable&) = delete;
    ObjTable(ObjTable&&) = delete;
    ObjTable& operator[](const ObjTable&) = delete;
    ObjTable& operator[](ObjTable&&) = delete;

    // Remove unchecked access.
    T& operator[](unsigned long idx) = delete;
    T const& operator[](unsigned long idx) const = delete;

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
        return idx < std::vector<T>::size() || sparse_elements.count(idx);
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
        if (id < std::vector<T>::size()) {
            return &std::vector<T>::operator[](id);
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
        if (id < std::vector<T>::size()) {
            return &std::vector<T>::operator[](id);
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
        if (min_sparse == std::vector<T>::size()) {
            return emplace_back_large(std::forward<Args&&...>(args...));
        }
        return std::vector<T>::emplace_back(std::forward<Args&&...>(args...));
    }

    // Try to emplace an element to the end of the vector. If there is a conflicting (non-default)
    // sparse element, emplace it to the end of the vector instead. Return a reference to the
    // emplaced element and true if a new element has been inserted.
    template <class... Args>
    inline std::pair<T&, bool>
    try_emplace_back(Args&&... args)
    {
        if (min_sparse == std::vector<T>::size()) {
            auto it = sparse_elements.begin();

            bool insert = it->second == T();
            auto& result = !insert ? std::vector<T>::emplace_back(std::move(it->second))
                                   : std::vector<T>::emplace_back(std::forward<Args&&...>(args...));
            it = sparse_elements.erase(it);
            min_sparse =
                it == sparse_elements.end() ? std::numeric_limits<size_t>::max() : it->first;
            return {result, insert};
        }
        return {std::vector<T>::emplace_back(std::forward<Args&&...>(args...)), true};
    }

    void
    resize(size_t a_size)
    {
        std::vector<T>::resize(a_size);
        if (a_size > min_sparse) {
            auto it = sparse_elements.begin();
            auto end = sparse_elements.end();
            while (it != end && it->first < a_size) {
                std::vector<T>::operator[](it->first) = std::move(it->second);
                it = sparse_elements.erase(it);
            }
            min_sparse = (it == end) ? std::numeric_limits<size_t>::max() : it->first;
        }
    }

    inline void
    forEach(std::function<void(int, T const&)> fn) const
    {
        int i = 0;
        for (auto const& item: *this) {
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
        for (auto& item: *this) {
            fn(i++, item);
        }
        for (auto& [id, item]: sparse_elements) {
            fn(QIntC::to_int(id), item);
        }
    }

  private:
    std::map<size_t, T> sparse_elements;
    // Smallest id present in sparse_elements.
    size_t min_sparse{std::numeric_limits<size_t>::max()};

    inline T&
    element(size_t idx)
    {
        if (idx < std::vector<T>::size()) {
            return std::vector<T>::operator[](idx);
        }
        return large_element(idx);
    }

    // Must only be called by element. Separated out from element to keep inlined code tight.
    T&
    large_element(size_t idx)
    {
        static const size_t max_size = std::vector<T>::max_size();
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
        static const size_t max_size = std::vector<T>::max_size();
        if (idx < std::vector<T>::size()) {
            return std::vector<T>::operator[](idx);
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
        while (it != end && it->first == std::vector<T>::size()) {
            std::vector<T>::emplace_back(std::move(it->second));
            it = sparse_elements.erase(it);
        }
        min_sparse = (it == end) ? std::numeric_limits<size_t>::max() : it->first;
        return std::vector<T>::emplace_back(std::forward<Args&&...>(args...));
    }
};

#endif // OBJTABLE_HH
