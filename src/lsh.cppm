/********************************************************************************
* @Author : hexne
* @Date   : 2026/03/11 21:35:00
********************************************************************************/
module;
export module lsh;
import std;
import minhash;
import tools;
export class LSH {
    Minhash minhash_;

    std::vector<std::string> files_path_{};
    std::vector<int> root{};
    // 查找


    int find_root(int index) {
        if (index == root[index])
            return index;
        root[index] = find_root(root[index]);
        return root[index];
    }
    void unite(int x, int y) {
        root[find_root(x)] = find_root(y);
    }


    auto query() {
        std::unordered_map<std::size_t, int> ret;

        for (const auto &band : minhash_.get()) {
            for (const auto &file_info : band | std::views::values) {
                if (file_info.size() < 2)
                    continue;

                for (int i = 0;i < file_info.size(); i++) {
                    for (int j = i + 1;j < file_info.size(); j++) {
                        auto file_id_i = file_info[i].file_id;
                        auto file_id_j = file_info[j].file_id;

                        if (file_id_i == file_id_j)
                            continue;
                        if (file_id_i > file_id_j)
                            std::swap(file_id_i, file_id_j);

                        std::size_t key = static_cast<std::size_t>(file_id_i) << 32 | file_id_j;
                        ret[key] ++;
                    }
                }
            }
        }
        return ret;
    }

public:
    explicit LSH(const std::vector<std::string> &files_path)
            : files_path_(files_path) {
        root.resize(files_path_.size());
        std::ranges::iota(root, 0);
    }

    // 插入一个minhash
    void insert(const Minhash &minhash) {
        minhash_.merge(minhash);
    }

    // 分类
    std::vector<DedupeFilesPath> dsu() {
        auto query = this->query();
        for (const auto &[key, count] : query) {
            if (count < 2)
                continue;

            const double jaccard = static_cast<double>(count) / band_count;
            if (jaccard < jaccard_threshold)
                continue;

            auto file_id_i = static_cast<unsigned>(key >> 32);
            auto file_id_j = static_cast<unsigned>(key & 0xFFFFFFFF);

            unite(file_id_i, file_id_j);

        }
        std::unordered_map<int, std::vector<int>> classes;
        for (int i = 0;i < files_path_.size(); i++) {
            classes[find_root(i)].push_back(i);
        }

        std::vector<DedupeFilesPath> ret;
        for (const auto &indexs : classes | std::ranges::views::values) {
            if (indexs.size() == 1) {
                ret.push_back({.rep = files_path_[indexs[0]]});
            }
            else {
                std::string rep;
                std::vector<std::string> other_files;
                other_files.reserve(indexs.size());
                std::size_t size = std::numeric_limits<std::size_t>::min();
                for (const auto &index : indexs) {
                    if (auto cur_file_size = std::filesystem::file_size(files_path_[index]); cur_file_size >= size) {
                        size = cur_file_size;
                        rep = files_path_[index];
                    }
                    other_files.push_back(files_path_[index]);
                }
                other_files.erase(std::ranges::find(other_files, rep));
                ret.push_back({.rep = rep, .other_files = other_files});
            }
        }
        return ret;
    }

};