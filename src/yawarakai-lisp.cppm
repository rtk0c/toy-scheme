export module yawarakai:lisp;

import std;

#include "yawarakai/util.hpp"

export namespace yawarakai {

/******** Forward declarations ********/
struct Sexp;
struct ConsCell;
struct Heap;

using MemoryLocation = size_t;

struct Nil {};
struct Symbol {
    std::string name;
};

/// Handles both atoms and lists (as references to heap allocated ConsCell's)
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

    void set_nil() { _value = Nil(); }
    void set(Nil) { set_nil(); }
    void set(double v) { _value = v; }
    void set(std::string v) { _value = std::move(v); }
    void set_symbol(std::string name) { _value = Symbol(std::move(name)); }
    void set(Symbol v) { _value = std::move(v); }
    void set(MemoryLocation v) { _value = std::move(v); }
};

Sexp operator ""_sym(const char* str, size_t len) {
    Sexp sexp;
    sexp.set_symbol(std::string(str, len));
    return sexp;
}

/// A heap allocated cons, with a car/left and cdr/right Sexp
struct ConsCell {
    Sexp car;
    Sexp cdr;
};

struct Heap {
    std::vector<ConsCell> storage;

    // A collection of canonical symbols
    struct {
        Sexp define = "define"_sym;
        Sexp quote = "quote"_sym;
        Sexp unquote = "unquote"_sym;
        Sexp quasiquote = "quasiquote"_sym;
    } sym;

    Heap();

    MemoryLocation push(ConsCell cons);
    const ConsCell& lookup(MemoryLocation addr) const;
    ConsCell& lookup(MemoryLocation addr);
};

/// Constructs a ConsCell on heap, with car = a and cdr = b, and return a reference Sexp to it.
Sexp cons(Sexp a, Sexp b, Heap& heap);
void cons_inplace(Sexp a, Sexp& list, Heap& heap);

// NB: we use varadic template for perfect forwarding, std::initializer_list forces us to make copies
template <typename... Ts>
Sexp make_list_v(Heap& heap, Ts&&... sexps) {
    Sexp the_list;
    FOLD_ITER_BACKWARDS(cons_inplace(std::forward<Ts>(sexps), the_list, heap));
    return the_list;
}

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
