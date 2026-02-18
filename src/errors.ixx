// error formatting + diagnostics

export module opus.errors;

import opus.types;
import std;

export namespace opus {

// ansi colors
namespace color {
    constexpr const char* reset = "\033[0m";
    constexpr const char* bold = "\033[1m";
    constexpr const char* red = "\033[31m";
    constexpr const char* green = "\033[32m";
    constexpr const char* yellow = "\033[33m";
    constexpr const char* blue = "\033[34m";
    constexpr const char* magenta = "\033[35m";
    constexpr const char* cyan = "\033[36m";
    constexpr const char* white = "\033[37m";
    constexpr const char* bright_red = "\033[91m";
    constexpr const char* bright_green = "\033[92m";
    constexpr const char* bright_yellow = "\033[93m";
    constexpr const char* bright_blue = "\033[94m";
}

// Error severity levels
enum class Severity {
    Error,
    Warning,
    Note,
    Help
};

// Rich error with all context
struct RichError {
    Severity severity = Severity::Error;
    std::string message;
    SourceLoc loc;
    std::string source_line;
    std::size_t highlight_start;
    std::size_t highlight_len;
    std::string suggestion;
    std::vector<std::string> notes;
};

// Format a rich error for display
std::string format_error(const RichError& err) {
    std::string out;
    
    // header: file:line:col: severity: message
    const char* sev_color = color::bright_red;
    const char* sev_text = "error";
    switch (err.severity) {
        case Severity::Warning: sev_color = color::bright_yellow; sev_text = "warning"; break;
        case Severity::Note: sev_color = color::bright_blue; sev_text = "note"; break;
        case Severity::Help: sev_color = color::bright_green; sev_text = "help"; break;
        default: break;
    }
    
    out += std::format("{}{}{}:{}:{}: {}{}{}: {}{}{}\n",
        color::bold, color::white, 
        err.loc.file.empty() ? "<source>" : err.loc.file,
        err.loc.line, err.loc.column,
        sev_color, sev_text, color::reset,
        color::bold, err.message, color::reset);
    
    if (!err.source_line.empty()) {
        std::string line_num = std::format("{}", err.loc.line);
        std::string padding(line_num.size(), ' ');
        
        out += std::format(" {} {}|{}\n", padding, color::blue, color::reset);
        out += std::format(" {}{}{} |{} {}\n", 
            color::blue, line_num, color::reset, color::reset, err.source_line);
        
        // underline
        std::string underline(err.highlight_start > 0 ? err.highlight_start - 1 : 0, ' ');
        std::string carets(std::max(err.highlight_len, std::size_t(1)), '^');
        out += std::format(" {} {}|{} {}{}{}{}\n",
            padding, color::blue, color::reset,
            underline, sev_color, carets, color::reset);
    }
    
    // Suggestion
    if (!err.suggestion.empty()) {
        out += std::format(" {} {}= {}help:{} {}\n",
            std::string(std::format("{}", err.loc.line).size(), ' '),
            color::bright_green, color::bold, color::reset, err.suggestion);
    }
    
    // notes
    for (const auto& note : err.notes) {
        out += std::format(" {} {}= {}note:{} {}\n",
            std::string(std::format("{}", err.loc.line).size(), ' '),
            color::bright_blue, color::bold, color::reset, note);
    }
    
    return out;
}

// Get a line from source by line number (1-indexed)
std::string get_source_line(std::string_view source, std::size_t line_num) {
    std::size_t current_line = 1;
    std::size_t start = 0;
    
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (current_line == line_num) {
            start = i;
            while (i < source.size() && source[i] != '\n' && source[i] != '\r') {
                ++i;
            }
            return std::string(source.substr(start, i - start));
        }
        if (source[i] == '\n') {
            ++current_line;
        }
    }
    return "";
}

// Format a simple error (when we don't have rich context)
std::string format_simple_error(const std::string& message, const SourceLoc& loc, 
                                 std::string_view source = "") {
    RichError err;
    err.message = message;
    err.loc = loc;
    
    if (!source.empty() && loc.line > 0) {
        err.source_line = get_source_line(source, loc.line);
        err.highlight_start = loc.column;
        err.highlight_len = 1;
    }
    
    return format_error(err);
}

// levenshtein distance for "did you mean" suggestions
std::size_t edit_distance(std::string_view a, std::string_view b) {
    std::vector<std::vector<std::size_t>> dp(a.size() + 1, std::vector<std::size_t>(b.size() + 1));
    
    for (std::size_t i = 0; i <= a.size(); ++i) dp[i][0] = i;
    for (std::size_t j = 0; j <= b.size(); ++j) dp[0][j] = j;
    
    for (std::size_t i = 1; i <= a.size(); ++i) {
        for (std::size_t j = 1; j <= b.size(); ++j) {
            std::size_t cost = (a[i-1] == b[j-1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i-1][j] + 1,
                dp[i][j-1] + 1,
                dp[i-1][j-1] + cost
            });
        }
    }
    return dp[a.size()][b.size()];
}

// find closest match from candidates
std::optional<std::string> find_closest_match(std::string_view input, 
                                               const std::vector<std::string>& candidates,
                                               std::size_t max_distance = 3) {
    std::string best;
    std::size_t best_dist = max_distance + 1;
    
    for (const auto& candidate : candidates) {
        auto dist = edit_distance(input, candidate);
        if (dist < best_dist) {
            best_dist = dist;
            best = candidate;
        }
    }
    
    if (best_dist <= max_distance) {
        return best;
    }
    return std::nullopt;
}

} // namespace opus
