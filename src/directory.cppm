/********************************************************************************
* @Author : hexne
* @Date   : 2026/03/08 17:54:32
********************************************************************************/

export module directory;
import thread_pool;
import std;
import tools;

export enum class FileType {
    text, video, audio
};


export std::vector<std::string> find_files(const std::string& path, const FileType& file_type) {
    std::vector<std::string> extent;

    switch (file_type) {
    case FileType::text:
        extent = { "txt", "text" };
        break;
    case FileType::video:
        extent = { "mp4", "avi", "mkv" };
        break;
    case FileType::audio:
        extent = { "mp3", "wav", "flac" };
        break;
    }

    std::vector<std::string> ret;
    auto base = std::filesystem::absolute(path);

    for (const auto& file : std::filesystem::recursive_directory_iterator(base)) {
        if (!std::filesystem::is_regular_file(file))
            continue;

        auto ext = file.path().extension().string();
        if (ext.empty())
            continue;;
        ext = ext.substr(1);

        if (std::ranges::find(extent, ext) == std::ranges::end(extent))
            continue;
        ret.emplace_back(file.path().string());
    }
    return ret;
}

export void class_to(const std::vector<DedupeFilesPath> &dedupe_files, const std::string &path) {
    auto create_dir = [](const std::string& dir) {
        if (!std::filesystem::exists(dir))
            std::filesystem::create_directory(dir);
    };
    auto copy_to = [](const std::string& src_file, const std::string &dst_dir) {
        const auto file = std::filesystem::path(src_file);
        std::filesystem::copy_file(file, dst_dir / file.filename(), std::filesystem::copy_options::overwrite_existing);
    };


    std::println("copy -> {} begin", path);
    create_dir(path);
    for (int i = 0; i < dedupe_files.size(); ++i) {
        const auto &dedupe_file = dedupe_files[i];
        std::println("\t copy -> {:{}}/{}\r", i, std::to_string(dedupe_files.size()).size(), dedupe_files.size());
        // 仅有单个文件
        if (dedupe_file.other_files.empty()) {
            copy_to(dedupe_file.rep, path);
        }
        // 有其他文件
        else {
            auto dir = path / std::filesystem::path(dedupe_file.rep).filename();
            auto rep_dir = dir / "rep";
            create_dir(dir);
            create_dir(rep_dir);
            copy_to(dedupe_file.rep, rep_dir);

            for (const auto& file : dedupe_file.other_files) {
                copy_to(file, dir);
            }
        }
    }
    std::println("copy -> {} end", path);
}