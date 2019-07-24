
#ifndef HASHER_H
#define HASHER_H

#include <boost/asio.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/crc.hpp> 
#include <list>
#include <memory>
#include <fstream>
#include <atomic>
#include <vector>

namespace hs {

struct data_blk_t {  
    std::shared_ptr<char> data;
    int crc;
    size_t size;
};
    
typedef boost::asio::thread_pool thread_pool_t;
typedef std::shared_ptr<data_blk_t> data_blk_ptr_t;
typedef std::list<data_blk_ptr_t> job_list_t;
typedef std::shared_ptr<job_list_t> job_list_ptr_t;
typedef std::vector<data_blk_ptr_t> job_vec_t;

class hasher {
    
public:
    explicit hasher(const std::string &in_file, const std::string &out_file, size_t blk_size, std::size_t calc_thread_num = 1);
    virtual ~hasher();
    void run();    
    
private:
    void process_hashing(const job_vec_t &jobs, size_t jobs_to_calc);
    void calc_worker(data_blk_ptr_t blk);
    void write_worker(int crc);
    
private:
    // main hasher options
    std::string m_in_filename;
    std::string m_out_filename;
    size_t      m_blk_size;
    size_t      m_calc_thread_num;
    
    // file handlers
    std::ifstream m_in_file;
    std::ofstream m_out_file;

    // an atomic counter, counts jobsnumber  done in calc pool
    std::atomic_uint m_manager_calc_cntr;
    
    // a hash calculation pool, calculates hashes of m_calc_thread_num blocks on the same number of threads
    thread_pool_t m_calc_pool;
    
    // a pool for writing prepared data to a file
    thread_pool_t m_write_loop;
    
    // a stop flag. if true, something goes wrong, need the all threads to be stopped and joined
    std::atomic_bool m_need_stop;
};

}

#endif /* HASHER_H */

