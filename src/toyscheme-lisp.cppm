module;
#include "toyscheme/util.hpp"
#include <cassert>

export module toyscheme:lisp;
import :memory;
import :util;
import std;

namespace toyscheme {
export struct ParseException {
    std::string msg;
};

export struct EvalException {
    std::string msg;
};

export class SymbolPool;
export class Symbol {
private:
    friend class SymbolPool;

    uintptr_t _data = 0;
    size_t _size = 0;

public:
    /// Constructs an empty symbol
    Symbol() = default;

    Symbol(const Symbol&) = delete;
    Symbol& operator=(const Symbol&) = delete;

    Symbol(Symbol&& s) noexcept
        : _data{ s._data }
        , _size{ s._size } //
    {
        s._data = 0;
        s._size = 0;
    }

    Symbol& operator=(Symbol&& s) noexcept {
        if ((_data & 0x1) == 0) {
            delete data();
        }
        this->_data = std::exchange(s._data, 0);
        this->_size = std::exchange(s._size, 0);
        return *this;
    }

    ~Symbol() {
        // NOTE: for _data == nullptr, delete is no-op, so considering that as heap-allocated is fine
        if ((_data & 0x1) == 0) {
            delete data();
        }
    }

    const char* data() const { return std::bit_cast<const char*>(_data & ~0x1); }
    size_t size() const { return _size; }

    operator std::string_view() const { return { data(), size() }; }

    bool empty() const { return _size == 0; }
};

export class SymbolPool {
private:
    // TODO custom hashtable
    std::unordered_map<std::string, Symbol, StringHash, std::equal_to<>> _pool;

public:
    // Constructor for string literals
    // This *technically* also accepts things like `const char arr[5];` - just don't do it
    template <size_t N>
    const Symbol& intern(const char (&str)[N]) {
        // Length of the char array from a literal contains the null terminator
        size_t actual_len = N - 1;
        auto& sym = _pool[std::string(str, actual_len)];
        // If this Symbol is default constructed, i.e. this is a new entry in the symbol pool
        if (sym.data() == nullptr) {
            // `str` is of type const char[N], we need a pointer for std::bit_cast
            const char* str_ptr = str;

            // Sanity check: the pointer is not using the lowest bit
            assert((std::bit_cast<uintptr_t>(str_ptr) & 0x1) == 0);

            // Set lowest bit, indicating this is a literal
            sym._data = std::bit_cast<uintptr_t>(str_ptr) | 0x1;
            sym._size = actual_len;
        }
        return sym;
    }

    // Constructor for runtime strings (make a copy)
    const Symbol& intern(const char* str, size_t len) {
        auto& sym = _pool[std::string(str, len)];
        if (sym.data() == nullptr) {
            char* data = new char[len + 1]{};
            sym._size = len;
            sym._data = std::bit_cast<uintptr_t>(data);
            // Sanity check: the pointer is not using the lowest bit
            assert((std::bit_cast<uintptr_t>(data) & 0x1) == 0);
            // Copy string content
            std::memcpy(data, str, len);
            // Null terminate
            data[len] = 0;
        }
        return sym;
    }

    // Constructor for runtime strings (make a copy)
    const Symbol& intern(std::string_view str) {
        return intern(str.data(), str.size());
    }
};

// All heap objects are 8-byte aligned
export constexpr uintptr_t SCVAL_MASK_FLAG = 0x0000'0000'0000'0007;

// 32 bit signed integer in the MSB
export constexpr unsigned int SCVAL_FLAG_INT = 0b000;

// 32 bit IEEE754 floating pointer number in the MSB
export constexpr unsigned int SCVAL_FLAG_FLOAT = 0b010;

// Bi-state value
export constexpr unsigned int SCVAL_FLAG_BOOL = 0b100;
export constexpr uintptr_t SCVAL_FALSE = 0x0000'0000'0000'0000 | SCVAL_FLAG_BOOL;
export constexpr uintptr_t SCVAL_TRUE = 0x0000'0000'0000'0010 | SCVAL_FLAG_BOOL;

// 32-bit unsigned integer in the MSB, storing the symbol ID
export constexpr unsigned int SCVAL_FLAG_SYMBOL = 0b110;

// 64-bit pointer with the lowest 3 bits assumed to be 0 (aligned to 8 byte boundries)
export constexpr unsigned int SCVAL_FLAG_PTR = 0b001;
// Empty list, special value for SCVAL_MASK_PTR
// All address bits are 0 and flag == SCVAL_MASK_PTR
export constexpr uintptr_t SCVAL_NIL = 0x0000'0000'0000'0000 | SCVAL_FLAG_PTR;

export struct Sexp {
    uintptr_t _value;

    [[nodiscard]] constexpr uint8_t get_flags() const {
        return _value & SCVAL_MASK_FLAG;
    }

    [[nodiscard]] constexpr bool is_numeric() const {
        return is_int() || is_float();
    }

    /******** Fixnum ********/

    [[nodiscard]] constexpr bool is_int() const {
        return get_flags() == SCVAL_FLAG_INT;
    }

    [[nodiscard]] constexpr int32_t as_int() const {
        assert(is_int());
        auto payload = static_cast<uint32_t>(_value >> 32);
        return std::bit_cast<int32_t>(payload);
    }

    constexpr explicit Sexp(int32_t v) { set_int(v); }

    constexpr void set_int(int32_t v) {
        auto payload = std::bit_cast<uint32_t>(v);
        _value = (static_cast<uint64_t>(payload) << 32) | SCVAL_FLAG_INT;
    }

    /******** Flonum ********/

    constexpr bool is_float() const { return get_flags() == SCVAL_FLAG_FLOAT; }

    constexpr float as_float() const {
        assert(is_float());
        auto payload = static_cast<uint32_t>(_value >> 32);
        return std::bit_cast<float>(payload);
    }

    constexpr explicit Sexp(float v) { set_float(v); }

    constexpr void set_float(float v) {
        auto payload = std::bit_cast<uint32_t>(v);
        _value = (static_cast<uint64_t>(payload) << 32) | SCVAL_FLAG_FLOAT;
    }

    /******** Boolean ********/

    constexpr bool is_bool() const { return get_flags() == SCVAL_FLAG_BOOL; }

    constexpr bool as_bool() const {
        assert(is_bool());
        return _value == SCVAL_TRUE;
    }

    constexpr bool evalute_bool() const {
        return is_bool() ? _value == SCVAL_TRUE : true;
    }

    // // Using a template + concept to prohibit pointer and integral implicit convdrsion to bool
    // template <std::same_as<bool> T>
    // explicit Sexp(T v) { set_bool(v); }
    constexpr explicit Sexp(bool v) { set_bool(v); }

    constexpr void set_bool(bool v) {
        _value = v ? SCVAL_TRUE : SCVAL_FALSE;
    }

    /******** Symbol ********/

    constexpr bool is_symbol() const { return get_flags() == SCVAL_FLAG_SYMBOL; }

    constexpr const Symbol& as_symbol() const {
        assert(is_symbol());
        return *std::bit_cast<const Symbol*>(_value & ~0b111);
    }

    constexpr explicit Sexp(const Symbol& sym) { set_symbol(sym); }

    constexpr void set_symbol(const Symbol& sym) {
        auto bits = std::bit_cast<uintptr_t>(&sym);
        assert((bits & SCVAL_MASK_FLAG) == 0);
        _value = bits | SCVAL_FLAG_SYMBOL;
    }

    /******** Heap pointer ********/

    constexpr bool is_nil() const { return _value == SCVAL_NIL; }

    constexpr bool is_ptr() const { return get_flags() == SCVAL_FLAG_PTR; }

    template <typename T>
    constexpr bool is_ptr() const {
        return is_ptr() && as_ptr().get_type() == T::HEAP_OBJECT_TYPE;
    }

    constexpr HeapPtr<void> as_ptr() const {
        assert(is_ptr());
        return HeapPtr(std::bit_cast<void*>(_value & ~0b111));
    }

    template <typename T>
    constexpr HeapPtr<T> as_ptr() const {
        return as_ptr().as<T>();
    }

    // nil constructor
    constexpr explicit Sexp()
        : _value{ SCVAL_NIL } {}

    // DO NOT REMOVE THIS CONSTRUCTOR
    // otherwise, all usages of Sexp(T*) is going to match the Sexp(bool), which is totally wrong!
    template <typename T>
    constexpr explicit Sexp(T* v) { set_pointer(HeapPtr<void>(v)); }

    // Handles Sexp(HeapPtr<void>)
    // Handles Sexp(HeapPtr<T>) by the implicit conversion operator
    constexpr explicit Sexp(HeapPtr<void> v) { set_pointer(v); }

    constexpr void set_pointer(HeapPtr<void> v) {
        auto bits = std::bit_cast<uintptr_t>(v.get());
        assert((bits & SCVAL_MASK_FLAG) == 0);
        _value = bits | SCVAL_FLAG_PTR;
    }
};

export struct Environment {
    Heap heap;
    SymbolPool sym_pool;

    /// A stack of scopes, added as we call into functions and popped as we exit
    Scope* curr_scope;
    Scope* global_scope;

    Environment();

    const Sexp* lookup_binding(const Symbol& name) const;
    void set_binding(const Symbol& name, Sexp value);
};

/// A heap allocated cons, with a car/left and cdr/right Sexp
export struct ConsCell {
    static constexpr auto HEAP_OBJECT_TYPE = ObjectType::TYPE_CONS_CELL;

    Sexp car;
    Sexp cdr;
};

export struct String {
    static constexpr auto HEAP_OBJECT_TYPE = ObjectType::TYPE_STRING;

    std::string v;
};

export struct UserProc {
    static constexpr auto HEAP_OBJECT_TYPE = ObjectType::TYPE_USER_PROC;

    const Symbol* name;
    HeapPtr<Scope> closure_frame;
    std::vector<const Symbol*> arguments;
    // NOTE: we could use Sexp here, but since the body is always a list, pointing directly to ConsCell is just easier
    HeapPtr<ConsCell> body;
};

export struct BuiltinProc {
    static constexpr auto HEAP_OBJECT_TYPE = ObjectType::TYPE_BUILTIN_PROC;

    // NOTE: we don't bind parameters to names in a scope when evaluating builtin functions, instead just using the list directly
    using FnPtr = Sexp (*)(Sexp args, Environment& env);

    const Symbol* name;
    FnPtr fn;
};

export struct Scope {
    static constexpr auto HEAP_OBJECT_TYPE = ObjectType::TYPE_CALL_FRAME;

    /// The CallFrame in the "previous level" of closure
    HeapPtr<Scope> prev;
    std::unordered_map<const Symbol*, Sexp> bindings;
};

/// Constructs a ConsCell on heap, with car = a and cdr = b, and return a reference Sexp to it.
export Sexp cons(Sexp a, Sexp b, Environment& env);
export void cons_inplace(Sexp a, Sexp& list, Environment& env);

// NB: we use varadic template for perfect forwarding, std::initializer_list forces us to make copies
export template <typename... Ts>
Sexp make_list_v(Environment& env, Ts&&... sexps) {
    Sexp the_list;
    FOLD_ITER_BACKWARDS(cons_inplace(std::forward<Ts>(sexps), the_list, env));
    return the_list;
}

export template <typename TIter, typename TSentinel>
Sexp make_list(TIter&& iter, TSentinel&& sentinel, Environment& env) {
    // TODO use compact list optimization here
    Sexp lst;
    for (; iter != sentinel; ++iter) {
        cons_inplace(*iter, lst, env);
    }
    return lst;
}

// Returns true if its cdr is a cons cell pointer, e.g. (1 . ()) or (1 . (2 . ()))
// Returns false otherwise, such as (1 . 2)
export bool is_list(const ConsCell& cons) {
    return cons.cdr.is_ptr();
}

// Same as is_list(ConsCell) but ensures the Sexp is a ConsCell
export bool is_list(Sexp s) {
    if (!s.is_ptr()) return false;
    return is_list(*s.as_ptr().as<ConsCell>());
}

export Sexp car(Sexp s);
export Sexp cdr(Sexp s);
export Sexp list_nth_elm(Sexp list, int idx, Environment& env);

export void list_get_prefix(Sexp list, std::initializer_list<Sexp*> out_prefix, Sexp* out_rest, Environment& env);
export void list_get_everything(Sexp list, std::initializer_list<Sexp*> out, Environment& env);

export UserProc* make_user_proc(Sexp param_decl, Sexp body_decl, Environment& env);

export struct SexpListSentinel {};
export struct SexpListIterator {
    using Sentinel = SexpListSentinel;

    ConsCell* curr;
    Environment* env;

    static ConsCell* calc_next(Sexp s, Environment& env) {
        if (s.is_ptr()) {
            return s.as_ptr().get_as<ConsCell>();
        }
        return nullptr;
    }

    SexpListIterator(ConsCell* cons, Environment& env)
        : curr{ cons }
        , env{ &env } {}

    SexpListIterator(Sexp s, Environment& env)
        : curr{ calc_next(s, env) }
        , env{ &env } {}

    Sexp& operator*() const {
        return curr->car;
    }

    SexpListIterator& operator++() {
        curr = calc_next(curr->cdr, *env);
        return *this;
    }

    bool operator==(SexpListSentinel) const {
        return curr == nullptr;
    }

    bool is_end() const {
        return curr == nullptr;
    }
};
export using SexpListIterable = Iterable<SexpListIterator, SexpListIterator>;

export SexpListIterable iterate(ConsCell* cons, Environment& env) {
    return { SexpListIterator(cons, env) };
}
export SexpListIterable iterate(Sexp s, Environment& env) {
    return { SexpListIterator(s, env) };
}

export Sexp parse_sexp(std::string_view src, Environment& env);
export std::string dump_sexp(Sexp sexp, Environment& env);

export Sexp call_user_proc(const UserProc& proc, Sexp params, Environment& env);

void setup_scope_for_builtins(Environment& env);

/// Implements (eval)
export Sexp eval(Sexp sexp, Environment& env);

/// If `forms` isd a list, given to eval_many();
/// If `forms` is not a list, given to eval().
export Sexp eval_maybe_many(Sexp forms, Environment& env);
/// Implements (progn): evalute each element in `forms`, and return the result of the last one.
export Sexp eval_many(ConsCell* forms, Environment& env);

} // namespace toyscheme
