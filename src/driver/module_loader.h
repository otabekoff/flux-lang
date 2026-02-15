#ifndef FLUX_MODULE_LOADER_H
#define FLUX_MODULE_LOADER_H

#include "ast/ast.h"
#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace flux {

class ModuleLoader {
  public:
    ModuleLoader();

    /// Set search paths for modules (e.g., ["std/"])
    void add_search_path(const std::filesystem::path& path);

    /// Load a module and all its dependencies recursively.
    /// @param path_or_name Either a file path or a module name (e.g., "std::io")
    ast::Module* load(const std::string& path_or_name);

    /// Get all loaded modules
    const std::map<std::string, ast::Module>& modules() const {
        return modules_;
    }

  private:
    std::filesystem::path find_module_file(const std::string& module_name);
    std::string module_name_to_path(const std::string& module_name);

    std::vector<std::filesystem::path> search_paths_;
    std::map<std::string, ast::Module> modules_;
    std::vector<std::string> loading_stack_; // For circular dependency detection
};

} // namespace flux

#endif // FLUX_MODULE_LOADER_H
