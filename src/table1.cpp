#include "tomos/tomos.hpp"
#include "tomos/util/simple_args.hpp"
using namespace tomo;

void usage(std::string program_name) {
    std::cout
        << "USAGE: " << program_name
        << " --out OUT_DIR\n(OUT_DIR should contain trailing forward slash)";
}

int main(int argc, char** argv) {
    using T = float;
    constexpr dimension D = 3_D;

    auto opts = tomo::options{argc, argv};
    if (!(opts.required_arguments({"--out"})) ||
        opts.arg("--out")[opts.arg("--out").size() - 1] != '/') {
        usage(argv[0]);
        return -1;
    }

    for (auto size : {64, 128}) {
        auto v = tomo::volume<D, T>(size);

        auto g = tomo::geometry::cone_beam<T>(v, size, {(T)2.0, (T)2.0},
                                              {size, size}, (T)5.5, (T)3.5);
        auto k = tomo::dim::closest<D, T>(v);

        int nzs = 0;
        for (auto [idx, line] : g) {
            (void)idx;
            for (auto elem : k(line)) {
                (void)elem;
                nzs++;
            }
        }

        std::ofstream of(opts.arg("--out") + "cone_beam_" +
                             std::to_string(size) + ".mtx",
                         std::ios::out);

        of << "%%MatrixMarket matrix coordinate real general\n";
        of << g.lines() << " " << v.cells() << " " << nzs << "\n";
        for (auto [idx, line] : g) {
            (void)idx;
            for (auto elem : k(line)) {
                of << idx + 1 << " " << elem.index + 1 << " 1.0\n";
            }
        }
    }
}
