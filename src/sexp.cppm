export module yawarakai;

import std;
import std.compat;

export namespace yawarakai {

/******** Forward declarations ********/

/// Handles both atoms and lists (as references to heap allocated ConsCell's)
struct Sexp;
/// A heap allocated cons, with a car/left and cdr/right Sexp
struct ConsCell;

using MemoryLocation = size_t;

struct Heap {
    std::vector<ConsCell> storage;

    Heap();

    MemoryLocation push(ConsCell cons);
    ConsCell& lookup(MemoryLocation addr);
};

struct Nil {};
struct Symbol {
    std::string v;
};

struct Sexp {
    // NOTE: keep types in std::variant and their corresponding TYPE_XXX indices in sync
    using Storage = std::variant<Nil, int64_t, double, std::string, Symbol, MemoryLocation>;
    enum Type { TYPE_NIL = 0, TYPE_INT = 1, TYPE_FLOAT = 2, TYPE_STRING = 3, TYPE_SYMBOL = 4, TYPE_REF = 5 };

    Storage _value;

    Type get_type() const { return static_cast<Type>(_value.index()); }
    void set_nil() { _value.emplace<Nil>(); }
    void set(Nil) { set_nil(); }
    void set(int64_t v) { _value = v; }
    void set(double v) { _value = v; }
    void set(std::string v) { _value = std::move(v); }
    void set(Symbol v) { _value = std::move(v); }
    void set(MemoryLocation v) { _value = std::move(v); }
};

struct ConsCell {
    Sexp car;
    Sexp cdr;
};

/// Constructs a ConsCell on heap, with car = a and cdr = b, and return a reference Sexp to it.
Sexp cons(Sexp a, Sexp b, Heap& heap);
void cons_inplace(Sexp a, Sexp& list, Heap& heap);

template <typename TFunction>
void traverse_list(const Sexp& list, Heap& heap, TFunction&& callback) {
    const Sexp* curr = &list;
    while (curr->get_type() == Sexp::TYPE_REF) {
        auto ref = std::get<MemoryLocation>(list._value);
        auto& cons_cell = heap.lookup(ref);
        callback(cons_cell.car);
        curr = &cons_cell.cdr;
    }
}

Sexp parse_sexp(std::string_view src, Heap& heap);
std::string dump_sexp(Sexp sexp, const Heap& heap);

}
