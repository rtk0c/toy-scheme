module;
#include <cassert>

module yawarakai;

namespace yawarakai {

bool ObjectHeader::is_flag_set(int flag_bit) const {
    assert(flag_bit >= 0 && flag_bit < 8);
    return static_cast<bool>((_flags >> flag_bit) & 1);
}

void ObjectHeader::set_flag(int flag_bit, bool value) {
    assert(flag_bit >= 0 && flag_bit < 8);
    if (value) {
        _flags |= 1 << flag_bit;
    } else {
        _flags &= ~(1 << flag_bit);
    }
}

size_t ObjectHeader::_read_size() const {
    return (_size_p3 << 24) | (_size_p2 << 16) | (_size_p1 << 8) | _size_p0;
}

size_t ObjectHeader::get_size() const {
    switch (get_type()) {
        using enum ObjectType;
        case TYPE_UNKNOWN: return _read_size();
        case TYPE_CONS_CELL: return sizeof(ConsCell);
        case TYPE_CALL_FRAME: return sizeof(Scope);
    }
    return 0;
}

void ObjectHeader::set_size(size_t size) {
    assert(size <= std::numeric_limits<uint32_t>::max());
    _size_p0 = size & 0xFF;
    _size_p1 = (size >> 8) & 0xFF;
    _size_p2 = (size >> 16) & 0xFF;
    _size_p3 = (size >> 24) & 0xFF;
}

size_t ObjectHeader::get_alignment() const {
    switch (get_type()) {
        using enum ObjectType;
        case TYPE_UNKNOWN: return _align;
        case TYPE_CONS_CELL: return alignof(ConsCell);
        case TYPE_CALL_FRAME: return alignof(Scope);
    }
    return 0;
}

void ObjectHeader::set_alignment(size_t alignment) {
    assert(alignment <= std::numeric_limits<uint8_t>::max());
    _align = static_cast<uint8_t>(alignment);
}

ObjectType ObjectHeader::get_type() const {
    auto n = (_type_p1 << 8) | _type_p0;
    return static_cast<ObjectType>(n);
}

void ObjectHeader::set_type(ObjectType type) {
    auto n = std::to_underlying(type);
    _type_p0 = n & 0xFF;
    _type_p1 = (n >> 8) & 0xFF;
}

constexpr size_t HEAP_SEGMENT_SIZE = 32 * 1024;

Heap::Heap() {
    new_heap_segment();
}

Heap::~Heap() {
    for (auto& hg : heap_segments) {
        std::free(hg.arena);
    }
}

std::pair<std::byte*, ObjectHeader*> Heap::allocate(size_t size, size_t alignment) {
    // We only support types that aligns to 64-bit word boundraries
    // because Sexp uses pointer tagging with the lowest 3 bits
    assert(alignment == alignof(void*));

    auto& hg = heap_segments.back();

    auto start = std::bit_cast<uintptr_t>(hg.last_object);
    uintptr_t raw = shift_down_and_align(start, size, alignment);
    // N.B. no need to align because ObjectHeader has alignment of 1
    uintptr_t raw_header = raw - sizeof(ObjectHeader);

    if (raw_header < std::bit_cast<uintptr_t>(hg.arena)) {
        // We ran out of space
        new_heap_segment();
        return allocate(size, alignment);
    }

    auto new_obj_header = std::bit_cast<std::byte*>(raw_header);
    auto new_obj = std::bit_cast<std::byte*>(raw);
    hg.last_object = new_obj_header;

    // Padding members initialized to 0 automatically
    auto h = new (new_obj_header) ObjectHeader{};
    h->set_size(size);
    h->set_alignment(alignment);
    h->set_type(ObjectType::TYPE_UNKNOWN);

    return { new_obj, h };
}

std::byte* Heap::find_object(ObjectHeader* header) const {
    return reinterpret_cast<std::byte*>(header) + sizeof(ObjectHeader);
}

ObjectHeader* Heap::find_header(std::byte* object) const {
    return reinterpret_cast<ObjectHeader*>(object - sizeof(ObjectHeader));
}

void Heap::new_heap_segment() {
    auto& hg = heap_segments.emplace_back();
    hg.arena = static_cast<std::byte*>(std::malloc(HEAP_SEGMENT_SIZE));
    hg.last_object = hg.arena + HEAP_SEGMENT_SIZE;
    hg.arena_size = HEAP_SEGMENT_SIZE;
}

}
