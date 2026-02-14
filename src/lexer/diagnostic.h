//
// Created by marko on 2/9/2026.
//

#ifndef FLUX_DIAGNOSTIC_H
#define FLUX_DIAGNOSTIC_H

#include <cstddef>
#include <stdexcept>
#include <string>

namespace flux {

class DiagnosticError : public std::runtime_error {
  public:
    DiagnosticError(std::string message, std::size_t line, std::size_t column)
        : std::runtime_error(build_message(message, line, column)), line_(line), column_(column) {}

    std::size_t line() const {
        return line_;
    }
    std::size_t column() const {
        return column_;
    }

  private:
    static std::string build_message(const std::string& message, std::size_t line,
                                     std::size_t column) {
        return "error: " + message + " at " + std::to_string(line) + ":" + std::to_string(column);
    }

    std::size_t line_;
    std::size_t column_;
};

} // namespace flux

#endif // FLUX_DIAGNOSTIC_H
