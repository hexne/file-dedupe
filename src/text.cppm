/********************************************************************************
* @Author : hexne
* @Date   : 2026/03/11 18:41:39
********************************************************************************/
module;
#include <iconv.h>
#include <opencc/opencc.h>
export module text;
import std;
import lsh;
import tools;
import minhash;
import directory;
import thread_pool;

// 用于处理文件的字符串
class FileText {
    int id_;
    std::u32string context_;
    Minhash minhash_;

    static std::string traditional_to_simplified(const std::string& text) {
        opencc_t h = opencc_open("t2s.json");
        if (h == (opencc_t)-1) throw std::runtime_error("OpenCC failed: t2s.json");

        char* out = opencc_convert_utf8(h, text.c_str(), (size_t)-1);
        std::string result = out ? std::string(out) : text;
        if (out) opencc_convert_utf8_free(out);

        opencc_close(h);
        return result;
    }

    static std::string load_text_file(const std::string& path) {
        std::ifstream f(path, std::ios::binary);

        std::vector<char> bytes((std::istreambuf_iterator<char>(f)), {});
        std::vector<std::string> encodings = {"UTF-8", "GB18030", "GBK", "BIG5", "LATIN-1", "UTF-16"};

        auto try_convert = [&](const std::string& enc) {
            iconv_t cd = iconv_open("UTF-8", enc.c_str());
            if (cd == (iconv_t)-1) return std::string();

            size_t in_len = bytes.size();
            size_t out_len = bytes.size() * 4 + 4;
            std::string out(out_len, '\0');

            const char* in_buf = bytes.data();
            char* out_buf = &out[0];
            size_t result = iconv(cd, const_cast<char**>(&in_buf), &in_len, &out_buf, &out_len);
            iconv_close(cd);

            if (result == (size_t)-1 && in_len) return std::string();
            out.resize(out.size() - out_len);
            return out;
        };

        for (const auto& enc : encodings) {
            std::string res = try_convert(enc);
            if (!res.empty())
                return res;
        }
        return {bytes.begin(), bytes.end()};
    }

    static std::u32string utf8_to_utf32(const std::string& utf8) {
        std::u32string result;
        size_t i = 0;
        while (i < utf8.size()) {
            unsigned char c = utf8[i];
            char32_t codepoint = 0;
            size_t len = 0;

            if (c < 0x80) { codepoint = c; len = 1; }
            else if ((c >> 5) == 0x6) { codepoint = (c & 0x1F) << 6 | (utf8[i+1] & 0x3F); len = 2; }
            else if ((c >> 4) == 0xE) { codepoint = (c & 0x0F) << 12 | (utf8[i+1] & 0x3F) << 6 | (utf8[i+2] & 0x3F); len = 3; }
            else if ((c >> 3) == 0x1E) { codepoint = (c & 0x07) << 18 | (utf8[i+1] & 0x3F) << 12 | (utf8[i+2] & 0x3F) << 6 | (utf8[i+3] & 0x3F); len = 4; }
            else {
                std::cout << utf8 << std::endl;
                throw std::runtime_error("非法 UTF-8 序列");
            }

            result.push_back(codepoint);
            i += len;
        }
        return result;
    }

    static std::u32string normalization_text(const std::string& in) {
        std::string simplified = traditional_to_simplified(in);

        std::string remove_ad;
        remove_ad.reserve(simplified.size());
        std::string line;
        std::istringstream ss(simplified);
        std::vector<std::string> ad_mask = { "www.", "https://" };

        while (std::getline(ss, line)) {
            bool find_ad = false;
            for (auto &ad : ad_mask) {
                if (line.find(ad) != std::string::npos) {
                    find_ad = true;
                    break;
                }
            }
            if (find_ad) continue;
            remove_ad += line;
            remove_ad.push_back('\n');
        }

        auto is_cjk = [](const char32_t ch) {
            return ch >= 0x4E00 and ch <= 0x9FFF;
        };
        auto context = utf8_to_utf32(remove_ad);

        std::u32string cjk;
        cjk.reserve(remove_ad.size());
        for (auto &ch : context) {
            if (is_cjk(ch))
                cjk.push_back(ch);
        }

        std::u32string ret;
        ret.reserve(cjk.size());
        for (size_t i = 0; i < cjk.size(); ++i) {
            if (cjk[i] == '\r') {
                ret.push_back('\n');
                if (i+1 < cjk.size() && cjk[i+1] == '\n') ++i;
            }
            else if (cjk[i] == ' ') {
                while (i+1 < cjk.size() && cjk[i+1] == ' ') ++i;
                ret.push_back(' ');
            }
            else {
                ret.push_back(cjk[i]);
            }
        }
        return ret;
    }


    static std::vector<std::u32string> split_chunk(const std::u32string& in) {
        constexpr int window = 2000;
        constexpr int stride = window / 2;

        std::vector<std::u32string> chunks;
        size_t index{};
        while (index < in.size()) {
            auto begin = in.begin() + index;
            auto end = std::min(begin + window, in.end());
            chunks.emplace_back(begin, end);
            index += stride;
        }
        return chunks;
    }

    // 分割出gram， 一个u32 是一个 gram
    static std::vector<std::u32string> split_gram(const std::u32string &in) {
        constexpr int n = 5;

        std::vector<std::u32string> ret;
        int pos{};
        while (pos < in.size()) {
            auto begin = in.begin() + pos;
            auto end = std::min(in.begin() + pos + n, in.end());
            ++ pos;
            ret.emplace_back(begin, end);
        }
        return ret;
    }


    static auto grams_to_hash(const std::vector<std::u32string> &in) {
        static std::hash<std::u32string> hasher;
        std::unordered_set<size_t> ret;
        for (auto &str : in) {
            ret.insert(hasher(str));
        }
        return ret;
    }

public:
    explicit FileText(const std::string &path, const int id) {
        id_ = id;
        context_ = normalization_text(load_text_file(path));
    }

    Minhash to_minhash() {
        auto split_chunks = split_chunk(context_);
        for (int i = 0;i < split_chunks.size(); ++i) {
            static std::hash<std::u32string> hasher;
            auto &chunk = split_chunks[i];
            auto grams = split_gram(chunk);

            std::unordered_set<Hash> chunk_hash;
            for (int j = 0;j < grams.size(); ++j)
                chunk_hash.insert(hasher(grams[j]));

            minhash_.insert_chunk_hash(chunk_hash, {.file_id = static_cast<unsigned>(id_), .chunk_id = static_cast<unsigned>(i)});
        }
        return minhash_;
    }
};

// 分析path下的所有text文件
export std::vector<DedupeFilesPath> analyse_text(const std::string& path) {
    const auto files = search_files(path, FileType::text);
    LSH lsh(files);

    // 加载文件夹下所有的text文件
    std::println("analyse -> {}", path);
    for (int i = 0;i < files.size(); ++i) {
        std::print("\t-> [{:{}}/{}]\r", i + 1, std::to_string(i).size(), files.size());
        std::cout.flush();

        // 处理当前文件
        FileText file_text(files[i], i);
        lsh.insert(file_text.to_minhash());
    }
    endl(std::cout);
    return lsh.dsu();
}