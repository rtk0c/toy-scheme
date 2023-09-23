import std;
import std.compat;
import yawarakai;

namespace fs = std::filesystem;
using namespace std::literals;

int main(int argc, char** argv) {
    if (argc > 1) {
        fs::path input_file(argv[1]);
        
        std::ifstream ifs(input_file);
        if (!ifs) {
            std::cerr << "Unable to open input file.\n";
            return -1;
        }

        ifs.seekg(0, std::ios::end);
        size_t input_size = ifs.tellg();

        auto buf = std::make_unique<char[]>(input_size);
        ifs.seekg(0);
        ifs.read(buf.get(), input_size);

        std::string_view input(buf.get(), input_size);
        
        yawarakai::Heap heap;
        auto sexps = yawarakai::parse_sexp(input, heap);
        for (auto& sexp : sexps) {
            std::cout << yawarakai::dump_sexp(sexp, heap) << '\n';
        }

        return 0;
    }

    std::cerr << "Supply an input file to run it.\n";
    return 0;
}
