
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
        m_calc_pool(calc_thread_num), 
        m_write_loop(1)
{
}

hasher::~hasher()
{
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

void hasher::write_worker(int crc)
{
    if (m_need_stop.load()) {
        return;
    }
    
    TRY_ALL {
//        m_out_file.write(blk->data.get(), blk->size);
        m_out_file << std::hex << std::setw(8) << std::setfill('0') << crc << '\n';
//        m_out_file.write(reinterpret_cast<char*>(&blk->crc), sizeof(int));
    } CATCH_ALL;    
}

void hasher::process_hashing(const job_vec_t &jobs, size_t jobs_to_calc)
{
    if (jobs_to_calc == 0) {
        return;
    }
    
    if (m_need_stop.load()) {
        return;
    }
    
    m_manager_calc_cntr.store(0);
    
    // calculating hashes in the calculating pool
    for (size_t i = 0; i < jobs_to_calc; ++i) {
        boost::asio::post(m_calc_pool, std::bind(&hasher::calc_worker, this, jobs[i]));        
    }
    
    // wait until all the jobs in the list are done
    while ((m_manager_calc_cntr.load() != jobs_to_calc) && !m_need_stop.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // send results to the writer loop
    for (size_t i = 0; i < jobs_to_calc; ++i) {
        boost::asio::post(m_write_loop, std::bind(&hasher::write_worker, this, jobs[i]->crc));        
    }
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

inline job_vec_t alloc_job_vector(size_t vec_size, size_t blk_size)
{
    job_vec_t jobs_vec;    
    for (size_t i = 0; i < vec_size; ++i) {
        jobs_vec.push_back(create_blk(alloc_data_buffer(blk_size), blk_size));
    }
    return jobs_vec;
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
    
    job_vec_t jobs = alloc_job_vector(m_calc_thread_num, m_blk_size);
    
    for (;;) {
        size_t job_cntr = 0;
        while (!m_need_stop.load()) {            
            if (job_cntr == m_calc_thread_num) {
                break;
            }
            
            TRY_ALL {            
                m_in_file.read(jobs[job_cntr]->data.get(), m_blk_size);
                if (m_in_file) {
                    ++job_cntr;
                    continue;
                } else if (m_in_file.gcount() > 0) {
                    std::shared_ptr<char> tail_data = alloc_data_buffer(m_in_file.gcount());
                    jobs[job_cntr]->data = tail_data;
                    jobs[job_cntr]->size = m_in_file.gcount();
                    ++job_cntr;
                }
            } CATCH_ALL;
            
            done_hashing = true;
            break;
        }
        
        if (m_need_stop.load()) {
            break;
        }
        
        process_hashing(jobs, job_cntr);
        if (done_hashing) {
            break;
        }
    }
    
    // joining pools
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