export module yawarakai;

import std;

export namespace yawakarai {

// Forward declarations
struct Sexp;




using MemoryLocation = size_t;
constexpr MemoryLocation NIL = 0;

struct Memory {
    std::vector<Sexp> storage;

    Memory();

    MemoryLocation push(Sexp sexp);
};

struct ConsCell {
    MemoryLocation car;
    MemoryLocation cdr;
};

struct Sexp {
    union {
        int64_t v_int;
        double v_float;
        std::string v_str;
        std::string v_symbol;
        ConsCell v_cons;
        struct {} v_nil;
    };

    enum Type : unsigned char {
        Int,
        Float,
        String,
        Symbol,
        Cons,
        Nil,
    } type;

    Sexp();
    ~Sexp();

    void set(std::string v);
    void set(int64_t v);
    void set(double v);
    void set(ConsCell v);
    void set_nil();
};

Sexp parse_sexp(std::string_view src, Memory& heap);
std::string dump_sexp(MemoryLocation addr, const Memory& heap);

}
