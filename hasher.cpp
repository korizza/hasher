
#include <iostream>
#include <functional>
#include <chrono>
#include <iomanip>

#include "hasher.h"

namespace hs {

#define TRY_ALL try

#define CATCH_ALL                                                                   \
            catch (std::exception &ex) {                                            \
                std::cerr << "An exception has occured: \"" << ex.what() << "\"\n"; \
                m_need_stop.store(true);                                            \
            } catch ( ... ) {                                                       \
                std::cerr << "An undefined exception has occured\n";                \
                m_need_stop.store(true);                                            \
            }


hasher::hasher(const std::string &in_file, const std::string &out_file, size_t blk_size, std::size_t calc_thread_num) 
        : m_in_filename(in_file),
        m_out_filename(out_file),
        m_blk_size(blk_size),
        m_calc_thread_num(calc_thread_num),
        m_manager_loop(1),
        m_calc_pool(calc_thread_num), 
        m_write_loop(1)
{
}

hasher::~hasher()
{
}

// this worker function run in the single loop
void hasher::manager_worker(job_list_ptr_t jobs)
{
    if (m_need_stop.load()) {
        return;
    }
    
    m_manager_calc_cntr.store(0);
    unsigned int num_jobs = jobs->size();
    
    // calculating hashes in the calculating pool
    for (auto &blk : *jobs) {
        boost::asio::post(m_calc_pool, std::bind(&hasher::calc_worker, this, blk));
    }
    
    // wait until all the jobs in the list are done
    while ( (m_manager_calc_cntr.load() != num_jobs) && !m_need_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // send results to the writer loop
    for (auto &blk : *jobs) {
        boost::asio::post(m_write_loop, std::bind(&hasher::write_worker, this, blk));
    }        
}

void hasher::calc_worker(data_blk_ptr_t blk)
{
    if (m_need_stop.load()) {
        return;
    }

    TRY_ALL {
        // getting crc32
        boost::crc_32_type result;
        result.process_bytes(blk->data.get(), blk->size);
        blk->crc = result.checksum();
    } CATCH_ALL;   
    
    ++m_manager_calc_cntr;
}

void hasher::write_worker(data_blk_ptr_t blk)
{
    if (m_need_stop.load()) {
        return;
    }
    
    TRY_ALL {
//        m_out_file.write(blk->data.get(), blk->size);
        m_out_file << std::hex << std::setw(8) << std::setfill('0') << blk->crc << '\n';
//        m_out_file.write(reinterpret_cast<char*>(&blk->crc), sizeof(int));
    } CATCH_ALL;    
}

void hasher::process_hashing(job_list_ptr_t jobs)
{
    if (jobs->empty()) {
        return;
    }
    boost::asio::post(m_manager_loop, std::bind(&hasher::manager_worker, this, jobs));
}

inline data_blk_ptr_t create_blk(std::shared_ptr<char> data, size_t size)
{
    data_blk_ptr_t blk = std::make_shared<data_blk_t>();
    blk->data = data;
    blk->size = size;
    return blk;    
}

inline std::shared_ptr<char> alloc_data_buffer(size_t size)
{
    // TODO: it would be nice using some fixed-size lock-free memory pool here
    return std::shared_ptr<char>(new char [size], [](char *buffer){delete [] buffer;});
}

void hasher::run()
{
    auto start_time = std::chrono::steady_clock::now();
    
    m_need_stop.store(false);    
    
    // opening files
    m_in_file.open(m_in_filename);
    if (!m_in_file.is_open()) {
        std::cerr << "Cannot open file " << m_in_filename;
        return;
    }    
    m_out_file.open(m_out_filename);
    if (!m_out_file.is_open()) {
        std::cerr << "Cannot open file " << m_out_filename;
        m_in_file.close();
        return;
    }
    
    std::cout << "Hashing started with block size: " << m_blk_size << ", on " << m_calc_thread_num << " threads\n";
    // reading and processing data
    bool done_hashing = false;
    for (;;) {
        job_list_ptr_t jobs = std::make_shared<job_list_t>();       
        
        while (!m_need_stop.load()) {
            if (jobs->size() == m_calc_thread_num) {
                break;
            }
            
            TRY_ALL {            
                std::shared_ptr<char> data = alloc_data_buffer(m_blk_size);
                m_in_file.read(data.get(), m_blk_size);
                if (m_in_file) {
                    jobs->push_back(create_blk(data, m_blk_size));
                    continue;
                } else if (m_in_file.gcount() > 0) {
                    std::shared_ptr<char> tail_data = alloc_data_buffer(m_in_file.gcount());
                    memcpy(tail_data.get(), data.get(), m_in_file.gcount());                
                    jobs->push_back(create_blk(tail_data, m_in_file.gcount()));
                }
            } CATCH_ALL;
            
            done_hashing = true;
            break;
        }
        
        if (m_need_stop.load()) {
            break;
        }
        
        process_hashing(jobs);
        if (done_hashing) {
            break;
        }
    }
    
    // joining pools
    m_manager_loop.join();
    m_calc_pool.join();
    m_write_loop.join();
    
    // closing handlers
    m_out_file.close();
    m_in_file.close();
    
    // calculating elapsed time
    auto end_time = std::chrono::steady_clock::now();
    std::cout << "Time elapsed: " << std::chrono::duration <double, std::milli> (end_time - start_time).count() << " milliseconds \n";
}

}