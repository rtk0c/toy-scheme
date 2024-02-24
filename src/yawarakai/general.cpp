module;
#include <cassert>

// TODO this file should be of module `yawarakai:lisp`, but if we do that, nothing from the :lisp interface unit is available here -- xmake bug?
module yawarakai;

import std;

using namespace std::literals;

namespace yawarakai {

Environment::Environment() {
    auto [s, _] = heap.allocate<CallFrame>();
    curr_scope = s;
    global_scope = s;
}

const Sexp* Environment::lookup_binding(const Symbol& name) const {
    CallFrame* curr = curr_scope;
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
    CallFrame* curr = curr_scope;
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
    // TODO replace this with car() ?
    return curr->as_ptr<ConsCell>()->car;
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

std::vector<Sexp> parse_sexp(std::string_view src, Environment& env) {
    auto sym_quote = Sexp(env.sym_pool.intern("quote"));
    auto sym_unquote = Sexp(env.sym_pool.intern("unquote"));
    auto sym_quasiquote = Sexp(env.sym_pool.intern("quasiquote"));

    struct ParserStackFrame {
        std::vector<Sexp> children;
        const Sexp* wrapper = {};
    };
    std::vector<ParserStackFrame> cs;

    cs.push_back(ParserStackFrame());

    size_t cursor = 0;
    const Sexp* next_sexp_wrapper = nullptr;

    auto push_sexp_to_parent = [&](Sexp sexp) {
        auto& target = cs.back().children;
        if (next_sexp_wrapper) {
            // Turns (my-sexp la la la) into (<the wrapper> (my-sexp la la la))
            target.push_back(make_list_v(env, *next_sexp_wrapper, std::move(sexp)));
            next_sexp_wrapper = nullptr;
        } else {
            target.push_back(std::move(sexp));
        }
    };

    while (cursor < src.length()) {
        if (std::isspace(src[cursor])) {
            cursor += 1;
            continue;
        }

        // Eat comments
        if (src[cursor] == ';') {
            while (cursor < src.length() && src[cursor] != '\n')
                cursor += 1;
            continue;
        }

#define CHECK_FOR_WRAP(literal, sym) \
    if (src[cursor] == literal) {    \
        next_sexp_wrapper = &sym;    \
        cursor++;                    \
        continue;                    \
    }
        CHECK_FOR_WRAP('\'', sym_quote);
        CHECK_FOR_WRAP(',', sym_unquote);
        CHECK_FOR_WRAP('`', sym_quasiquote);
#undef CHECK_FOR_WRAP

        if (src[cursor] == '(') {
            ParserStackFrame psf;
            ;
            if (next_sexp_wrapper) {
                psf.wrapper = next_sexp_wrapper;
                next_sexp_wrapper = nullptr;
            }

            cs.push_back(std::move(psf));

            cursor += 1;
            continue;
        }

        if (src[cursor] == ')') {
            if (cs.size() == 1) {
                throw ParseException("unbalanced parenthesis"s);
            }

            ParserStackFrame& curr = cs.back();
            Sexp list;
            for (auto it = curr.children.rbegin(); it != curr.children.rend(); ++it) {
                cons_inplace(std::move(*it), list, env);
            }
            if (curr.wrapper)
                list = make_list_v(env, *curr.wrapper, std::move(list));
            cs.pop_back(); // Removes `curr`

            auto& parent = cs.back();
            parent.children.push_back(std::move(list));

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

            auto [h_str, _] = env.heap.allocate<String>();
            auto& str = h_str->v;
            str.reserve(str_size);

            size_t i = str_begin;
            while (i < str_begin + str_size) {
                if (src[i] == '\\') {
                    char esc = src[i + 1];
                    switch (esc) {
                        case 'n':
                            str.push_back('\n');
                            break;
                        case '\\':
                            str.push_back('\\');
                            break;
                        default: {
                            throw ParseException(std::format("invalid escaped char '{}'", esc));
                        } break;
                    }

                    i += 2;
                    continue;
                }

                str.push_back(src[i]);
                i += 1;
            }

            push_sexp_to_parent(Sexp(h_str));

            continue;
        }

        if (src[cursor] == '#') {
            cursor += 1;
            if (cursor >= src.length()) throw ParseException("unexpected EOF while parsing #-symbols"s);

            char next_c = src[cursor];
            cursor += 1;

            switch (next_c) {
                case 't':
                    push_sexp_to_parent(Sexp(true));
                    continue;
                case 'f':
                    push_sexp_to_parent(Sexp(false));
                    continue;
                case ':': break; // TODO keyword argument
                default: throw ParseException("invalid #-symbol"s);
            }
        }

        {
            float v;
            auto [rest, ec] = std::from_chars(&src[cursor], &*src.end(), v);
            if (ec == std::errc()) {
                // TODO proper Scheme numeric literal parsing
                if (auto n = static_cast<int32_t>(v); n == v)
                    push_sexp_to_parent(Sexp(n));
                else
                    push_sexp_to_parent(Sexp(v));

                cursor += rest - &src[cursor];
                continue;
            } else if (ec == std::errc::result_out_of_range) {
                throw ParseException("number literal out of range"s);
            } else if (ec == std::errc::invalid_argument) {
                // Not a number
            }
        }

        {
            size_t sym_size = 0;
            size_t sym_begin = cursor;

            while (true) {
                if (cursor >= src.length())
                    break;
                char c = src[cursor];
                if (std::isspace(c) || c == '(' || c == ')')
                    break;

                sym_size += 1;
                cursor += 1;
            }

            char next_c = src[cursor];
            if (std::isspace(next_c))
                cursor += 1;

            const Symbol& h_sym = env.sym_pool.intern(&src[sym_begin], sym_size);
            push_sexp_to_parent(Sexp(h_sym));
        }
    }

    // NB: this is not against NRVO: our return value is heap allocated anyways (in a std::vector), so we must use std::move
    return std::move(cs[0].children);
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
            switch (ptr.get_type()) {
                using enum ObjectType;

                case TYPE_UNKNOWN: {
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
                        output += "#PROC";
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