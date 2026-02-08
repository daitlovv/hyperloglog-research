#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <unordered_set>
#include <cmath>
#include <map>
#include <numeric>

class RandomStreamGen {
    std::mt19937 rng;
    std::string chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-";

public:
    RandomStreamGen(int seed = 123) : rng(seed) {}

    std::string make_str() {
        std::uniform_int_distribution<int> len_dist(1, 30);
        std::uniform_int_distribution<int> char_dist(0, chars.size() - 1);
        int len = len_dist(rng);
        std::string s;
        for (int i = 0; i < len; ++i) {
            s.push_back(chars[char_dist(rng)]);
        }
        return s;
    }

    std::vector<std::string> make_stream(int size) {
        std::vector<std::string> stream;
        for (int i = 0; i < size; ++i) {
            stream.push_back(make_str());
        }
        return stream;
    }

    std::vector<std::vector<std::string>> split_stream(const std::vector<std::string>& stream, int step = 10) {
        std::vector<std::vector<std::string>> parts;
        int total = stream.size();
        for (int p = step; p <= 100; p += step) {
            int part_size = total * p / 100;
            std::vector<std::string> part(stream.begin(), stream.begin() + part_size);
            parts.push_back(part);
        }
        return parts;
    }
};

int count_unique_exact(const std::vector<std::string>& strings) {
    std::unordered_set<std::string> unique;
    for (const std::string& s : strings) {
        unique.insert(s);
    }
    return unique.size();
}

class HashFuncGen {
    uint64_t a;
    uint64_t b;

public:
    HashFuncGen() {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;
        a = dist(gen) | 1;
        b = dist(gen);
    }

    uint32_t hash(const std::string& s) const {
        uint64_t h = b;
        for (char c : s) {
            h = h * a + static_cast<unsigned char>(c);
        }
        return static_cast<uint32_t>(h);
    }

    void test_uniformity(const std::vector<std::string>& samples, int buckets = 100) {
        std::vector<int> counts(buckets, 0);
        for (const std::string& s : samples) {
            uint32_t h = hash(s);
            counts[h % buckets]++;
        }

        double mean = samples.size() / (double)buckets;
        double variance = 0;
        for (int c : counts) {
            variance += (c - mean) * (c - mean);
        }
        variance /= buckets;
        double std_dev = std::sqrt(variance);
        double relative_std = std_dev / mean * 100;

        std::cout << "Тест равномерности хеш-функции:\n";
        std::cout << "Среднее на корзину: " << mean << '\n';
        std::cout << "Стандартное отклонение: " << std_dev << '\n';
        std::cout << "Относительное отклонение: " << relative_std << "%\n";
        std::cout << "Ожидаемое для равномерного: " << 100.0 / std::sqrt(buckets) << "%\n";
    }
};

class HyperLogLog {
    int b;
    int q;
    std::vector<uint8_t> regs;
    HashFuncGen hasher;
    double alpha;

public:
    HyperLogLog(int bits = 10) : b(bits), q(1 << bits), regs(q, 0) {
        if (q == 2) {
            alpha = 0.3512;
        }
        else if (q == 4) {
            alpha = 0.5324;
        }
        else if (q == 16) {
            alpha = 0.673;
        }
        else if (q == 32) {
            alpha = 0.697;
        }
        else if (q == 64) {
            alpha = 0.709;
        }
        else {
            alpha = 0.7213 / (1.0 + 1.079 / q);
        }
    }

    void add(const std::string& s) {
        uint32_t h = hasher.hash(s);
        int ind = h >> 32 - b;
        uint32_t w = h << b;
        if (w == 0) {
            w = 1;
        }

        int pos = 1;
        while ((w & 0x80000000) == 0 && pos < 32) {
            w <<= 1;
            pos++;
        }

        if (pos > regs[ind]) {
            regs[ind] = pos;
        }
    }

    double estimate() {
        double sum = 0.0;
        int zeros = 0;
        for (int i = 0; i < q; ++i) {
            sum += std::pow(2.0, -regs[i]);
            if (regs[i] == 0) {
	            zeros++;
            }
        }

        double est = alpha * q * q / sum;

        if (est <= 2.5 * q) {
            if (zeros > 0) {
                est = q * std::log(static_cast<double>(q) / zeros);
            }
        }

        if (est > 143165576.533) {
            est = -4294967296.0 * std::log(1.0 - est / 4294967296.0);
        }

        return est;
    }

    int memory_used() {
        return q;
    }

    void reset() {
        std::fill(regs.begin(), regs.end(), 0);
    }
};

class HyperLogLogPlus {
    int b;
    int q;
    bool sparse_mode;
    std::map<uint16_t, uint8_t> sparse_data;
    std::vector<uint8_t> dense_data;
    HashFuncGen hasher;
    double alpha;

    void to_dense() {
        dense_data.assign(q, 0);
        for (auto& [ind, val] : sparse_data) {
            if (val > dense_data[ind]) {
                dense_data[ind] = val;
            }
        }
        sparse_data.clear();
        sparse_mode = false;
    }

public:
    HyperLogLogPlus(int bits = 10) : b(bits), q(1 << bits), sparse_mode(true), dense_data(q, 0) {
        if (q == 2) {
            alpha = 0.3512;
        }
        else if (q == 4) {
            alpha = 0.5324;
        }
        else if (q == 16) {
            alpha = 0.673;
        }
        else if (q == 32) {
            alpha = 0.697;
        }
        else if (q == 64) {
            alpha = 0.709;
        }
        else {
            alpha = 0.7213 / (1.0 + 1.079 / q);
        }
    }

    void add(const std::string& s) {
        uint32_t h = hasher.hash(s);
        int ind = h >> 32 - b;
        uint32_t w = h << b;
        if (w == 0) {
            w = 1;
        }

        int pos = 1;
        while ((w & 0x80000000) == 0 && pos < 32) {
            w <<= 1;
            pos++;
        }

        if (sparse_mode) {
            auto it = sparse_data.find(ind);
            if (it == sparse_data.end() || pos > it->second) {
                sparse_data[ind] = pos;
            }
            if (sparse_data.size() * 4 > q) {
                to_dense();
            }
        } else {
            if (pos > dense_data[ind]) {
                dense_data[ind] = pos;
            }
        }
    }

    double estimate() {
        if (sparse_mode) {
            if (sparse_data.empty()) return 0;

            double sum = 0.0;
            for (const auto& [ind, val] : sparse_data) {
                sum += std::pow(2.0, -val);
            }

            double est = alpha * q * q / sum;

            if (est <= 5 * q) {
                int zeros = q - sparse_data.size();
                if (zeros > 0) {
                    est = q * std::log(static_cast<double>(q) / zeros);
                }
            }

            return est;
        }
        double sum = 0.0;
        int zeros = 0;
        for (int i = 0; i < q; ++i) {
            sum += std::pow(2.0, -dense_data[i]);
            if (dense_data[i] == 0) zeros++;
        }

        double est = alpha * q * q / sum;

        if (est <= 2.5 * q) {
            if (zeros > 0) {
                est = q * std::log(static_cast<double>(q) / zeros);
            }
        }

        if (est > 143165576.533) {
            est = -4294967296.0 * std::log(1.0 - est / 4294967296.0);
        }

        return est;
    }

    int memory_used() {
        if (sparse_mode) {
            return sparse_data.size() * 3;
        }
        return q;
    }

    void reset() {
        if (sparse_mode) {
            sparse_data.clear();
        } else {
            std::fill(dense_data.begin(), dense_data.end(), 0);
        }
        sparse_mode = true;
    }
};

void test_hash_uniformity() {
    std::cout << "Тестим равномерность хеша\n";
    RandomStreamGen gen;
    std::vector<std::string> stream = gen.make_stream(10000);
    HashFuncGen hasher;
    hasher.test_uniformity(stream, 100);
    std::cout << '\n';
}

void test_accuracy() {
    std::cout << "Тестим точность\n";
    std::cout << "процент,точное_число,оценка,ошибка_процентов\n";

    RandomStreamGen gen;
    std::vector<std::string> stream = gen.make_stream(100000);
    std::vector<std::vector<std::string>> parts = gen.split_stream(stream, 10);

    HyperLogLog counter(10);

    for (size_t i = 0; i < parts.size(); ++i) {
        int true_count = count_unique_exact(parts[i]);

        counter.reset();
        for (std::string& s : parts[i]) {
            counter.add(s);
        }

        double est = counter.estimate();
        double err = std::abs(est - true_count) / true_count * 100.0;

        std::cout << (i + 1) * 10 << ","
                  << true_count << ","
                  << est << ","
                  << err << '\n';
    }
    std::cout << '\n';
}

void test_statistics() {
    std::cout << "Тестим статистики\n";
    std::cout << "процент,среднее_Nt,стандартное_отклонение,среднее-отклонение,среднее+отклонение\n";

    RandomStreamGen gen;
    std::vector<std::string> stream = gen.make_stream(80000);
    std::vector<std::vector<std::string>> parts = gen.split_stream(stream, 10);

    const int runs = 30;

    for (size_t i = 0; i < parts.size(); ++i) {
        std::vector<double> estimates;

        for (int run = 0; run < runs; ++run) {
            HyperLogLog counter(10);
            for (const std::string& s : parts[i]) {
                counter.add(s);
            }
            estimates.push_back(counter.estimate());
        }

        double sum = std::accumulate(estimates.begin(), estimates.end(), 0.0);
        double mean = sum / runs;

        double variance = 0.0;
        for (double est : estimates) {
            variance += (est - mean) * (est - mean);
        }
        variance /= runs;
        double std_dev = std::sqrt(variance);

        std::cout << (i + 1) * 10 << ","
                  << mean << ","
                  << std_dev << ","
                  << mean - std_dev << ","
                  << mean + std_dev << '\n';
    }
    std::cout << '\n';
}

void test_different_b() {
    std::cout << "Тестим разные b\n";
    std::cout << "биты,средняя_ошибка,память_байт,теор_нижняя_граница,теор_верхняя_граница\n";

    RandomStreamGen gen;
    std::vector<std::string> stream = gen.make_stream(50000);
    std::vector<std::vector<std::string>> parts = gen.split_stream(stream, 20);

    for (int bits = 6; bits <= 14; bits += 2) {
        double total_err = 0.0;
        int cnt = 0;

        HyperLogLog counter(bits);

        for (const std::vector<std::string>& part : parts) {
            int true_count = count_unique_exact(part);

            counter.reset();
            for (const std::string &s : part) {
                counter.add(s);
            }

            double est = counter.estimate();
            double err = std::abs(est - true_count) / true_count * 100.0;
            total_err += err;
            cnt++;
        }

        double avg_err = total_err / cnt;
        int memory = 1 << bits;

        double m = 1 << bits;
        double theoretical_low = 1.04 / std::sqrt(m) * 100;
        double theoretical_high = 1.30 / std::sqrt(m) * 100;

        std::cout << bits << ","
                  << avg_err << ","
                  << memory << ","
                  << theoretical_low << ","
                  << theoretical_high << '\n';
    }
    std::cout << '\n';
}

void compare_versions() {
    std::cout << "Сравниваем версии\n";
    std::cout << "процент,точное,базовая_оценка,улучшенная_оценка,ошибка_базовой,ошибка_улучшенной,память_базовой,память_улучшенной\n";
    RandomStreamGen gen;
    std::vector<std::string> stream = gen.make_stream(80000);
    std::vector<std::vector<std::string>> parts = gen.split_stream(stream, 10);

    HyperLogLog basic(10);
    HyperLogLogPlus plus(10);

    for (size_t i = 0; i < parts.size(); ++i) {
        int true_count = count_unique_exact(parts[i]);

        basic.reset();
        plus.reset();

        for (std::string& s : parts[i]) {
            basic.add(s);
            plus.add(s);
        }

        double basic_est = basic.estimate();
        double plus_est = plus.estimate();

        double basic_err = std::abs(basic_est - true_count) / true_count * 100.0;
        double plus_err = std::abs(plus_est - true_count) / true_count * 100.0;

        std::cout << (i + 1) * 10 << ","
                  << true_count << ","
                  << basic_est << ","
                  << plus_est << ","
                  << basic_err << ","
                  << plus_err << ","
                  << basic.memory_used() << ","
                  << plus.memory_used() << '\n';
    }
    std::cout << '\n';
}


int main() {
    test_hash_uniformity();
    test_accuracy();
    test_statistics();
    test_different_b();
	compare_versions();
    return 0;
}