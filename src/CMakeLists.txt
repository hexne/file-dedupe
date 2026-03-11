file(GLOB module_files "./*.cppm")

add_library(module ${LibMod})
target_sources(module
        PUBLIC
        FILE_SET cxx_modules TYPE CXX_MODULES FILES
        ${module_files}
)

target_compile_features(module PUBLIC cxx_std_26)

find_package(OpenCC REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)


add_executable(${PROJECT_NAME} main.cpp)

target_link_libraries(${PROJECT_NAME} PRIVATE
        module
        OpenCC::OpenCC
        nlohmann_json::nlohmann_json
)