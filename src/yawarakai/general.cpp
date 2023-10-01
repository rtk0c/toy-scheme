// TODO this file should be of module `yawarakai:lisp`, but if we do that, nothing from the :lisp interface unit is available here -- xmake bug?
module yawarakai;

import std;

using namespace std::literals;

namespace yawarakai {

Environment::Environment() {
    scopes.emplace_back();
}

MemoryLocation Environment::push(ConsCell cons) {
    storage.push_back(std::move(cons));
    return storage.size() - 1;
}

const ConsCell& Environment::lookup(MemoryLocation addr) const {
    return storage[addr];
}

ConsCell& Environment::lookup(MemoryLocation addr) {
    return storage[addr];
}

const Sexp* Environment::lookup_binding(std::string_view name) const {
    for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
        auto& scope = *it;
        auto iter = scope.bindings.find(name);
        if (iter != scope.bindings.end()) {
            return &iter->second;
        }
    }
    return nullptr;
}

Sexp cons(Sexp a, Sexp b, Environment& env) {
    auto addr = env.push(ConsCell{ std::move(a), std::move(b) });
    return Sexp(addr);
}

void cons_inplace(Sexp a, Sexp& list, Environment& env) {
    auto addr = env.push(ConsCell{ std::move(a), std::move(list) });
    list = Sexp(addr);
}

bool is_list(const ConsCell& cons) {
    auto type = cons.cdr.get_type();
    return type == Sexp::TYPE_NIL || type == Sexp::TYPE_REF;
}

const Sexp& car(const Sexp& the_cons, Environment& env) {
    auto ref = the_cons.as<MemoryLocation>();
    return env.lookup(ref).car;
}

const Sexp& cdr(const Sexp& the_cons, Environment& env) {
    auto ref = the_cons.as<MemoryLocation>();
    return env.lookup(ref).cdr;
}

const Sexp& list_nth_elm(const Sexp& list, int idx, Environment& env) {
    const Sexp* curr = &list;
    int n_to_go = idx + 1;
    while (curr->get_type() == Sexp::TYPE_REF) {
        auto& cons_cell = env.lookup(curr->as<MemoryLocation>());
        n_to_go -= 1;
        if (n_to_go == 0)
            break;
        curr = &cons_cell.cdr;
    }

    if (n_to_go != 0) {
        throw "Index out of bounds";
    }
    return car(*curr, env);
}

void list_get_prefix(const Sexp& list, std::initializer_list<const Sexp**> out_prefix, const Sexp** out_rest, Environment& env) {
    const Sexp* curr = &list;
    auto it = out_prefix.begin();
    while (curr->get_type() == Sexp::TYPE_REF) {
        auto& cons_cell = env.lookup(curr->as<MemoryLocation>());
        **it = &cons_cell.car;
        curr = &cons_cell.cdr;
        if (++it == out_prefix.end())
            break;
    }
    if (out_rest)
        *out_rest = curr;
}

std::vector<Sexp> parse_sexp(std::string_view src, Environment& env) {
    struct ParserStackFrame {
        std::vector<Sexp> children;
        const Sexp* wrapper = nullptr;
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

#define CHECK_FOR_WRAP(literal, sym) if (src[cursor] == literal) { next_sexp_wrapper = &sym; cursor++; continue; }
        CHECK_FOR_WRAP('\'', env.sym.quote);
        CHECK_FOR_WRAP(',', env.sym.unquote);
        CHECK_FOR_WRAP('`', env.sym.quasiquote);
#undef CHECK_FOR_WRAP

        if (src[cursor] == '(') {
            ParserStackFrame psf;;
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
                throw "Error: unbalanced parenthesis";
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
                    throw "Error: unexpected EOF while parsing string";
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

            std::string str;
            str.reserve(str_size);

            size_t i = str_begin;
            while (i < str_begin + str_size) {
                if (src[i] == '\\') {
                    char esc = src[i + 1];
                    switch (esc) {
                        case 'n': str.push_back('\n');
                        case '\\': str.push_back('\\');
                        default: {
                            throw std::format("Invalid escaped char '{}'", esc);
                        } break;
                    }

                    i += 2;
                    continue;
                }

                str.push_back(src[i]);
                i += 1;
            }

            push_sexp_to_parent(
                Sexp(std::move(str))
            );

            continue;
        }

        if (src[cursor] == '#') {
            cursor += 1;
            if (cursor >= src.length()) throw "Error: unexpected EOF while parsing #-symbols";

            char next_c = src[cursor];
            cursor += 1;

            switch (next_c) {
                case 't': push_sexp_to_parent(Sexp(true)); continue;
                case 'f': push_sexp_to_parent(Sexp(false)); continue;
                case ':': break; // TODO keyword argument
                default: throw "Error: invalid #-symbol";
            }
        }

        {
            double v;
            auto [rest, ec] = std::from_chars(&src[cursor], &*src.end(), v);
            if (ec == std::errc()) {
                push_sexp_to_parent(Sexp(v));

                cursor += rest - &src[cursor];
                continue;
            } else if (ec == std::errc::result_out_of_range) {
                throw "Error: number literal out of range"s;
            } else if (ec == std::errc::invalid_argument) {
                // Not a number
            }
        }

        {
            size_t symbol_size = 0;
            size_t symbol_begin = cursor;

            while (true) {
                if (cursor >= src.length())
                    break;
                char c = src[cursor];
                if (std::isspace(c) || c == '(' || c == ')')
                    break;

                symbol_size += 1;
                cursor += 1;
            }

            char next_c = src[cursor];
            if (std::isspace(next_c))
                cursor += 1;

            push_sexp_to_parent(
                Sexp(Symbol(std::string(&src[symbol_begin], symbol_size)))
            );
        }
    }

    // NB: this is not against NRVO: our return value is heap allocated anyways (in a std::vector), so we must use std::move
    return std::move(cs[0].children);
}

void dump_sexp_impl(std::string& output, const Sexp& sexp, Environment& env) {
    switch (sexp.get_type()) {
        using enum Sexp::Type;

        case TYPE_NIL: {
            output += "()";
        } break;

        case TYPE_NUM: {
            auto v = sexp.as<double>();

            // TODO I have no idea why max_digits10 isn't big enough
            //constexpr auto BUF_SIZE = std::numeric_limits<double>::max_digits10;
            const auto BUF_SIZE = 32;
            char buf[BUF_SIZE];
            auto res = std::to_chars(buf, buf + BUF_SIZE, v);

            if (res.ec == std::errc()) {
                output += std::string_view(buf, res.ptr);
            } else {
                throw "Error formatting number.";
            }
        } break;

        case TYPE_BOOL: {
            auto v = sexp.as<bool>();
            output += v ? "#t" : "#f";
        } break;

        case TYPE_STRING: {
            auto& v = sexp.as<std::string>();

            output += '"';
            output += v;
            output += '"';
        } break;

        case TYPE_SYMBOL: {
            auto& v = sexp.as<Symbol>();

            output += v.name;
        } break;

        case TYPE_REF: {
            output += "(";
            for (const Sexp& elm : iterate(sexp, env)) {
                dump_sexp_impl(output, elm, env);
                output += " ";
            }
            output.pop_back(); // Remove the trailing space
            output += ")";
        } break;

        case TYPE_BUILTIN_PROC: {
            auto& v = *sexp.get<TYPE_BUILTIN_PROC>();
            output += "#BUILTIN:";
            output += v.name;
        } break;

        case TYPE_USER_PROC: {
            auto& v = *sexp.get<TYPE_USER_PROC>();
            output += "#PROC:";
            output += v.name;
        } break;
    }
}

std::string dump_sexp(const Sexp& sexp, Environment& env) {
    std::string result;
    dump_sexp_impl(result, sexp, env);
    return result;
}

}
