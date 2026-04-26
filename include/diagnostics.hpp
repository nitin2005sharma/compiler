#ifndef DIAGNOSTICS_HPP
#define DIAGNOSTICS_HPP

#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>

struct SourceRange {
    int line = 1;
    int column = 1;
    int length = 1;
};

class CompileError : public std::runtime_error {
public:
    std::string stage;
    SourceRange range;

    CompileError(std::string stage_name, std::string message, SourceRange source_range)
        : std::runtime_error(std::move(message)),
          stage(std::move(stage_name)),
          range(source_range) {}
};

inline std::string source_line_text(const std::string& source, int target_line) {
    if (target_line < 1) {
        return "";
    }

    std::istringstream input(source);
    std::string line;
    for (int current = 1; std::getline(input, line); ++current) {
        if (current == target_line) {
            return line;
        }
    }
    return "";
}

inline std::string format_compile_error(const CompileError& error, const std::string& source) {
    std::ostringstream out;
    out << error.stage << " error at line " << error.range.line
        << ", column " << error.range.column << ": " << error.what();

    std::string line_text = source_line_text(source, error.range.line);
    if (!line_text.empty()) {
        int safe_column = std::max(1, error.range.column);
        int safe_length = std::max(1, error.range.length);
        out << "\n" << line_text << "\n"
            << std::string(static_cast<size_t>(safe_column - 1), ' ')
            << std::string(static_cast<size_t>(safe_length), '^');
    }

    return out.str();
}

#endif
