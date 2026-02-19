// error formatting + diagnostics

export module opus.errors;

import opus.types;
import std;

export namespace opus {

// ansi colors
namespace color {
    constexpr std::string_view reset = "\033[0m";
    constexpr std::string_view bold = "\033[1m";
    constexpr std::string_view red = "\033[31m";
    constexpr std::string_view green = "\033[32m";
    constexpr std::string_view yellow = "\033[33m";
    constexpr std::string_view blue = "\033[34m";
    constexpr std::string_view magenta = "\033[35m";
    constexpr std::string_view cyan = "\033[36m";
    constexpr std::string_view white = "\033[37m";
    constexpr std::string_view bright_red = "\033[91m";
    constexpr std::string_view bright_green = "\033[92m";
    constexpr std::string_view bright_yellow = "\033[93m";
    constexpr std::string_view bright_blue = "\033[94m";
}

enum class Severity {
    Error,
    Warning,
    Note,
    Help
};

struct RichError {
    Severity severity = Severity::Error;
    std::string message;
    SourceLoc loc;
    std::string source_line;
    std::size_t highlight_start = 0;
    std::size_t highlight_len = 0;
    std::string suggestion;
    std::vector<std::string> notes;
};

std::string format_error(const RichError& err) {
    std::string out;
    
    std::string_view sev_color = color::bright_red;
    std::string_view sev_text = "error";
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
    
    std::string line_num = std::format("{}", err.loc.line);
    std::string padding(line_num.size(), ' ');
    
    if (!err.source_line.empty()) {
        out += std::format(" {} {}|{}\n", padding, color::blue, color::reset);
        out += std::format(" {}{}{} |{} {}\n", 
            color::blue, line_num, color::reset, color::reset, err.source_line);
        
        std::string underline(err.highlight_start > 0 ? err.highlight_start - 1 : 0, ' ');
        std::string carets(std::max(err.highlight_len, std::size_t(1)), '^');
        out += std::format(" {} {}|{} {}{}{}{}\n",
            padding, color::blue, color::reset,
            underline, sev_color, carets, color::reset);
    }
    
    if (!err.suggestion.empty()) {
        out += std::format(" {} {}= {}help:{} {}\n",
            padding, color::bright_green, color::bold, color::reset, err.suggestion);
    }
    
    for (const auto& note : err.notes) {
        out += std::format(" {} {}= {}note:{} {}\n",
            padding, color::bright_blue, color::bold, color::reset, note);
    }
    
    return out;
}

// line_num is 1-indexed
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
    // last line with no trailing newline
    if (current_line == line_num) {
        return std::string(source.substr(start));
    }
    return "";
}

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

// two-row edit distance, O(n) memory instead of O(m*n)
std::size_t edit_distance(std::string_view a, std::string_view b) {
    const auto m = a.size();
    const auto n = b.size();
    std::vector<std::size_t> prev(n + 1);
    std::vector<std::size_t> curr(n + 1);

    for (std::size_t j = 0; j <= n; ++j) prev[j] = j;

    for (std::size_t i = 1; i <= m; ++i) {
        curr[0] = i;
        for (std::size_t j = 1; j <= n; ++j) {
            std::size_t cost = (a[i-1] == b[j-1]) ? 0 : 1;
            curr[j] = std::min({
                prev[j] + 1,
                curr[j-1] + 1,
                prev[j-1] + cost
            });
        }
        std::swap(prev, curr);
    }
    return prev[n];
}

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

// collects errors and warnings for batch reporting
class DiagnosticSink {
    std::vector<RichError> errors_;
    std::vector<RichError> warnings_;
public:
    void emit(RichError err) {
        if (err.severity == Severity::Warning)
            warnings_.push_back(std::move(err));
        else
            errors_.push_back(std::move(err));
    }

    bool has_errors() const { return !errors_.empty(); }
    std::size_t error_count() const { return errors_.size(); }

    std::string format_all() const {
        std::string out;
        for (const auto& w : warnings_)
            out += format_error(w);
        for (const auto& e : errors_)
            out += format_error(e);
        return out;
    }
};

} // namespace opus
