export module yawarakai:sexp;

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
    const ConsCell& lookup(MemoryLocation addr) const;
    ConsCell& lookup(MemoryLocation addr);
};

struct Nil {};
struct Symbol {
    std::string name;
};

struct Sexp {
    // NOTE: keep types in std::variant and their corresponding TYPE_XXX indices in sync
    using Storage = std::variant<Nil, double, std::string, Symbol, MemoryLocation>;
    enum Type { TYPE_NIL = 0, TYPE_NUM = 1, TYPE_STRING = 2, TYPE_SYMBOL = 3, TYPE_REF = 4 };

    Storage _value;

    Type get_type() const { return static_cast<Type>(_value.index()); }

    template <typename T>
    bool is() const { return std::holds_alternative<T>(_value); }
    
    template <typename T>
    T& as() { return std::get<T>(_value); }
    template <typename T>
    const T& as() const { return std::get<T>(_value); }

    void set_nil() { _value.emplace<Nil>(); }
    void set(Nil) { set_nil(); }
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

bool is_list(const ConsCell& cons);

template <typename THeap, typename TFunction>
void traverse_list(const Sexp& list, THeap&& heap, TFunction&& callback) {
    const Sexp* curr = &list;
    while (curr->get_type() == Sexp::TYPE_REF) {
        auto ref = curr->as<MemoryLocation>();
        auto& cons_cell = heap.lookup(ref);
        callback(cons_cell.car);
        curr = &cons_cell.cdr;
    }
}

std::vector<Sexp> parse_sexp(std::string_view src, Heap& heap);
std::string dump_sexp(const Sexp& sexp, const Heap& heap);

}
