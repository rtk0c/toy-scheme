module yawarakai;

import std;
import std.compat;

using namespace std::literals;

namespace yawarakai {

Memory::Memory() {
    // The value at 0 is always nil
    push(/* nil */ Sexp());
}

MemoryLocation Memory::push(Sexp sexp) {
    storage.push_back(std::move(sexp));
    return storage.size() - 1;
}

Sexp::Sexp()
    : v_nil{}
{
}

Sexp::Sexp(const Sexp& that)
    : type{ that.type }
{
    switch (type) {
        case Int: v_int = that.v_int; break;
        case Float: v_float = that.v_float; break;
        case String: new(&v_str) auto(that.v_str); break;
        case Symbol: new(&v_symbol) auto(that.v_symbol); break;
        case Cons: new(&v_cons) auto(that.v_cons); break;
        case Nil: v_nil = {}; break;
    }
}

Sexp& Sexp::operator=(const Sexp& that) {
    set_nil();
    type = that.type;
    switch (that.type) {
        case Int: v_int = that.v_int; break;
        case Float: v_float = that.v_float; break;
        case String: new(&v_str) auto(that.v_str); break;
        case Symbol: new(&v_symbol) auto(that.v_symbol); break;
        case Cons: new(&v_cons) auto(that.v_cons); break;
        case Nil: v_nil = {}; break;
    }
    return *this;
}

Sexp::Sexp(Sexp&& that)
    : type{ that.type }
{
    switch (type) {
        case Int: v_int = that.v_int; break;
        case Float: v_float = that.v_float; break;
        case String: new(&v_str) auto(std::move(that.v_str)); break;
        case Symbol: new(&v_symbol) auto(std::move(that.v_symbol)); break;
        case Cons: new(&v_cons) auto(std::move(that.v_cons)); break;
        case Nil: v_nil = {}; break;
    }
}

Sexp& Sexp::operator=(Sexp&& that) {
    set_nil();
    type = that.type;
    switch (type) {
        case Int: v_int = that.v_int; break;
        case Float: v_float = that.v_float; break;
        case String: new(&v_str) auto(std::move(that.v_str)); break;
        case Symbol: new(&v_symbol) auto(std::move(that.v_symbol)); break;
        case Cons: new(&v_cons) auto(std::move(that.v_cons)); break;
        case Nil: v_nil = {}; break;
    }
    return *this;
}

Sexp::~Sexp() {
    set_nil();
}

void Sexp::set(std::string v) {
    set_nil();
    type = String;
    new(&v_str) auto(std::move(v));
}

void Sexp::set(int64_t v) {
    set_nil();
    type = Int;
    v_int = v;
}

void Sexp::set(double v) {
    set_nil();
    type = Float;
    v_float = v;
}

void Sexp::set(ConsCell v) {
    set_nil();
    type = String;
    new(&v_cons) auto(std::move(v));
}

void Sexp::set_nil() {
    // Clear previous value
    switch (type) {
        case Int:
        case Float:
            break;

        case String:
            v_str.~basic_string();
            break;

        case Symbol:
            v_symbol.~basic_string();
            break;

        case Cons:
            v_cons.~ConsCell();
            break;

        case Nil: break;
    }

    type = Nil;
    v_nil = {};
}

MemoryLocation parse_sexp(std::string_view src, Memory& heap) {
    struct ParserStackFrame {
        MemoryLocation the_list;
    };

    auto push_child_sexp = [&](ParserStackFrame& psf, MemoryLocation sexp) {
        Sexp new_head;
        new_head.set(ConsCell{ .car = sexp, .cdr = psf.the_list });

        MemoryLocation new_head_addr = heap.push(std::move(new_head));

        psf.the_list = new_head_addr;
    };

    // Parser Call Stack
    std::vector<ParserStackFrame> pcs;

    pcs.push_back(ParserStackFrame());

    size_t cursor = 0;

    while (cursor < src.length()) {
        if (auto c = src[cursor];
            c == ' ' || c == '\t' || c == '\n')
        {
            continue;
        }

        // TODO
        // if (src[cursor] == '\'') {
        //     pcs.push_back(ParserStackFrame());
        //     const auto& quoter = pcs.rbegin()[0];
        //     const auto& parent = pcs.rbegin()[1]
        //     parent.;
        //     cursor += 1;
        //     continue;
        // }

        if (src[cursor] == '(') {
            pcs.push_back(ParserStackFrame());

            cursor += 1;
            continue;
        }

        if (src[cursor] == ')') {
            if (pcs.size() == 1) {
                throw "Error: unbalanced parenthesis";
            }

            auto& curr = pcs.rbegin()[0];
            auto& parent = pcs.rbegin()[1];
            push_child_sexp(parent, curr.the_list);
            pcs.pop_back();

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
                    throw "Error: unexpected end of file while parsing string";
                if (src[cursor] != '"')
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

            Sexp sexp;
            sexp.set(std::move(str));

            MemoryLocation addr = heap.push(std::move(sexp));

            auto& parent = pcs.back();
            push_child_sexp(parent, addr);

            continue;
        }

        auto try_parse_number = [&]<typename T>() -> bool {
            T v;
            auto [rest, ec] = std::from_chars(&src[cursor], &*src.end(), v);
            if (ec == std::errc()) {
                Sexp sexp;
                sexp.set(v);

                MemoryLocation addr = heap.push(std::move(sexp));

                auto& parent = pcs.back();
                push_child_sexp(parent, addr);

                cursor += rest - &src[cursor];
                return true;
            } else if (ec == std::errc::result_out_of_range) {
                throw "Error: number literal out of range"s;
            } else if (ec == std::errc::invalid_argument) {
                // Not a number, let's continue
            }

            return false;
        };
        if (try_parse_number.template operator()<int64_t>()) continue;
        if (try_parse_number.template operator()<double>()) continue;

        {
            size_t symbol_size = 0;
            size_t symbol_begin = cursor;

            while (true) {
                if (cursor >= src.length())
                    break;
                if (src[cursor] != ' ')
                    break;

                symbol_size += 1;
                cursor += 1;
            }
            cursor += 1;

            std::string symbol(&src[symbol_begin], symbol_size);

            Sexp sexp;
            sexp.set(std::move(symbol));

            MemoryLocation addr = heap.push(std::move(sexp));

            auto& parent = pcs.back();
            push_child_sexp(parent, addr);
        }
    }

    return pcs[0].the_list;
}

std::string dump_sexp(MemoryLocation addr, const Memory& heap) {
    return ""s;
}

}
