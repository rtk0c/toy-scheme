module;
#include <cassert>

export module toyscheme:memory;

import std;
import std.compat;

namespace toyscheme {

/******** Forward declarations ********/
struct ConsCell;
struct Scope;

export enum class ObjectType : uint16_t {
    TYPE_UNKNOWN,
    TYPE_CONS_CELL,
    TYPE_STRING,
    TYPE_USER_PROC,
    TYPE_BUILTIN_PROC,
    TYPE_CALL_FRAME,
};

export struct ObjectHeader {
    static constexpr int TRACKED_FLAG_BIT = 0;
    static constexpr int TRACKED_GC_MARK_BIT = 1;

    // TODO we should move the size as an extra allocation after the header, only for UNKNOWN heap objects
    uint8_t _size_p0, _size_p1, _size_p2, _size_p3;
    uint8_t _type_p0, _type_p1;
    uint8_t _align;
    uint8_t _flags;

    // https://godbolt.org/z/orKPGK4af
    // On platforms with unaligned memory access, these get optimized to just direct loads/stores

    bool is_flag_set(int flag_bit) const;
    void set_flag(int flag_bit, bool value);

    size_t _read_size() const;
    size_t get_size() const;
    void set_size(size_t size);

    size_t get_alignment() const;
    void set_alignment(size_t alignment);

    ObjectType get_type() const;
    void set_type(ObjectType type);
};

static_assert(sizeof(ObjectHeader) == sizeof(uint64_t));
static_assert(alignof(ObjectHeader) == 1);

struct HeapSegment {
    std::byte* arena;
    std::byte* last_object;
    size_t arena_size;
};

export template <typename T>
struct HeapPtr {
    T* ptr;

    // Allow implicit construction of null value
    /*implicit*/ HeapPtr()
        : ptr{ nullptr } {}

    explicit HeapPtr(T* ptr)
        : ptr{ ptr } {}

    operator bool() const { return ptr != nullptr; }

    // C++20: operator!= automatically generated
    bool operator==(std::nullptr_t) const { return ptr == nullptr; }

    T& operator*() const { return *ptr; }
    T* operator->() const { return ptr; }

    T* get() const { return ptr; }

    ObjectHeader* get_header() const {
        assert(ptr != nullptr);
        return reinterpret_cast<ObjectHeader*>(reinterpret_cast<std::byte*>(ptr) - sizeof(ObjectHeader));
    }

    ObjectType get_type() const {
        assert(ptr != nullptr);
        return get_header()->get_type();
    }
};

export template <>
struct HeapPtr<void> {
    void* ptr;

    constexpr /*implicit*/ HeapPtr()
        : ptr{ nullptr } {}

    template <typename T>
    constexpr explicit HeapPtr(T* ptr)
        : ptr{ static_cast<void*>(ptr) } {}

    template <typename T>
    constexpr /*implicit*/ HeapPtr(HeapPtr<T> ptr)
        : ptr{ static_cast<void*>(ptr.get()) } {}

    constexpr operator bool() const { return ptr != nullptr; }

    // C++20: operator!= automatically generated
    constexpr bool operator==(std::nullptr_t) const { return ptr == nullptr; }

    constexpr void* get() const { return ptr; }

    template <typename T>
    T* get_as() const {
        if (ptr == nullptr)
            return nullptr;
        if (get_type() != T::HEAP_OBJECT_TYPE)
            return nullptr;
        return reinterpret_cast<T*>(ptr);
    }

    template <typename T>
    T* get_as_unchecked() const {
        return reinterpret_cast<T*>(ptr);
    }

    template <typename T>
    HeapPtr<T> as() const {
        return HeapPtr<T>(get_as<T>());
    }

    template <typename T>
    HeapPtr<T> as_unchecked() const {
        return HeapPtr<T>(get_as_unchecked<T>());
    }

    ObjectHeader* get_header() const {
        assert(ptr != nullptr);
        return reinterpret_cast<ObjectHeader*>(reinterpret_cast<std::byte*>(ptr) - sizeof(ObjectHeader));
    }

    ObjectType get_type() const {
        assert(ptr != nullptr);
        return get_header()->get_type();
    }
};

export class Heap {
private:
    std::vector<HeapSegment> heap_segments;

public:
    Heap();
    ~Heap();

    std::pair<std::byte*, ObjectHeader*> allocate(size_t size, size_t alignment);

    template <typename T, typename... TArgs>
    std::pair<T*, ObjectHeader*> allocate(TArgs&&... args) {
        auto [obj_raw, header] = allocate(sizeof(T), alignof(T));
        auto obj = new (obj_raw) T(std::forward<TArgs>(args)...);
        header->set_type(T::HEAP_OBJECT_TYPE);
        return { obj, header };
    }

    template <typename T>
    std::pair<T*, ObjectHeader*> allocate_only() {
        auto [obj_raw, header] = allocate(sizeof(T), alignof(T));
        header->set_type(T::HEAP_OBJECT_TYPE);
        return { reinterpret_cast<T*>(obj_raw), header };
    }

    std::byte* find_object(ObjectHeader* header) const;
    ObjectHeader* find_header(std::byte* object) const;

    void walk_heap_objects(auto&& visitor) const {
        for (auto& hg : heap_segments) {
            auto curr = std::bit_cast<uintptr_t>(hg.last_object);
            auto end = std::bit_cast<uintptr_t>(hg.arena) + hg.arena_size;
            while (curr < end) {
                auto header = std::bit_cast<ObjectHeader*>(curr);
                curr += sizeof(ObjectHeader);
                size_t obj_size = header->get_size();
                size_t obj_align = header->get_alignment();

                auto obj = std::bit_cast<std::byte*>(curr);
                curr += obj_size;

                switch (header->get_type()) {
                    using enum ObjectType;
                    case TYPE_UNKNOWN:
                        visitor(std::span<std::byte>(obj, obj_size));
                        break;
                    case TYPE_CONS_CELL:
                        visitor(reinterpret_cast<ConsCell*>(obj));
                        break;
                    case TYPE_CALL_FRAME:
                        visitor(reinterpret_cast<Scope*>(obj));
                        break;
                    // TODO
                    // case TYPE_STRING:
                    //     visitor(reinterpret_cast<String*>(obj));
                    //     break;
                    // case TYPE_USER_PROC:
                    //     visitor(reinterpret_cast<String*>(obj));
                    //     break;
                    // case TYPE_BUILTIN_PROC:
                    //     visitor(reinterpret_cast<String*>(obj));
                    //     break;
                }
            }
        }
    }

private:
    void new_heap_segment();
};

} // namespace toyscheme
