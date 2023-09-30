export module yawarakai:lisp;

import :util;

import std;

#include "yawarakai/util.hpp"

export namespace yawarakai {

/******** Forward declarations ********/
struct Sexp;
struct ConsCell;
struct UserProc;
struct BuiltinProc;
struct Environment;

using MemoryLocation = size_t;

struct Nil {};
struct Symbol {
    std::string name;
};

/// Handles both atoms and lists (as references to heap allocated ConsCell's)
struct Sexp {
    // NOTE: keep types in std::variant and their corresponding TYPE_XXX indices in sync
    using Storage = std::variant<
        /*s-expression*/ Nil, double, bool, std::string, Symbol, MemoryLocation,
        /*transient*/ BuiltinProc*, UserProc*>;
    enum Type {
        // s-expression
        TYPE_NIL, TYPE_NUM, TYPE_BOOL, TYPE_STRING, TYPE_SYMBOL, TYPE_REF,
        // transient
        TYPE_BUILTIN_PROC, TYPE_USER_PROC,
    };

    // NOTE: visible, but not intended to be touched!
    //       Sexp should act like a custom union type, it just happens to be implemented with std::variant
    Storage _value;

    Sexp() : _value{ Nil() } {}
    Sexp(double v) : _value{ v } {}
    Sexp(bool v) : _value{ v } {}
    Sexp(std::string v) : _value{ std::move(v) } {}
    Sexp(Symbol v) : _value{ std::move(v) } {}
    Sexp(MemoryLocation v) : _value{ std::move(v) } {}

    Type get_type() const { return static_cast<Type>(_value.index()); }

    template <typename T>
    bool is() const { return std::holds_alternative<T>(_value); }
    
    template <typename T>
    const T& as() const { return *std::get_if<T>(&_value); }

    template <typename T>
    T& as() { return const_cast<T&>(const_cast<const Sexp*>(this)->as<T>()); }

    template <typename T>
    const T& as_or_error(std::string_view err_msg) const {
        if (const T* ptr = std::get_if<T>(&_value)) {
            return *ptr;
        } else {
            throw err_msg;
        }
    }
    
    template <typename T>
    T& as_or_error(std::string_view err_msg) { return const_cast<T&>(const_cast<const Sexp*>(this)->as_or_error<T>(err_msg)); }
};

Sexp operator ""_sym(const char* str, size_t len) {
    return Sexp(Symbol(std::string(str, len)));
}

/// A heap allocated cons, with a car/left and cdr/right Sexp
struct ConsCell {
    Sexp car;
    Sexp cdr;
};

struct BuiltinProc {
    // NOTE: we don't bind parameters to names in a scope when evaluating builtin functions, instead just using the list directly
    using FnPtr = Sexp(*)(const Sexp&, Environment&);

    std::string_view name;
    FnPtr fn;
};

struct UserProc {
    std::string name;
    std::vector<std::string> arguments;
    MemoryLocation body;
};

struct LexcialScope {
    std::map<std::string, Sexp> bindings;
};

struct Environment {
    std::vector<ConsCell> storage;
    
    /// A stack of scopes, added as we call into functions and popped as we exit
    /// `scopes[0]` is always the global scope
    std::vector<LexcialScope> scopes;

    // A collection of canonical symbols
    struct {
        Sexp define = "define"_sym;
        Sexp op_add = "+"_sym;
        Sexp op_sub = "-"_sym;
        Sexp op_mul = "*"_sym;
        Sexp op_div = "/"_sym;
        Sexp if_ = "if"_sym;
        Sexp quote = "quote"_sym;
        Sexp unquote = "unquote"_sym;
        Sexp quasiquote = "quasiquote"_sym;
    } sym;

    Environment();

    MemoryLocation push(ConsCell cons);
    const ConsCell& lookup(MemoryLocation addr) const;
    ConsCell& lookup(MemoryLocation addr);
};

/// Constructs a ConsCell on heap, with car = a and cdr = b, and return a reference Sexp to it.
Sexp cons(Sexp a, Sexp b, Environment& env);
void cons_inplace(Sexp a, Sexp& list, Environment& env);

// NB: we use varadic template for perfect forwarding, std::initializer_list forces us to make copies
template <typename... Ts>
Sexp make_list_v(Environment& env, Ts&&... sexps) {
    Sexp the_list;
    FOLD_ITER_BACKWARDS(cons_inplace(std::forward<Ts>(sexps), the_list, env));
    return the_list;
}

bool is_list(const ConsCell& cons);

const Sexp& car(const Sexp& the_cons, Environment& env);
const Sexp& cdr(const Sexp& the_cons, Environment& env);
const Sexp& list_nth_elm(const Sexp& list, int idx, Environment& env);

void list_get_prefix(const Sexp& list, std::initializer_list<const Sexp**> out, Environment& env);

struct SexpListIterator {
    using Sentinel = CommonSentinel;

    ConsCell* curr;
    Environment* env;

    static ConsCell* calc_next(const Sexp& s, Environment& env) {
        if (s.get_type() == Sexp::TYPE_REF) {
            return &env.lookup(s.as<MemoryLocation>());
        } else {
            return nullptr;
        }
    }

    SexpListIterator(const Sexp& s, Environment& env)
        : curr{ calc_next(s, env) }
        , env{ &env } {}

    Sexp& operator*() const {
        return curr->car;
    }

    SexpListIterator& operator++() {
        curr = calc_next(curr->cdr, *env);
        return *this;
    }

    bool operator==(CommonSentinel) const {
        return curr == nullptr;
    }

};

auto iterate(const Sexp& s, Environment& env) {
    return Iterable<SexpListIterator, SexpListIterator>(SexpListIterator(s, env));
}

std::vector<Sexp> parse_sexp(std::string_view src, Environment& env);
std::string dump_sexp(const Sexp& sexp, Environment& env);

Sexp eval(const Sexp& sexp, Environment& env);

}
