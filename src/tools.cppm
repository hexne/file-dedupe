/********************************************************************************
* @Author : hexne
* @Date   : 2026/03/11 18:00:59
********************************************************************************/
export module tools;
import std;

export struct ChunkID {
    unsigned file_id;
    unsigned chunk_id;
    bool operator==(const ChunkID& other) const {
        return file_id == other.file_id and chunk_id == other.chunk_id;
    }
    struct Hash {
        std::size_t operator()(const ChunkID& id) const noexcept {
            static std::hash<unsigned> hasher;
            return hasher(id.file_id) ^ hasher(id.chunk_id);
        }
    };
};

export struct DedupeFilesPath {
    std::string rep;
    std::vector<std::string> other_files;
};

