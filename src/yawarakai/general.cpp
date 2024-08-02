module;
#include <cassert>

module yawarakai;
import std;

using namespace std::literals;

namespace yawarakai {

Environment::Environment() {
    auto [s, _] = heap.allocate<Scope>();
    curr_scope = s;
    global_scope = s;

    setup_scope_for_builtins(*this);
}

const Sexp* Environment::lookup_binding(const Symbol& name) const {
    Scope* curr = curr_scope;
    while (curr) {
        auto iter = curr->bindings.find(&name);
        if (iter != curr->bindings.end()) {
            return &iter->second;
        }

        curr = curr->prev.get();
    }
    return nullptr;
}

void Environment::set_binding(const Symbol& name, Sexp value) {
    Scope* curr = curr_scope;
    while (curr) {
        auto iter = curr->bindings.find(&name);
        if (iter != curr->bindings.end()) {
            iter->second = value;
            return;
        }

        curr = curr->prev.get();
    }
}

Sexp cons(Sexp a, Sexp b, Environment& env) {
    auto [addr, _] = env.heap.allocate<ConsCell>(std::move(a), std::move(b));
    return Sexp(addr);
}

void cons_inplace(Sexp a, Sexp& list, Environment& env) {
    auto [addr, _] = env.heap.allocate<ConsCell>(std::move(a), std::move(list));
    list = Sexp(addr);
}

Sexp car(Sexp s) {
    auto cons_cell = s.as_ptr<ConsCell>();
    if (cons_cell == nullptr)
        throw EvalException("car(): argument is not not a cons");
    return cons_cell->car;
}

Sexp cdr(Sexp s) {
    auto cons_cell = s.as_ptr<ConsCell>();
    if (cons_cell == nullptr)
        throw EvalException("cdr(): argument is not not a cons");
    return cons_cell->cdr;
}

Sexp list_nth_elm(Sexp list, int idx, Environment& env) {
    Sexp* curr = &list;
    int n_to_go = idx + 1;
    while (curr->is_ptr()) {
        auto& cons_cell = *curr->as_ptr<ConsCell>();
        n_to_go -= 1;
        if (n_to_go == 0)
            break;
        curr = &cons_cell.cdr;
    }

    if (n_to_go != 0) {
        throw EvalException("list_nth_elm(): index out of bounds"s);
    }
    return car(*curr);
}

void list_get_prefix(Sexp list, std::initializer_list<Sexp*> out_prefix, Sexp* out_rest, Environment& env) {
    Sexp* curr = &list;
    auto it = out_prefix.begin();
    while (curr->is_ptr()) {
        auto& cons_cell = *curr->as_ptr<ConsCell>();
        **it = cons_cell.car;
        curr = &cons_cell.cdr;
        if (++it == out_prefix.end())
            break;
    }
    if (it != out_prefix.end())
        throw EvalException("list_get_prefix(): too few elements in list"s);
    if (out_rest)
        *out_rest = *curr;
}

void list_get_everything(Sexp list, std::initializer_list<Sexp*> out, Environment& env) {
    Sexp rest;
    list_get_prefix(list, out, &rest, env);

    if (!rest.is_nil())
        throw EvalException("list_get_everything(): too many elements in list"s);
}

UserProc* make_user_proc(Sexp param_decl, Sexp body_decl, Environment& env) {
    std::vector<const Symbol*> proc_args;
    for (Sexp param : iterate(param_decl, env)) {
        if (!param.is_symbol())
            throw EvalException("proc parameter must be a symbol"s);
        proc_args.push_back(&param.as_symbol());
    }

    if (!is_list(body_decl))
        throw EvalException("proc body must have 1 or more forms"s);

    auto [proc, _] = env.heap.allocate_only<UserProc>();
    new (proc) UserProc{
        .closure_frame = HeapPtr(env.curr_scope),
        .arguments = std::move(proc_args),
        .body = body_decl.as_ptr<ConsCell>(),
    };

    return proc;
}

class SexpParser {
public:
    /* ---- Inputs ---- */
    /* Initalize them with aggregate initilization, and then call parse() */
    Environment* env;
    std::string_view src;

private:
    /* ---- State Variables ---- */
    /// A path of every list we have to visit to get to `curr`
    /// For example, suppose we are parsing the following source, and the cursor is denoted by '|':
    ///     (define (a b)
    ///       (my-func a |b))
    /// `curr` points to the the cdr of th e ConsCell `(a . '())`.
    /// To push some `Sexp s` into the current list, just set curr to a new ConsCell `(s . '())`, and then set `curr` to its cdr (same logic as `yawarakai::cons_inplace()`).
    std::vector<Sexp*> path;
    Sexp* curr;
    /// If not null, the next sexp `x` produced by the parser loop shall be rewritten as `(wrapper x)`
    const Symbol* next_sexp_wrapper = nullptr;
    size_t cursor;

public:
    // Defined out of line to reduce indentation
    Sexp parse();

private:
    Sexp* push_sexp(Sexp val) {
        // Pointer to the `val` moved to the heap
        Sexp* p_val = nullptr;

        if (next_sexp_wrapper != nullptr) {
            // Rolling the logic of make_list_v() manually here to keep a pointer to `val`
            // i.e. let s = cons1[next_sexp_wrapper cons2[val nil]]
            auto [cons1, _] = env->heap.allocate<ConsCell>();
            // WORKAROUND(msvc): no support for P2169 "Placeholder variables with no name" yet
            auto [cons2, __] = env->heap.allocate<ConsCell>();
            cons1->car = Sexp(*next_sexp_wrapper);
            cons1->cdr = Sexp(cons2);
            cons2->car = val;
            cons2->cdr = Sexp();

            p_val = &cons2->car;
            val = Sexp(cons1);
        }

        auto [the_cons, _] = env->heap.allocate<ConsCell>();
        the_cons->car = val;
        *curr = Sexp(the_cons);

        if (p_val == nullptr)
            p_val = &the_cons->car;
        curr = &the_cons->cdr;

        next_sexp_wrapper = nullptr;
        return p_val;
    }

    void enter_nesting() {
        // The nil is our nested list
        // Sexp wrapping is taken care by push_sexp() automatically
        Sexp* car = push_sexp(Sexp());
        Sexp* cdr = curr;
        path.push_back(cdr);
        curr = car;
    }

    bool leave_nesting() {
        if (path.empty())
            return false;

        curr = path.back();
        path.pop_back();
        return true;
    }

    static bool is_token_separator(char c) {
        return std::isspace(c) || c == '(' || c == ')';
    }

    std::string_view take_token() {
        size_t begin = cursor;
        while (cursor < src.length() && !is_token_separator(src[cursor]))
            cursor += 1;
        size_t end = cursor;

        const char* d = src.data();
        return std::string_view(d + begin, d + end);
    }

    void skip_until(char c) {
        while (cursor < src.length() && src[cursor] != c)
            cursor += 1;
    }
};

Sexp SexpParser::parse() {
    this->path = {};
    this->curr = {};
    this->cursor = 0;
    this->next_sexp_wrapper = nullptr;

    // Synthesized a top-level list, so we can pretend that every sexp in the source file is actually inside a giant list enclosing everything
    Sexp program;
    curr = &program;
    // We do not push into path, because it makes no sense to leave the synthesized top-level
    /*path.push_back(curr);*/

    auto& sym_quote = env->sym_pool.intern("quote");
    auto& sym_unquote = env->sym_pool.intern("unquote");
    auto& sym_quasiquote = env->sym_pool.intern("quasiquote");

    while (cursor < src.length()) {
        // Skip all whitespace
        // Token splitting is automatically handled by each case (it stops right on a whitespace character)
        if (std::isspace(src[cursor])) {
            ++cursor;
            continue;
        }

        if (src[cursor] == ';') {
            skip_until('\n');
            continue;
        }

#define CHECK_FOR_WRAP(literal, sym) \
    if (src[cursor] == literal) {    \
        next_sexp_wrapper = &(sym);  \
        cursor++;                    \
        continue;                    \
    }
        CHECK_FOR_WRAP('\'', sym_quote);
        CHECK_FOR_WRAP(',', sym_unquote);
        CHECK_FOR_WRAP('`', sym_quasiquote);
#undef CHECK_FOR_WRAP

        if (src[cursor] == '(') {
            enter_nesting();
            cursor += 1;
            continue;
        }
        if (src[cursor] == ')') {
            leave_nesting();
            cursor += 1;
            continue;
        }

        if (src[cursor] == '"') {
            cursor += 1;

            size_t str_size = 0;
            size_t str_begin = cursor;
            while (true) {
                // Break conditions
                if (cursor >= src.length())
                    throw ParseException("unexpected EOF while parsing string"s);
                if (src[cursor] == '"')
                    break;

                if (src[cursor] == '\\') {
                    str_size += 1;
                    cursor += 2;
                    continue;
                }

                // otherwise...
                str_size += 1;
                cursor += 1;
            }
            cursor += 1;

            auto [h_str, _] = env->heap.allocate<String>();
            auto& str = h_str->v;
            str.reserve(str_size);

            size_t i = str_begin;
            while (i < str_begin + str_size) {
                if (src[i] != '\\') {
                    str.push_back(src[i]);
                    i += 1;
                    continue;
                }

                char esc = src[i + 1];
                i += 2;
                switch (esc) {
                    case 'n': str.push_back('\n'); break;
                    case '\\': str.push_back('\\'); break;
                    default: throw ParseException(std::format("invalid escaped char '{}'", esc));
                }
            }

            push_sexp(Sexp(h_str));

            continue;
        }

        if (src[cursor] == '#') {
            cursor += 1;
            if (cursor >= src.length()) throw ParseException("unexpected EOF while parsing #-symbols"s);

            char next_c = src[cursor];
            cursor += 1;

            auto token = take_token();
            if (token == "t"sv) {
                push_sexp(Sexp(true));
                continue;
            }
            if (token == "f"sv) {
                push_sexp(Sexp(false));
                continue;
            }

            throw ParseException("invalid #-symbol"s);
        }

        auto token = take_token();

        // Try parse a number literal
        float v;
        auto [rest, ec] = std::from_chars(&src[cursor], src.data() + src.size(), v);
        if (ec == std::errc()) {
            // TODO proper Scheme numeric literal parsing
            if (auto n = static_cast<int32_t>(v); n == v)
                push_sexp(Sexp(n));
            else
                push_sexp(Sexp(v));

            cursor += rest - &src[cursor];
            continue;
        } else if (ec == std::errc::result_out_of_range) {
            throw ParseException("number literal out of range"s);
        } else if (ec == std::errc::invalid_argument) {
            // Not a number, continue
        }

        // Parse a symbol
        const Symbol& h_sym = env->sym_pool.intern(token);
        push_sexp(Sexp(h_sym));
    }

    return program;
}

Sexp parse_sexp(std::string_view src, Environment& env) {
    SexpParser parser;
    parser.env = &env;
    parser.src = src;

    return parser.parse();
}

template <typename T>
void dump_numerical_value(std::string& output, T v) {
    // TODO I have no idea why max_digits10 isn't big enough
    // constexpr auto BUF_SIZE = std::numeric_limits<T>::max_digits10;
    const auto BUF_SIZE = 32;
    char buf[BUF_SIZE];
    auto res = std::to_chars(buf, buf + BUF_SIZE, v);

    if (res.ec == std::errc()) {
        output += std::string_view(buf, res.ptr);
    } else {
        throw std::runtime_error("failed to format number with std::to_chars()"s);
    }
}

void dump_sexp_impl(std::string& output, Sexp sexp, Environment& env) {
    switch (sexp.get_flags()) {
        case SCVAL_FLAG_INT: {
            dump_numerical_value(output, sexp.as_int());
        } break;

        case SCVAL_FLAG_FLOAT: {
            dump_numerical_value(output, sexp.as_float());
        } break;

        case SCVAL_FLAG_BOOL: {
            auto v = sexp.as_bool();
            output += v ? "#t" : "#f";
        } break;

        case SCVAL_FLAG_SYMBOL: {
            auto& v = sexp.as_symbol();
            output += v;
        } break;

        case SCVAL_FLAG_PTR: {
            HeapPtr<void> ptr = sexp.as_ptr();

            // Support dumping empty lists
            // For non-empty lists, the terminating nil is automatically handled by SexpListIterator
            if (ptr == nullptr) {
                output += "'()";
                break;
            }

            switch (ptr.get_type()) {
                using enum ObjectType;

                case TYPE_UNKNOWN: {
                    output += "#UNKNOWN";
                } break;

                case TYPE_CONS_CELL: {
                    output += "(";
                    for (Sexp elm : iterate(ptr.get_as_unchecked<ConsCell>(), env)) {
                        dump_sexp_impl(output, elm, env);
                        output += " ";
                    }
                    output.pop_back(); // Remove the trailing space
                    output += ")";
                } break;

                case TYPE_STRING: {
                    auto& v = ptr.get_as_unchecked<String>()->v;
                    output += '"';
                    output += v;
                    output += '"';
                } break;

                case TYPE_USER_PROC: {
                    auto& v = *ptr.get_as_unchecked<UserProc>();
                    output += "#BUILTIN:";
                    output += *v.name;
                } break;

                case TYPE_BUILTIN_PROC: {
                    auto& v = *ptr.get_as_unchecked<BuiltinProc>();
                    if (v.name->empty()) {
                        // Unnamed proc, probably a lambda
                        output += "#PROC:<unnamed>";
                    } else {
                        output += "#PROC:";
                        output += *v.name;
                    }
                } break;

                case TYPE_CALL_FRAME: {
                    assert(false && "unimplemented");
                } break;
            }
        } break;
    }
}

std::string dump_sexp(Sexp sexp, Environment& env) {
    std::string result;
    dump_sexp_impl(result, sexp, env);
    return result;
}

} // namespace yawarakai
