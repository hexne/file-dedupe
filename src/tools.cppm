/********************************************************************************
* @Author : hexne
* @Date   : 2026/03/11 18:00:59
********************************************************************************/

export module tools;
import std;

export struct DedupeFilesPath {
    std::string rep;
    std::vector<std::string> other_files;
};