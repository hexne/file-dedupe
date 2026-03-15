/********************************************************************************
* @Author : hexne
* @Date   : 2026/03/11 19:16:40
********************************************************************************/
module;
export module minhash;
import tools;
import std;
import std.compat;

consteval float consteval_pow(float base, size_t exp) {
    float result = 1.0f;
    for (size_t i = 0; i < exp; ++i) {
        result *= base;
    }
    return result;
}
consteval float consteval_abs(float number) {
    if (number >= 0)
        return number;
    return -number;
}
consteval std::tuple<size_t, size_t> band_format(float threshold, int minhash_size) {
    size_t np = minhash_size;
    size_t best_b = 1, best_r = np;
    float best_err = std::numeric_limits<float>::max();

    for (size_t b = 1; b <= np; ++b) {
        if (np % b != 0) continue;
        size_t r = np / b;
        float err = consteval_abs(1.0f / b - consteval_pow(threshold, r));
        if (err < best_err) {
            best_err = err;
            best_b = b;
            best_r = r;
        }
    }
    return {best_b, best_r};
}

constexpr size_t minhash_size = 128;
export constexpr float jaccard_threshold = 0.1f;
constexpr std::tuple<size_t, size_t> band_format_tuple = band_format(jaccard_threshold, minhash_size);
export constexpr size_t band_count = std::get<0>(band_format_tuple);
export constexpr size_t band_size = std::get<1>(band_format_tuple);

export using Hash = size_t;

// 一个chunk的minhash
// 一个file的minhash
// 一个file 集合的minhash
// 因此两个minhash的合并只需要对应位置的chunk合并即可
export class Minhash {

    std::array<std::unordered_map<Hash, std::vector<ChunkID>>, band_count> minhash_;
    inline static std::array<std::tuple<unsigned, unsigned>, minhash_size> ab_pairs = []{
        std::array<std::tuple<unsigned, unsigned>, minhash_size> ret;
        static std::mt19937 rng(0);
        for (int i = 0;i < minhash_size; ++i)
            ret[i] = {rng(), rng()};
        return ret;
    }();

    static auto split_minhash(const std::array<Hash, minhash_size> &minhash) {
        std::array<std::array<Hash, band_size>, band_count> ret;
        for (int i = 0;i < band_count; ++i)
            for (int j = 0; j < band_size; ++j)
                ret[i][j] = minhash[i * band_size + j];

        return ret;
    }

    static auto band_to_hash(const std::array<std::array<Hash, band_size>, band_count> &minhash) {

        auto hasher = [](const std::array<Hash, band_size> &chunk_hash) {
            std::size_t h = 14695981039346656037ULL;
            for(const auto v : chunk_hash){
                h ^= v;
                h *= 1099511628211ULL;
            }
            return h;
        };
        std::array<Hash, band_count> ret{};
        for (int i = 0;i < band_count; ++i) {
            ret[i] = hasher(minhash[i]);
        }
        return ret;
    }


public:
    // 参数为一个chunk中所有hash的集合 和 该chunk的id
    void insert_chunk_hash(const std::unordered_set<Hash> &chunk_hash, const ChunkID &chunk_id) {
        static constexpr std::size_t LP = 2305843009213693951;

        // 先将当前的chunk hash 转为minhash_size长度的minhash
        std::array<Hash, minhash_size> minhash;
        for (int i = 0; i < ab_pairs.size(); ++i) {
            auto [a, b] = ab_pairs[i];
            Hash min = std::numeric_limits<Hash>::max();
            for (const auto hash : chunk_hash) {
                min = std::min((a * hash + b) % LP, min);
            }
            minhash[i] = min;
        }

        auto split_minhash = this->split_minhash(minhash);
        auto band_hash = band_to_hash(split_minhash);

        for (int i = 0;i < band_count; ++i) {
            minhash_[i][band_hash[i]].push_back(chunk_id);
        }
    }

    // 给lsh中的minhash用
    void merge(const Minhash &other_minhash) {
        for (int i = 0;i < band_count; ++i) {
            const auto minhash = other_minhash.minhash_[i];
            for (const auto &[hash, chunks_info] : minhash) {
                minhash_[i][hash].append_range(chunks_info);
            }
        }
    }

    auto& get() {
        return minhash_;
    }

};