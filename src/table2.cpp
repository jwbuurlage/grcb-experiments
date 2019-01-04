#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "fmt/format.h"

#include "tomos/tomos.hpp"
#include "tomos/util/simple_args.hpp"
#include "tomos/util/trees.hpp"

std::mutex g_result_mutex;

using T = float;
constexpr tomo::dimension D = 3_D;

void partition(std::string meta_file, std::string output_file, int processors,
               T epsilon, bool output,
               tomo::util::report& table) {
    int k = tomo::math::min(512, processors * 4);
    auto problem = tomo::read_configuration<3_D, T>(meta_file, k);

    // now partition using problem.acquisition_geometry and
    // problem.object_volume
    auto tree = tomo::distributed::partition_bisection<D, T>(
        *problem.acquisition_geometry, problem.object_volume, processors,
        epsilon);
    auto neutral = tomo::to_neutral_tree<T>(tree, problem.object_volume);

    // then save to:
    tomo::serialize_tree(neutral, output_file);
    std::cout << "Saved: " << output_file << "\n";

    auto large_tree =
        tomo::from_neutral_tree<T>(neutral, problem.object_volume);

    if (output) {
        std::lock_guard<std::mutex> guard(g_result_mutex);

        auto part_bisected = bulk::tree_partitioning<D>(
            tomo::math::vec_to_array<D, int>(problem.object_volume.voxels()),
            processors, std::move(large_tree));

        auto part_trivial = tomo::distributed::partition_trivial(
            *problem.acquisition_geometry, problem.object_volume, processors);

        // Store result to table
        auto overlap_trivial = tomo::distributed::communication_volume<D, T>(
            *problem.acquisition_geometry, problem.object_volume, part_trivial);
        auto overlap_bisected = tomo::distributed::communication_volume<D, T>(
            *problem.acquisition_geometry, problem.object_volume,
            part_bisected);
        T imp = (T)0.0;
        if (overlap_trivial != 0)
            imp = (overlap_trivial - overlap_bisected) / (T)overlap_trivial;

        auto imbalance = tomo::distributed::load_imbalance(
            problem.object_volume, part_bisected,
            *problem.acquisition_geometry);

        auto name = fs::path(meta_file).stem();
        table.add_row(name);
        table.add_result(name, "trivial", overlap_trivial);
        table.add_result(name, "binary", overlap_bisected);
        table.add_result(name, "improvement",
                         fmt::format("{:.1f}%", 100 * imp));
        table.add_result(name, "imbalance", fmt::format("{:.2f}", imbalance));
        std::cout << "It is actually: " << overlap_bisected << "\n";
    }
}

void usage(std::string program_name) {
    std::cout << "USAGE: " << program_name << " --in GEOMS --out OUT_DIR [-p "
                                              "PROCS] [-e EPS] "
                                              "[--output]\n";
}

int main(int argc, char* argv[]) {
    auto opts = tomo::options{argc, argv};

    std::vector<std::pair<std::string, std::string>> ins_and_outs;
    int processors = 16;
    T epsilon = 0.05;

    // the usage should be:
    // ./partition -i geometry_meta_file -o output_partitioning -p procs -e
    // epsilon
    if (!(opts.required_arguments({"-i", "-o"}) ||
          opts.required_arguments({"--in", "--out"}))) {
        usage(argv[0]);
        return -1;
    }

    if (opts.required_arguments({"-i", "-o"})) {
        ins_and_outs.push_back(std::make_pair(opts.arg("-i"), opts.arg("-o")));
    } else if (opts.required_arguments({"--in", "--out"})) {
        auto ins = opts.args("--in");
        auto out_dir = opts.arg("--out");
        auto outs = ins;
        std::transform(outs.begin(), outs.end(), outs.begin(),
                       [&](std::string in) {
                           auto stem = fs::path(in).stem();
                           return out_dir + stem.string() + ".bsp";
                       });

        ins_and_outs = tomo::zip(ins, outs);
    }

    if (opts.passed("-p")) {
        processors = opts.arg_as<int>("-p");
    }
    if (opts.passed("-e")) {
        epsilon = opts.arg_as<T>("-e");
    }

    auto table = tomo::util::report(
        "Table 2: Communication volumes", "geometry");
    table.add_column("trivial");
    table.add_column("binary");
    table.add_column("improvement");
    table.add_column("imbalance");

    std::vector<std::thread> threads;
    for (auto in_and_out : ins_and_outs) {
        threads.emplace_back(partition, in_and_out.first, in_and_out.second,
                             processors, epsilon,
                             opts.passed("--output"), std::ref(table));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    if (opts.passed("--output")) {
        table.print();

        std::ofstream of(opts.arg("--out") + "volumes.md", std::ios::out);
        table.print(of);
    }

    return 0;
}
