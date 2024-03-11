import std;
import yawarakai;

namespace fs = std::filesystem;
using namespace std::literals;
using namespace yawarakai;

using Task = std::variant<fs::path, std::string>;
namespace TaskType { constexpr int FILE = 0, LITERAL = 1; };

struct ProgramOptions {
    std::vector<Task> tasks;
    bool parse_only = false;
};

ProgramOptions parse_args(int argc, char** argv) {
    ProgramOptions res;

    // Either no arguments (argc == 1) or something broke and our own executable name is not passed (argc == 0)
    if (argc <= 1)
        return res;

    bool positional_only = false;
    bool accept_str_input = false;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);

        if (positional_only)
            goto handle_positional_arg;

        if (arg == "--parse-only"sv) {
            res.parse_only = true;
            continue;
        }
        if (arg == "--exec"sv || arg == "-e"sv) {
            accept_str_input = true;
            continue;
        }
        if (arg == "--"sv) {
            positional_only = true;
            continue;
        }

    handle_positional_arg:
        if (accept_str_input) {
            accept_str_input = false;
            res.tasks.push_back(std::string(arg));
        } else {
            res.tasks.push_back(fs::path(arg));
        }
    }

    return res;
}

void run_buffer(std::string_view buffer, const ProgramOptions& opts, Environment& env) {
    Sexp program;
    try {
        program = parse_sexp(buffer, env);
    } catch (const ParseException& e) {
        std::cerr << "Parsing exception: " << e.msg << '\n';
        return;
    }

    for (auto& sexp : iterate(program, env)) {
        try {
            if (opts.parse_only) {
                std::cout << dump_sexp(sexp, env) << std::endl;
            } else {
                auto res = eval(sexp, env);
                std::cout << dump_sexp(res, env) << std::endl;
            }
        } catch (const EvalException& e) {
            std::cerr << "Eval exception: " << e.msg << std::endl;
        } catch (const std::runtime_error& e) {
            std::cerr << "Internal error: " << e.what() << std::endl;
        }
    }
}

int main(int argc, char** argv) {
    auto opts = parse_args(argc, argv);

    Environment env;
    for (auto& task : opts.tasks) {
        switch (task.index()) {
            case TaskType::FILE: {
                auto& input_file = *std::get_if<TaskType::FILE>(&task);

                if (input_file.empty()) {
                    std::cerr << "Supply an input file to run it.\n";
                    return -1;
                }

                std::ifstream ifs(input_file);
                if (!ifs) {
                    std::cerr << "Unable to open input file.\n";
                    return -1;
                }

                std::stringstream buffer;
                buffer << ifs.rdbuf();

                run_buffer(buffer.view(), opts, env);
            } break;

            case TaskType::LITERAL: {
                auto& input = *std::get_if<TaskType::LITERAL>(&task);

                run_buffer(input, opts, env);
            } break;
        }
    }

    return 0;
}
