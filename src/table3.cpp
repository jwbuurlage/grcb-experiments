#include <cmath>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

using namespace std::string_literals;

#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;

#include "fmt/format.h"

#include "tomos/tomos.hpp"
#include "tomos/util/simple_args.hpp"
#include "tomos/util/trees.hpp"

namespace td = tomo::distributed;

using T = float;

std::mutex g_result_mutex;

void compute(std::string geometry_file, tomo::util::report& table) {
    auto name = fs::path(geometry_file).stem().string();
    table.add_row(name + " V_cube"s);

    int k = 256;
    auto problem = tomo::read_configuration<3_D, T>(geometry_file, k);
    auto& geometry = *problem.acquisition_geometry;
    auto obj_vol = problem.object_volume;

    auto part_cube = bulk::block_partitioning<3_D, 3_D>(
        tomo::math::vec_to_array<3_D, int>(obj_vol.voxels()), {4, 4, 4});
    auto cube_vol =
        td::communication_volume<3_D, T>(geometry, obj_vol, part_cube);

    {
        std::lock_guard<std::mutex> guard(g_result_mutex);
        table.add_result(name + " V_cube"s, "p = 64", cube_vol);
    }
}

void usage(std::string program_name) {
    std::cout << "USAGE: " << program_name << " --in GEOMS\n";
}

int main(int argc, char* argv[]) {
    auto opts = tomo::options{argc, argv};
    if (!(opts.required_arguments({"--in"}))) {
        usage(argv[0]);
        return -1;
    }

    auto geoms = opts.args("--in");

    auto table =
        tomo::util::report("Table 3: Cube partitioning volumes", "geometry");
    table.add_column("p = 64");

    std::vector<std::thread> threads;
    for (auto i = 0u; i < geoms.size(); ++i) {
        std::cout << geoms[i] << "\n";
        threads.emplace_back(compute, geoms[i], std::ref(table));
    }

    for (auto& thread : threads) {
        thread.join();
    }

    table.print();

    return 0;
}
