#include "driver/module_loader.h"
#include "lexer/diagnostic.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace flux {

ModuleLoader::ModuleLoader() {
    // Default search paths
    search_paths_.push_back(std::filesystem::current_path());
}

void ModuleLoader::add_search_path(const std::filesystem::path& path) {
    search_paths_.push_back(path);
}

ast::Module* ModuleLoader::load(const std::string& path_or_name) {
    std::filesystem::path file_path;
    std::string module_name;

    if (std::filesystem::exists(path_or_name)) {
        file_path = std::filesystem::absolute(path_or_name);
        // We'll determine the module name after parsing
    } else {
        file_path = find_module_file(path_or_name);
        module_name = path_or_name;
    }

    if (file_path.empty()) {
        throw std::runtime_error("flux: could not find module: " + path_or_name);
    }

    // Check if already loaded
    // (We need to parse first to know the canonical name if we only have a path)

    std::ifstream file(file_path);
    if (!file) {
        throw std::runtime_error("flux: could not open file: " + file_path.string());
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    Lexer lexer(buffer.str());
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    ast::Module module = parser.parse_module();

    if (module_name.empty()) {
        module_name = module.name;
    }

    if (modules_.count(module_name)) {
        return &modules_[module_name];
    }

    // Detect circular dependencies
    if (std::find(loading_stack_.begin(), loading_stack_.end(), module_name) !=
        loading_stack_.end()) {
        throw std::runtime_error("flux: circular dependency detected involving module: " +
                                 module_name);
    }

    loading_stack_.push_back(module_name);

    // Recursively load imports
    for (const auto& import_node : module.imports) {
        load(import_node.module_path);
    }

    loading_stack_.pop_back();

    modules_[module_name] = std::move(module);
    return &modules_[module_name];
}

std::filesystem::path ModuleLoader::find_module_file(const std::string& module_name) {
    std::string relative_path = module_name_to_path(module_name);

    for (const auto& base : search_paths_) {
        auto p = base / relative_path;
        if (std::filesystem::exists(p))
            return p;
    }
    return {};
}

std::string ModuleLoader::module_name_to_path(const std::string& module_name) {
    std::string path = module_name;
    // Replace :: with /
    size_t pos = 0;
    while ((pos = path.find("::", pos)) != std::string::npos) {
        path.replace(pos, 2, "/");
        pos += 1;
    }
    return path + ".fl";
}

} // namespace flux
