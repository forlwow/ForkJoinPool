#include "numeric"
#include "functional"
#include "thread"

template<typename Iterator, typename T>
struct accumulate_block{
    void operator() (Iterator first, Iterator last, T& result){
        result = std::accumulate(first, last, result);
    }
};

template<typename Iterator, typename T>
T parallel_acc(Iterator first, Iterator last, T init){
    const unsigned long length = std::distance(first, last);
    if (!length) return init;

    const unsigned long min_per_thread = 25;
    const unsigned long max_threads = (length + min_per_thread - 1) / min_per_thread;

    const unsigned long hardware_threads = std::thread::hardware_concurrency();

    const unsigned long num_threads = std::min(hardware_threads != 0 ? hardware_threads : 2, max_threads);

    const unsigned long block_size = length / num_threads;

    std::vector<T> result(num_threads);
    std::vector<std::thread> threads(num_threads - 1);

    Iterator block_start = first;
    for (unsigned long i = 0; i < (num_threads - 1); ++i){
        Iterator block_end = block_start;
        std::advance(block_end, block_size);
        threads[i] = std::thread(
                accumulate_block<Iterator, T>(),
                block_start, block_end, std::ref(result[i])
        );
        block_start = block_end;
    }
    accumulate_block<Iterator, T>() (block_start, last, result[num_threads - 1]);
    for (auto &entry : threads){
        entry.join();
    }
    return std::accumulate(result.begin(), result.end(), init);
}


