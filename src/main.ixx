// sable compiler entry point

module;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

export module main;

import sable;
import sable.pe;
import sable.project;
import std;

void print_usage() {
    std::print("{}", R"(Sable Compiler v1.1

write code
compile to native x64
ship the exe or inject the dll

Usage:
  sable [options] <file.sb>
  sable repl
  sable build [path]
  sable inspect <file.sb> [--line N] [--json]

Output:
  default       standalone .exe
  --dll         native Windows DLL
  --run, -r     build a temporary exe and run it
  --timings     print compile/build timing breakdown
  --quiet, -q   suppress normal compile output
  --fast        skip project discovery for single-file compiles

Project:
  build         find sable.project and build it
  build <path>  build a specific sable.project
  inspect       compile a file fast and report function bytes/metadata

Help:
  --help, -h    show this message

The language surface can mix styles in one file:
  function int add(int a, int b) { return a + b }
  fn add(a: int, b: int) -> int { return a + b }
  int add(int a, int b) { return a + b }
  define function add with left as i64, right as i64 returning i64
      return left + right
  end function

Sable is not a vm and not an llvm front-end cosplay.
)");
}

void print_banner() {
    std::println(R"(
   _____       __    __     
  / ___/____ _/ /_  / /__   
  \__ \/ __ `/ __ \/ / _ \  
 ___/ / /_/ / /_/ / /  __/  
/____/\__,_/_.___/_/\___/             

 A Programming Language written from scratch
===========================================
)");
}

// returns empty string on success, error message on failure
[[nodiscard]] static std::string write_binary_file(const std::string& path, const std::uint8_t* data, std::size_t size) {
    auto output_path = std::filesystem::path(path);
    if (output_path.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(output_path.parent_path(), ec);
        if (ec) {
            return std::format("cannot create output directory for '{}': {}", path, ec.message());
        }
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return std::format("cannot write to '{}'", path);
    }
    out.write(reinterpret_cast<const char*>(data), size);
    out.close();
    if (!out) {
        return std::format("write failed for '{}'", path);
    }
    return {};
}

[[nodiscard]] static std::expected<std::string, std::string> read_text_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return std::unexpected(std::format("cannot open file: {}", path.string()));
    }

    auto end = file.tellg();
    if (end < 0) {
        return std::unexpected(std::format("cannot size file: {}", path.string()));
    }

    std::string text(static_cast<std::size_t>(end), '\0');
    file.seekg(0, std::ios::beg);
    if (!text.empty()) {
        file.read(text.data(), static_cast<std::streamsize>(text.size()));
    }
    if (!file && !text.empty()) {
        return std::unexpected(std::format("cannot read file: {}", path.string()));
    }
    return text;
}

[[nodiscard]] static std::filesystem::path normalize_path(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return canonical;
    }
    auto absolute = std::filesystem::absolute(path, ec);
    if (!ec) {
        return absolute;
    }
    return path;
}

static void push_unique_path(std::vector<std::string>& paths, const std::filesystem::path& path) {
    auto normalized = normalize_path(path).generic_string();
    if (normalized.empty()) {
        return;
    }
    if (std::find(paths.begin(), paths.end(), normalized) == paths.end()) {
        paths.push_back(std::move(normalized));
    }
}

[[nodiscard]] static std::vector<std::string> default_import_search_paths(const std::filesystem::path& compiler_exe) {
    static std::unordered_map<std::string, std::vector<std::string>> cache;
    std::vector<std::string> paths;
    auto normalized_compiler = normalize_path(compiler_exe).generic_string();
    if (auto it = cache.find(normalized_compiler); it != cache.end()) {
        return it->second;
    }
    auto dir = std::filesystem::path(normalized_compiler).parent_path();
    for (int i = 0; i < 8 && !dir.empty(); ++i) {
        auto candidate = dir / "stdlib";
        if (std::filesystem::exists(candidate / "mem.sb") || std::filesystem::exists(candidate / "mem.op")) {
            push_unique_path(paths, candidate);
        }
        auto parent = dir.parent_path();
        if (parent == dir) {
            break;
        }
        dir = parent;
    }
    cache[normalized_compiler] = paths;
    return paths;
}

[[nodiscard]] static std::optional<std::size_t> require_main_offset(
    const sable::Compiler::Result& result
) {
    if (result.main_offset) {
        return result.main_offset;
    }
    if (auto it = result.function_offsets.find("main"); it != result.function_offsets.end()) {
        return it->second;
    }
    return std::nullopt;
}

[[nodiscard]] static sable::OutputKind project_mode_to_output_kind(std::string_view mode) {
    if (mode == "dll") return sable::OutputKind::Dll;
    if (mode == "exe") return sable::OutputKind::Exe;
    return sable::OutputKind::Raw;
}

static void print_timing_block(std::string_view title, const std::vector<sable::TimingEntry>& timings) {
    if (timings.empty()) {
        return;
    }
    std::println("\n\033[1;95m{} timings:\033[0m", title);
    for (const auto& timing : timings) {
        std::println("  {:<30} {:>8.3f} ms", timing.name, timing.ms);
    }
}

[[nodiscard]] static std::string json_escape(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 16);
    for (char ch : value) {
        switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out += std::format("\\u{:04X}", static_cast<unsigned int>(static_cast<unsigned char>(ch)));
                } else {
                    out += ch;
                }
                break;
        }
    }
    return out;
}

[[nodiscard]] static std::string bytes_to_hex_string(std::span<const std::uint8_t> bytes) {
    if (bytes.empty()) {
        return {};
    }

    std::string out;
    out.reserve(bytes.size() * 3);
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            out.push_back(' ');
        }
        out += std::format("{:02X}", bytes[i]);
    }
    return out;
}

[[nodiscard]] static std::string bytes_to_hex_dump(std::span<const std::uint8_t> bytes, std::size_t base_offset = 0) {
    if (bytes.empty()) {
        return "(no bytes)";
    }

    std::string out;
    for (std::size_t offset = 0; offset < bytes.size(); offset += 16) {
        auto slice = bytes.subspan(offset, std::min<std::size_t>(16, bytes.size() - offset));
        out += std::format("{:04X}  ", base_offset + offset);

        for (std::size_t i = 0; i < 16; ++i) {
            if (i < slice.size()) {
                out += std::format("{:02X}", slice[i]);
            } else {
                out += "  ";
            }
            if (i != 15) {
                out.push_back(' ');
            }
            if (i == 7) {
                out.push_back(' ');
            }
        }

        out += " |";
        for (std::uint8_t byte : slice) {
            out.push_back((byte >= 32 && byte <= 126) ? static_cast<char>(byte) : '.');
        }
        for (std::size_t i = slice.size(); i < 16; ++i) {
            out.push_back(' ');
        }
        out += "|\n";
    }
    return out;
}

[[nodiscard]] static const sable::FunctionArtifact* choose_inspect_function(
    const sable::Compiler::Result& result,
    const std::filesystem::path& file_path,
    std::optional<std::uint32_t> selected_line
) {
    const auto normalized_target = normalize_path(file_path).generic_string();
    const sable::FunctionArtifact* best = nullptr;

    for (const auto& fn : result.functions) {
        if (fn.is_extern || fn.code_size == 0) {
            continue;
        }
        auto normalized_source = normalize_path(fn.source_file).generic_string();
        if (!normalized_source.empty() && normalized_source != normalized_target) {
            continue;
        }

        if (!selected_line) {
            if (fn.name == "main") {
                return &fn;
            }
            if (!best) {
                best = &fn;
            }
            continue;
        }

        const auto line = *selected_line;
        const auto start = fn.source_line_start;
        const auto end = fn.source_line_end == 0 ? fn.source_line_start : fn.source_line_end;
        if (line < start || line > end) {
            continue;
        }

        if (!best) {
            best = &fn;
            continue;
        }

        const auto best_span = (best->source_line_end == 0 ? best->source_line_start : best->source_line_end) - best->source_line_start;
        const auto fn_span = end - start;
        if (fn_span <= best_span) {
            best = &fn;
        }
    }

    return best;
}

[[nodiscard]] int inspect_file(const std::string& filename, std::optional<std::uint32_t> selected_line, bool json_output, bool timings, const std::filesystem::path& compiler_exe) {
    auto source_result = read_text_file(filename);
    if (!source_result) {
        if (json_output) {
            std::println(R"({{"success":false,"error":"cannot open file"}})");
        } else {
            std::print(std::cerr, "\033[1;91merror\033[0m: cannot open file: \033[1m{}\033[0m\n", filename);
        }
        return 1;
    }
    std::string source = std::move(*source_result);

    sable::Compiler compiler;
    auto import_search_paths = default_import_search_paths(compiler_exe);
    auto source_path = normalize_path(filename);
    auto source_root = source_path.has_parent_path() ? source_path.parent_path().generic_string() : std::string{};

    if (source_path.has_parent_path()) {
        if (auto project_file = sable::find_project_file(source_path.parent_path())) {
            if (auto config_result = sable::load_project(*project_file)) {
                const auto& config = *config_result;
                source_root = std::filesystem::weakly_canonical(config.project_dir).string();
                for (const auto& include_path : config.includes) {
                    if (std::find(import_search_paths.begin(), import_search_paths.end(), include_path) == import_search_paths.end()) {
                        import_search_paths.push_back(include_path);
                    }
                }
            }
        }
    }

    auto result = compiler.compile({
        .source = source,
        .filename = filename,
        .project_root = source_root,
        .import_search_paths = import_search_paths,
        .output_kind = sable::OutputKind::Exe,
        .healing_mode = sable::ast::HealingMode::Off,
        .debug_info = false,
        .collect_line_map = true,
        .collect_function_artifacts = true,
        .collect_timings = timings,
    });

    if (!result.success) {
        if (json_output) {
            std::string joined;
            for (std::size_t i = 0; i < result.errors.size(); ++i) {
                if (i != 0) joined += "\n";
                joined += result.errors[i];
            }
            std::println("{{\"success\":false,\"error\":\"{}\"}}", json_escape(joined));
        } else {
            for (const auto& err : result.errors) {
                std::println("{}", err);
            }
        }
        return 1;
    }

    const auto* selected_fn = choose_inspect_function(result, source_path, selected_line);

    if (!json_output) {
        std::println("Compiled {} bytes", result.code.size());
        if (selected_fn) {
            std::println("Selected function: {} ({} bytes)", selected_fn->name, selected_fn->code_size);
            auto start = selected_fn->code_offset;
            if (start > result.code.size()) {
                start = result.code.size();
            }
            auto remaining = result.code.size() - start;
            auto size = selected_fn->code_size;
            if (size > remaining) {
                size = remaining;
            }
            auto bytes = std::span<const std::uint8_t>(result.code).subspan(start, size);
            std::print("{}", bytes_to_hex_dump(bytes, start));
        }
        return 0;
    }

    std::print("{{\"success\":true,\"file\":\"{}\",\"line\":{},\"code_size\":{},\"code_hex\":\"{}\",\"line_map\":[",
        json_escape(source_path.generic_string()),
        selected_line.value_or(0),
        result.code.size(),
        json_escape(bytes_to_hex_string(result.code)));

    for (std::size_t i = 0; i < result.line_map.size(); ++i) {
        const auto& entry = result.line_map[i];
        if (i != 0) std::print(",");
        std::print("{{\"code_offset\":{},\"line\":{}}}", entry.code_offset, entry.line);
    }

    std::print("],\"functions\":[");

    for (std::size_t i = 0; i < result.functions.size(); ++i) {
        const auto& fn = result.functions[i];
        if (i != 0) std::print(",");
        std::print(
            "{{\"name\":\"{}\",\"source_file\":\"{}\",\"source_line_start\":{},\"source_line_end\":{},\"code_offset\":{},\"code_size\":{},\"is_extern\":{}}}",
            json_escape(fn.name),
            json_escape(normalize_path(fn.source_file).generic_string()),
            fn.source_line_start,
            fn.source_line_end,
            fn.code_offset,
            fn.code_size,
            fn.is_extern ? "true" : "false"
        );
    }

    std::print("],\"selected_function\":");
    if (!selected_fn) {
        std::print("null");
    } else {
        auto safe_offset = selected_fn->code_offset;
        if (safe_offset > result.code.size()) {
            safe_offset = result.code.size();
        }
        auto safe_size = selected_fn->code_size;
        auto remaining = result.code.size() - safe_offset;
        if (safe_size > remaining) {
            safe_size = remaining;
        }
        auto bytes = std::span<const std::uint8_t>(result.code).subspan(safe_offset, safe_size);
        std::vector<sable::pe::LineMapEntry> selected_line_map;
        for (const auto& entry : result.line_map) {
            if (entry.code_offset < safe_offset) {
                continue;
            }
            if (entry.code_offset >= safe_offset + safe_size) {
                continue;
            }
            selected_line_map.push_back(entry);
        }
        std::print(
            "{{\"name\":\"{}\",\"source_file\":\"{}\",\"source_line_start\":{},\"source_line_end\":{},\"code_offset\":{},\"code_size\":{},\"bytes_hex\":\"{}\",\"hex_dump\":\"{}\",\"line_map\":[",
            json_escape(selected_fn->name),
            json_escape(normalize_path(selected_fn->source_file).generic_string()),
            selected_fn->source_line_start,
            selected_fn->source_line_end,
            selected_fn->code_offset,
            selected_fn->code_size,
            json_escape(bytes_to_hex_string(bytes)),
            json_escape(bytes_to_hex_dump(bytes, selected_fn->code_offset))
        );
        for (std::size_t i = 0; i < selected_line_map.size(); ++i) {
            const auto& entry = selected_line_map[i];
            if (i != 0) {
                std::print(",");
            }
            std::print("{{\"code_offset\":{},\"line\":{}}}", entry.code_offset, entry.line);
        }
        std::print("]}}");
    }
    std::println("}}");
    return 0;
}

[[nodiscard]] int run_file(const std::string& filename, bool run_immediately, sable::OutputKind output_kind, bool quiet, bool timings, bool fast_mode, const std::filesystem::path& compiler_exe) {
    auto now = [] { return std::chrono::steady_clock::now(); };
    auto elapsed_ms = [](auto start, auto end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };
    auto total_start = now();
    std::vector<sable::TimingEntry> stage_timings;
    auto add_stage_timing = [&](std::string_view name, double ms) {
        stage_timings.push_back(sable::TimingEntry{
            .name = std::string(name),
            .ms = ms
        });
    };

    auto read_start = now();
    auto source_result = read_text_file(filename);
    if (!source_result) {
        std::print(std::cerr, "\033[1;91merror\033[0m: cannot open file: \033[1m{}\033[0m\n", filename);
        return 1;
    }
    std::string source = std::move(*source_result);
    if (timings) {
        add_stage_timing("driver.read_source", elapsed_ms(read_start, now()));
    }

    sable::Compiler compiler;
    auto import_search_paths = default_import_search_paths(compiler_exe);
    auto source_path = normalize_path(filename);
    auto source_root = source_path.has_parent_path() ? source_path.parent_path().generic_string() : std::string{};
    bool project_debug = false;
    auto project_healing = sable::ast::HealingMode::Off;

    auto discovery_start = now();
    if (!fast_mode && source_path.has_parent_path()) {
        if (auto project_file = sable::find_project_file(source_path.parent_path())) {
            if (auto config_result = sable::load_project(*project_file)) {
                const auto& config = *config_result;
                source_root = std::filesystem::weakly_canonical(config.project_dir).string();
                project_debug = config.debug;
                project_healing = config.healing.value_or(sable::ast::HealingMode::Off);
                for (const auto& include_path : config.includes) {
                    if (std::find(import_search_paths.begin(), import_search_paths.end(), include_path) == import_search_paths.end()) {
                        import_search_paths.push_back(include_path);
                    }
                }
            }
        }
    }
    if (timings) {
        add_stage_timing("driver.project_discovery", elapsed_ms(discovery_start, now()));
    }

    if (run_immediately) {
        auto compile_run_start = now();
        auto result = compiler.compile_and_run({
            .source = source,
            .filename = filename,
            .project_root = source_root,
            .import_search_paths = import_search_paths,
            .healing_mode = project_healing,
            .debug_info = project_debug,
            .collect_timings = timings,
            .collect_function_offsets = !quiet,
        });
        if (timings) {
            add_stage_timing("driver.compile_and_run", elapsed_ms(compile_run_start, now()));
            add_stage_timing("driver.total", elapsed_ms(total_start, now()));
            print_timing_block("driver", stage_timings);
        }
        if (result) {
            std::println("Program returned: {}", *result);
            return 0;
        } else {
            std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", result.error());
            return 1;
        }
    } else {
        auto compile_start = now();
        auto result = compiler.compile({
            .source = source,
            .filename = filename,
            .project_root = source_root,
            .import_search_paths = import_search_paths,
            .output_kind = output_kind,
            .healing_mode = project_healing,
            .debug_info = project_debug,
            .collect_timings = timings,
            .collect_function_offsets = !quiet,
        });
        if (timings) {
            add_stage_timing("driver.compile_call", elapsed_ms(compile_start, now()));
        }
        if (result.success) {
            for (const auto& warning : result.warnings) {
                std::println("\033[1;93mwarning:\033[0m {}", warning);
            }
            std::string outname = filename;
            if (outname.ends_with(".sb") || outname.ends_with(".op")) {
                outname = outname.substr(0, outname.size() - 3);
            }
            
            if (sable::output_is_dll(output_kind)) {
                outname += ".dll";
                auto main_offset = require_main_offset(result);
                if (!main_offset) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: no 'main' function found; cannot generate a DLL entry point\n");
                    return 1;
                }
                
                sable::pe::PeImageGenerator dll_gen;
                auto pe_start = now();
                auto dll_bytes = dll_gen.generate({
                    .code = result.code,
                    .main_offset = *main_offset,
                    .alloc_console = true,
                    .iat_fixups = result.iat_fixups,
                    .debug_source = project_debug ? source : std::string{},
                    .line_map = project_debug ? result.line_map : std::vector<sable::pe::LineMapEntry>{},
                    .healing_mode = project_healing,
                    .output_kind = sable::OutputKind::Dll,
                    .writable_text = result.needs_writable_text,
                });
                if (timings) {
                    add_stage_timing("driver.pe_generate", elapsed_ms(pe_start, now()));
                }
                
                if (!quiet) {
                    auto validate_start = now();
                    auto pe_errors = sable::pe::PeImageGenerator::validate_pe(dll_bytes);
                    for (const auto& err : pe_errors) {
                        std::println("\033[1;93mPE warning:\033[0m {}", err);
                    }
                    if (timings) {
                        add_stage_timing("driver.pe_validate", elapsed_ms(validate_start, now()));
                    }
                }
                
                auto write_start = now();
                auto write_err = write_binary_file(outname, dll_bytes.data(), dll_bytes.size());
                if (!write_err.empty()) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", write_err);
                    return 1;
                }
                if (timings) {
                    add_stage_timing("driver.write_binary", elapsed_ms(write_start, now()));
                }
                
                if (!quiet) {
                    std::println("Generated DLL: {} ({} bytes)", outname, dll_bytes.size());
                    std::println("  Entry point: DllMain -> calls main()");
                    std::println("  Inject with LoadLibrary or any injector");
                }
                
            } else {
                outname += ".exe";
                auto main_offset = require_main_offset(result);
                if (!main_offset) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: no 'main' function found; cannot generate an executable entry point\n");
                    return 1;
                }
                
                sable::pe::PeImageGenerator exe_gen;
                auto pe_start = now();
                auto exe_bytes = exe_gen.generate({
                    .code = result.code,
                    .main_offset = *main_offset,
                    .alloc_console = false,
                    .iat_fixups = result.iat_fixups,
                    .debug_source = project_debug ? source : std::string{},
                    .line_map = project_debug ? result.line_map : std::vector<sable::pe::LineMapEntry>{},
                    .healing_mode = project_healing,
                    .output_kind = sable::OutputKind::Exe,
                    .writable_text = result.needs_writable_text,
                });
                if (timings) {
                    add_stage_timing("driver.pe_generate", elapsed_ms(pe_start, now()));
                }
                
                if (!quiet) {
                    auto validate_start = now();
                    auto pe_errors = sable::pe::PeImageGenerator::validate_pe(exe_bytes);
                    for (const auto& err : pe_errors) {
                        std::println("\033[1;93mPE warning:\033[0m {}", err);
                    }
                    if (timings) {
                        add_stage_timing("driver.pe_validate", elapsed_ms(validate_start, now()));
                    }
                }
                
                auto write_start = now();
                auto write_err = write_binary_file(outname, exe_bytes.data(), exe_bytes.size());
                if (!write_err.empty()) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", write_err);
                    return 1;
                }
                if (timings) {
                    add_stage_timing("driver.write_binary", elapsed_ms(write_start, now()));
                }
                
                if (!quiet) {
                    std::println("Generated EXE: {} ({} bytes)", outname, exe_bytes.size());
                    std::println("  Entry point: exe startup -> calls main()");
                }
            }
            
            if (!quiet) {
                std::println("Functions:");
                for (const auto& [name, offset] : result.function_offsets) {
                    std::println("  {} @ 0x{:x}", name, offset);
                }
            }

            if (timings) {
                add_stage_timing("driver.total", elapsed_ms(total_start, now()));
                print_timing_block("driver", stage_timings);
            }

            return 0;
        } else {
            if (result.errors.empty()) {
                std::print(std::cerr, "\033[1;91minternal error\033[0m: compilation failed without a diagnostic\n");
                return 1;
            }
            for (const auto& err : result.errors) {
                // try to extract file:line:col from error string
                std::print(std::cerr, "{}\n", err);
                
                std::size_t first_colon = err.find(':');
                if (first_colon != std::string::npos) {
                    std::size_t second_colon = err.find(':', first_colon + 1);
                    if (second_colon != std::string::npos) {
                        try {
                            std::size_t line_num = std::stoull(err.substr(first_colon + 1, second_colon - first_colon - 1));
                            std::string src_line = sable::get_source_line(source, line_num);
                            if (!src_line.empty()) {
                                std::string line_str = std::format("{}", line_num);
                                std::string padding(line_str.size(), ' ');
                                std::print(std::cerr, " {} \033[34m|\033[0m\n", padding);
                                std::print(std::cerr, " \033[34m{}\033[0m \033[34m|\033[0m {}\n", line_str, src_line);
                                
                                std::size_t third_colon = err.find(':', second_colon + 1);
                                if (third_colon != std::string::npos) {
                                    std::size_t col = std::stoull(err.substr(second_colon + 1, third_colon - second_colon - 1));
                                    std::string spaces(col > 0 ? col - 1 : 0, ' ');
                                    std::print(std::cerr, " {} \033[34m|\033[0m {}\033[91m^\033[0m\n", padding, spaces);
                                }
                            }
                        } catch (...) {
                        }
                    }
                }
                std::print(std::cerr, "\n");
            }
            return 1;
        }
    }
}

[[nodiscard]] int build_project(std::optional<std::string> project_path_arg, bool timings, const std::filesystem::path& compiler_exe) {
    auto now = [] { return std::chrono::steady_clock::now(); };
    auto elapsed_ms = [](auto start, auto end) {
        return std::chrono::duration<double, std::milli>(end - start).count();
    };
    auto total_start = now();
    std::vector<sable::TimingEntry> stage_timings;
    auto add_stage_timing = [&](std::string_view name, double ms) {
        stage_timings.push_back(sable::TimingEntry{
            .name = std::string(name),
            .ms = ms
        });
    };

    std::filesystem::path project_file;
    
    if (project_path_arg) {
        project_file = *project_path_arg;
    } else {
        auto found = sable::find_project_file(std::filesystem::current_path());
        if (!found) {
            std::print(std::cerr, "\033[1;91merror\033[0m: no sable.project found in current directory or parents\n");
            return 1;
        }
        project_file = *found;
    }
    
    std::println("Loading project: {}", project_file.string());
    
    auto load_project_start = now();
    auto config_result = sable::load_project(project_file);
    if (timings) {
        add_stage_timing("driver.load_project", elapsed_ms(load_project_start, now()));
    }
    if (!config_result) {
        std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", config_result.error());
        return 1;
    }
    
    auto config = std::move(*config_result);
    
    if (config.entry.empty()) {
        std::print(std::cerr, "\033[1;91merror\033[0m: project must specify an 'entry' file\n");
        return 1;
    }
    
    std::println("Project: {} ({})", config.name, config.mode);
    std::println("Entry: {}", config.entry);
    
    std::filesystem::path entry_path = config.project_dir / config.entry;
    auto source_result = read_text_file(entry_path);
    if (!source_result) {
        std::print(std::cerr, "\033[1;91merror\033[0m: cannot read entry file: {}\n", entry_path.string());
        return 1;
    }
    auto read_start = now();
    std::string source = std::move(*source_result);
    if (timings) {
        add_stage_timing("driver.read_source", elapsed_ms(read_start, now()));
    }
    std::vector<std::string> import_search_paths = default_import_search_paths(compiler_exe);
    for (const auto& include_path : config.includes) {
        import_search_paths.push_back(include_path);
    }
    std::string project_root = std::filesystem::weakly_canonical(config.project_dir).string();
    
    sable::Compiler compiler;
    auto output_kind = project_mode_to_output_kind(config.mode);
    if (!sable::output_is_native_image(output_kind)) {
        std::print(std::cerr, "\033[1;91merror\033[0m: invalid project mode '{}'; expected 'dll' or 'exe'\n", config.mode);
        return 1;
    }
    auto compile_start = now();
    auto result = compiler.compile({
        .source = source,
        .filename = entry_path.string(),
        .project_root = project_root,
        .import_search_paths = import_search_paths,
        .output_kind = output_kind,
        .healing_mode = config.healing.value_or(sable::ast::HealingMode::Off),
        .debug_info = config.debug,
        .collect_timings = timings,
    });
    if (timings) {
        add_stage_timing("driver.compile_call", elapsed_ms(compile_start, now()));
    }
    
    if (result.success) {
        for (const auto& warning : result.warnings) {
            std::println("\033[1;93mwarning:\033[0m {}", warning);
        }
        std::string outname = config.output;
        if (outname.empty()) {
            outname = config.name;
            if (sable::output_is_dll(output_kind)) outname += ".dll";
            else if (sable::output_is_exe(output_kind)) outname += ".exe";
            else outname += ".sable";
        }
        
        std::filesystem::path output_path = config.project_dir / outname;
        
        if (sable::output_is_native_image(output_kind)) {
            auto main_offset = require_main_offset(result);
            if (!main_offset) {
                std::print(std::cerr, "\033[1;91merror\033[0m: no 'main' function found; cannot generate a {} entry point\n",
                    sable::output_is_exe(output_kind) ? "program" : "DLL");
                return 1;
            }
            
            std::string debug_source = config.debug ? source : "";
            
            sable::pe::PeImageGenerator pe_gen;
            auto pe_start = now();
            auto pe_bytes = pe_gen.generate({
                .code = result.code,
                .main_offset = *main_offset,
                .alloc_console = sable::output_is_dll(output_kind),
                .iat_fixups = result.iat_fixups,
                .debug_source = debug_source,
                .line_map = config.debug ? result.line_map : std::vector<sable::pe::LineMapEntry>{},
                .healing_mode = config.healing.value_or(sable::ast::HealingMode::Off),
                .output_kind = output_kind,
                .writable_text = result.needs_writable_text,
            });
            if (timings) {
                add_stage_timing("driver.pe_generate", elapsed_ms(pe_start, now()));
            }
            
            auto validate_start = now();
            auto pe_errors = sable::pe::PeImageGenerator::validate_pe(pe_bytes);
            for (const auto& err : pe_errors) {
                std::println("\033[1;93mPE warning:\033[0m {}", err);
            }
            if (timings) {
                add_stage_timing("driver.pe_validate", elapsed_ms(validate_start, now()));
            }
            
            auto write_start = now();
            auto write_err = write_binary_file(output_path.string(), pe_bytes.data(), pe_bytes.size());
            if (!write_err.empty()) {
                std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", write_err);
                return 1;
            }
            if (timings) {
                add_stage_timing("driver.write_binary", elapsed_ms(write_start, now()));
            }
            
            std::println("\n\033[1;92mBuild successful!\033[0m");
            std::println("Output: {} ({} bytes)", output_path.string(), pe_bytes.size());
            if (sable::output_is_exe(output_kind)) {
                std::println("Entry point: exe startup -> calls main()");
            } else {
                std::println("Entry point: DllMain -> calls main()");
            }
            if (config.debug) {
                std::println("\033[1;93mDebug mode:\033[0m Source in .src, line map in .srcmap ({} entries)", result.line_map.size());
            }
        } else {
            auto write_err = write_binary_file(output_path.string(), result.code.data(), result.code.size());
            if (!write_err.empty()) {
                std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", write_err);
                return 1;
            }
            
            std::println("\n\033[1;92mBuild successful!\033[0m");
            std::println("Output: {} ({} bytes)", output_path.string(), result.code.size());
        }
        
        std::println("Functions:");
        for (const auto& [name, offset] : result.function_offsets) {
            std::println("  {} @ 0x{:x}", name, offset);
        }

        if (timings) {
            add_stage_timing("driver.total", elapsed_ms(total_start, now()));
            print_timing_block("driver", stage_timings);
        }

        return 0;
    } else {
        std::println("\n\033[1;91mBuild failed!\033[0m");
        for (const auto& err : result.errors) {
            std::print(std::cerr, "{}\n", err);
        }
        return 1;
    }
}

export int main(int argc, char* argv[]) {
    try {
        // enable ansi escape codes on windows
        HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD console_mode = 0;
        GetConsoleMode(stdout_handle, &console_mode);
        SetConsoleMode(stdout_handle, console_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

        if (argc < 2) {
            print_usage();
            return 0;
        }

        std::string command = argv[1];

        auto compiler_exe = normalize_path(argv[0]);

        if (command == "--help" || command == "-h") {
            print_usage();
            return 0;
        }

        if (command == "repl") {
            print_banner();
            sable::Repl repl;
            repl.run();
            return 0;
        }

        if (command == "build") {
            std::optional<std::string> project_path;
            bool timings = false;
            if (argc > 2) {
                for (int i = 2; i < argc; ++i) {
                    std::string arg = argv[i];
                    if (arg == "--timings") {
                        timings = true;
                    } else if (!arg.starts_with("-") && !project_path) {
                        project_path = arg;
                    }
                }
            }
            return build_project(project_path, timings, compiler_exe);
        }

        if (command == "inspect") {
            std::string filename;
            std::optional<std::uint32_t> selected_line;
            bool json_output = false;
            bool timings = false;

            for (int i = 2; i < argc; ++i) {
                std::string arg = argv[i];
                if (arg == "--json") {
                    json_output = true;
                } else if (arg == "--timings") {
                    timings = true;
                } else if (arg == "--line" && i + 1 < argc) {
                    selected_line = static_cast<std::uint32_t>(std::stoul(argv[++i]));
                } else if (!arg.starts_with("-") && filename.empty()) {
                    filename = arg;
                }
            }

            if (filename.empty()) {
                std::println(std::cerr, "Error: no input file specified for inspect");
                return 1;
            }

            return inspect_file(filename, selected_line, json_output, timings, compiler_exe);
        }

        bool run = false;
        bool quiet = false;
        bool timings = false;
        bool fast_mode = false;
        auto output_kind = sable::OutputKind::Exe;
        std::string filename;
        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--run" || arg == "-r") {
                run = true;
            } else if (arg == "--timings") {
                timings = true;
            } else if (arg == "--fast") {
                fast_mode = true;
            } else if (arg == "--quiet" || arg == "-q") {
                quiet = true;
            } else if (arg == "--dll") {
                output_kind = sable::OutputKind::Dll;
            } else if (!arg.starts_with("-")) {
                filename = arg;
            }
        }

        if (filename.empty()) {
            std::println(std::cerr, "Error: no input file specified");
            return 1;
        }

        return run_file(filename, run, output_kind, quiet, timings, fast_mode, compiler_exe);
    } catch (const std::exception& ex) {
        std::print(std::cerr, "\033[1;91minternal error\033[0m: uncaught exception: {}\n", ex.what());
        return 1;
    } catch (...) {
        std::print(std::cerr, "\033[1;91minternal error\033[0m: uncaught non-standard exception\n");
        return 1;
    }
}

namespace opus = sable;
