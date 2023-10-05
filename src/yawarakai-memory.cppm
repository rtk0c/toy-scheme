module;
#include <cassert>

export module yawarakai:memory;

import std;
import std.compat;

namespace yawarakai {

/******** Forward declarations ********/
struct ConsCell;

export enum class ObjectType : uint16_t {
    TYPE_UNKNOWN = 0,
    TYPE_CONS_CELL = 1,
    TYPE_USER_PROC = 2,
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

        auto obj = new(obj_raw) T(std::forward<TArgs>(args)...);

        header->set_type(T::HEAP_OBJECT_TYPE);

        return { obj, header };
    }

    void walk_heap_objects(auto&& visitor) const {
        for (auto& hg : heap_segments) {
            auto curr = std::bit_cast<uintptr_t>(hg.arena + hg.arena_size);
            while (curr != hg.last_object) {
                curr -= sizeof(ObjectHeader);
                auto header = std::bit_cast<ObjectHeader*>(curr);
                size_t obj_size = header->get_size();
                size_t obj_align = header->get_alignment();

                auto obj_addr = shift_down_and_align(curr, obj_size, obj_align);
                auto obj = std::bit_cast<std::byte*>(obj_addr);
                curr = obj;

                switch (header->get_type()) {
                    using enum ObjectType;
                    case TYPE_UNKNOWN:
                        visitor(std::span<std::byte>(obj_addr, obj_size));
                        break;
                    case TYPE_CONS_CELL:
                        visitor(reinterpret_cast<ConsCell*>(obj_addr));
                        break;
                }
            }
        }
    }

private:
    void new_heap_segment();
};

}
