#include <iostream>
#include <array>
#include <random>
#include <thread>
#include <shared_mutex>
#include <mutex>
#include <chrono>

// A simple "database" with no locking that is dangerous to use across multiple threads
// The sleep_for statements simulate a more complex operation
class Database
{
public:
    Database()
    {
        std::fill(data_.begin(), data_.end(), 0);
    }

    // Obtain a value
    int readValue(size_t index) const
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        return data_.at(index);
    }

    // update a value
    void updateValue(size_t index, int64_t value)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        data_.at(index) += value;
    }

    // used for testing
    bool isAllZero()
    {
        return std::all_of(data_.begin(), data_.end(), [](const int64_t &x) -> bool
                           { return x == 0; });
    }

    static constexpr size_t DATABASE_SIZE = 10;
    static constexpr auto DATABASE_TYPE = "non-threadsafe database";

    friend std::ostream &operator<<(std::ostream &os, const Database &db);

protected:
    std::array<int64_t, DATABASE_SIZE> data_;
};

// A database with simple locking using a mutex
// Threadsafe but slower
class SingleMutexDatabase : public Database
{
public:
    SingleMutexDatabase() : Database(){};
    int readValue(size_t index) const
    {
        std::lock_guard<std::mutex> lock{mutex_};
        return Database::readValue(index);
    }

    void updateValue(size_t index, int64_t value)
    {
        std::lock_guard<std::mutex> lock{mutex_};
        Database::updateValue(index, value);
    }

    static constexpr auto DATABASE_TYPE = "single mutex database";

private:
    mutable std::mutex mutex_;
};

// A database using a shared_mutex allowing multiple reads at the same time
class SharedMutexDatabase : public Database
{
public:
    SharedMutexDatabase() : Database(){};
    int readValue(size_t index) const
    {
        // reads can oocurs simultaneously with a shared_lock
        std::shared_lock lock{shared_mutex_};
        return Database::readValue(index);
    }

    void updateValue(size_t index, int64_t value)
    {
        // writes require a unique_lock
        std::unique_lock lock{shared_mutex_};
        Database::updateValue(index, value);
    }

    static constexpr auto DATABASE_TYPE = "shared mutex database";

private:
    mutable std::shared_mutex shared_mutex_;
};

// A database where access is controlled by multiple mutexes allowing
// some degree of shared access
class SplitMutexDatabase : public Database
{
public:
    SplitMutexDatabase() : Database() {}
    int readValue(size_t index) const
    {
        std::lock_guard<std::mutex> lock{
            mutexes_[index % mutexes_.size()]};
        return Database::readValue(index);
    };

    void updateValue(size_t index, int64_t value)
    {
        std::lock_guard<std::mutex> lock{
            mutexes_[index % mutexes_.size()]};
        Database::updateValue(index, value);
    }

    static constexpr auto DATABASE_TYPE = "split mutex database";

private:
    mutable std::array<std::mutex, DATABASE_SIZE / 2> mutexes_;
};

std::ostream &
operator<<(std::ostream &os, const Database &db)
{
    for (auto &i : db.data_)
    {
        os << i << " ";
    }
    return os;
}

std::array<size_t, Database::DATABASE_SIZE>
getIndicesInShuffledOrder(int seed)
{
    std::array<size_t, Database::DATABASE_SIZE> indices;
    std::iota(indices.begin(), indices.end(), 0);
    std::mt19937 engine(seed);

    std::shuffle(indices.begin(), indices.end(), engine);
    return indices;
}

template <typename DBTYPE>
void readAllInRandomOrder(DBTYPE &database, int seed)
{
    std::array<size_t, Database::DATABASE_SIZE> indices{getIndicesInShuffledOrder(seed)};
    std::for_each(indices.begin(), indices.end(), [&](const size_t &i)
                  { database.readValue(i); 
                  std::this_thread::sleep_for(std::chrono::milliseconds(1)); });
}

template <typename DBTYPE>
void updateAllInRandomOrder(DBTYPE &database, int64_t value, int seed)
{
    std::array<size_t, Database::DATABASE_SIZE> indices{getIndicesInShuffledOrder(seed)};
    std::for_each(indices.begin(), indices.end(), [&](const size_t &i)
                  { database.updateValue(i, value);
                  std::this_thread::sleep_for(std::chrono::milliseconds(10)); });
}

template <typename DBTYPE>
void testDatabase()
{
    DBTYPE db;

    std::vector<std::thread> threads;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (size_t i = 0; i < 100; ++i)
    {
        threads.emplace_back([&]()
                             {
            for (int repeat = 0; repeat < 1; ++repeat)
            {
                updateAllInRandomOrder(db, 25, threads.size());
                updateAllInRandomOrder(db, -40, threads.size());
                updateAllInRandomOrder(db, 15, threads.size());
            } });
    }

    for (size_t i = 0; i < 1000; ++i)
    {
        threads.emplace_back([&]()
                             {
            for (int repeat = 0; repeat < 1; ++repeat)
            {
                readAllInRandomOrder(db, threads.size());
            } });
    }

    std::for_each(threads.begin(), threads.end(), [&](std::thread &t)
                  {
        if (t.joinable())
        {
            t.join();
        } });

    auto endTime = std::chrono::high_resolution_clock::now();

    std::cout << "Results for " << DBTYPE::DATABASE_TYPE << ":\n"
              << "  Elapsed Time:      " << std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count() << "ms\n"
              << "  Database Contents: " << db << "\n"
              << "  All Zero:          " << (db.isAllZero() ? "Pass" : "FAILED!!!") << std::endl;
}

int main(int, char *[])
{
    testDatabase<Database>();
    testDatabase<SingleMutexDatabase>();
    testDatabase<SharedMutexDatabase>();
    testDatabase<SplitMutexDatabase>();

    return 0;
}
