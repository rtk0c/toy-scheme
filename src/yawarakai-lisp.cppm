module;
#include "yawarakai/util.hpp"

export module yawarakai:lisp;

import :memory;
import :util;

import std;

export namespace yawarakai {

/******** Forward declarations ********/
struct ConsCell;
struct UserProc;
struct BuiltinProc;
struct Environment;

struct ParseException {
    std::string msg;
};

struct EvalException {
    std::string msg;
};

using MemoryLocation = ConsCell*;

struct Nil {};
struct Symbol {
    std::string name;
};

/// Handles both atoms and lists (as references to heap allocated ConsCell's)
struct Sexp {
    // NOTE: keep types in std::variant and their corresponding TYPE_XXX indices in sync
    using Storage = std::variant<
        /*s-expression*/ Nil, double, bool, std::string, Symbol, MemoryLocation,
        /*transient*/ const BuiltinProc*, const UserProc*>;
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
    Sexp(const BuiltinProc& v) : _value{ &v } {}
    Sexp(const UserProc& v) : _value{ &v } {}

    Type get_type() const { return static_cast<Type>(_value.index()); }

    bool is_nil() const { return _value.index() == TYPE_NIL; }

    template <size_t N> auto get() { return *std::get_if<N>(&_value); }
    template <size_t N> auto get() const { return *std::get_if<N>(&_value); }

    template <typename T>
    bool is() const { return std::holds_alternative<T>(_value); }

    template <typename T>
    const T& as() const { return *std::get_if<T>(&_value); }

    template <typename T>
    T& as() { return const_cast<T&>(const_cast<const Sexp*>(this)->as<T>()); }
};

Sexp operator ""_sym(const char* str, size_t len) {
    return Sexp(Symbol(std::string(str, len)));
}

/// A heap allocated cons, with a car/left and cdr/right Sexp
struct ConsCell {
    static constexpr auto HEAP_OBJECT_TYPE = ObjectType::TYPE_CONS_CELL;

    Sexp car;
    Sexp cdr;
};

struct CallFrame {
    static constexpr auto HEAP_OBJECT_TYPE = ObjectType::TYPE_CALL_FRAME;

    /// The CallFrame in the "previous level" of closure
    CallFrame* prev;
    std::map<std::string, Sexp, std::less<>> bindings;
};

struct BuiltinProc {
    // NOTE: we don't bind parameters to names in a scope when evaluating builtin functions, instead just using the list directly
    using FnPtr = Sexp(*)(const Sexp&, Environment&);

    std::string_view name;
    FnPtr fn;
};

struct UserProc {
    static constexpr auto HEAP_OBJECT_TYPE = ObjectType::TYPE_USER_PROC;

    CallFrame* closure_frame;
    std::string name;
    std::vector<std::string> arguments;
    // NOTE: we could use Sexp here, but since the body is always a list, using MemoryLocation is just easier
    MemoryLocation body;
};

struct Environment {
    Heap heap;

    /// A stack of scopes, added as we call into functions and popped as we exit
    CallFrame* curr_scope;
    CallFrame* global_scope;

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

    const Sexp* lookup_binding(std::string_view name) const;
    void set_binding(std::string_view name, Sexp value);
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

void list_get_prefix(const Sexp& list, std::initializer_list<const Sexp**> out_prefix, const Sexp** out_rest, Environment& env);
void list_get_everything(const Sexp& list, std::initializer_list<const Sexp**> out, Environment& env);

UserProc* make_user_proc(const Sexp& param_decl, const Sexp& body_decl, Environment& env);

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

    SexpListIterator(ConsCell* cons, Environment& env)
        : curr{ cons }
        , env{ &env } {}

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

    bool is_end() const {
        return curr == nullptr;
    }
};

using SexpListIterable = Iterable<SexpListIterator, SexpListIterator>;

SexpListIterable iterate(ConsCell* cons, Environment& env) { return { SexpListIterator(cons, env) }; }
SexpListIterable iterate(const Sexp& s, Environment& env) { return { SexpListIterator(s, env) }; }

std::vector<Sexp> parse_sexp(std::string_view src, Environment& env);
std::string dump_sexp(const Sexp& sexp, Environment& env);

Sexp eval(const Sexp& sexp, Environment& env);

}
