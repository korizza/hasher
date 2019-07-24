
#include <iostream>
#include <chrono>
#include <boost/program_options.hpp>

#include "hasher.h"

#define DEFAULT_BLOCK_SIZE 512
#define DEFAULT_CALC_TREAD_NUM 4

namespace opt = boost::program_options;

int main(int argc, char** argv) {
    std::string in_file, out_file;
    size_t blk_size, thread_num;    
    
    try {
        opt::options_description desc{"Options"};
        desc.add_options()
            ("help,h", "This help")
            ("in,i", opt::value<std::string>(), "In file")
            ("out,o", opt::value<std::string>(), "Out file")
            ("blk,b", opt::value<size_t>()->default_value(DEFAULT_BLOCK_SIZE), "Block size")
            ("nthread,t", opt::value<size_t>()->default_value(DEFAULT_CALC_TREAD_NUM), "Thread number");
        
        opt::variables_map vm;
        store(parse_command_line(argc, argv, desc), vm);
        opt::notify(vm);
        
        if ((vm["in"].empty() && vm["out"].empty()) || vm.count("help")) {
            std::cout << desc << '\n';
            return 0;
        }
        
        in_file = vm["in"].as<std::string>();
        out_file = vm["out"].as<std::string>();
        blk_size = vm["blk"].as<size_t>();
        thread_num = vm["nthread"].as<size_t>();
    } catch (const opt::error &ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
    
    
    try {
        hs::hasher h{in_file, out_file, blk_size, thread_num};
        h.run();
    } catch (const std::exception &ex) {
        std::cerr << "An exception has occured: \"" << ex.what() << "\"\n";
        return 1;       
    } catch (...) {
        std::cerr << "An undefined exception has occured\n";
        return 1;
    }
   
    return 0;
}
