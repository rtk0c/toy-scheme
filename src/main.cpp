import std;
import yawarakai;

namespace fs = std::filesystem;
using namespace std::literals;

struct ProgramOptions {
    fs::path input_file;
    bool parse_only = false;
};

ProgramOptions parse_args(int argc, char** argv) {
    ProgramOptions res;

    // Either no arguments (argc == 1) or something broke and our own executable name is not passed (argc == 0)
    if (argc <= 1)
        return res;

    int nth_pos_arg = 0;
    bool positional_only = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (positional_only)
            goto handle_positional_arg;

        if (arg == "--parse-only"sv) {
            res.parse_only = true;
            continue;
        }
        if (arg == "--"sv) {
            positional_only = true;
            continue;
        }

    handle_positional_arg:
        switch (nth_pos_arg++) {
            case 0: res.input_file = arg; break;
            default: break;
        }
    }

    return res;
}

int main(int argc, char** argv) {
    auto opts = parse_args(argc, argv);
    if (opts.input_file.empty()) {
        std::cerr << "Supply an input file to run it.\n";
        return -1;
    }

    std::ifstream ifs(opts.input_file);
    if (!ifs) {
        std::cerr << "Unable to open input file.\n";
        return -1;
    }

    std::stringstream buffer;
    buffer << ifs.rdbuf();

    yawarakai::Environment env;
    auto sexps = yawarakai::parse_sexp(buffer.view(), env);
    for (auto& sexp : sexps) {
        if (opts.parse_only) {
            std::cout << yawarakai::dump_sexp(sexp, env) << '\n';
        } else {
            auto res = yawarakai::eval(sexp, env);
            std::cout << yawarakai::dump_sexp(res, env) << '\n';
        }
    }

    return 0;
}
