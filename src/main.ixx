// opus compiler entry point

module;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

export module main;

import opus;
import opus.pe;
import opus.project;
import std;

void print_usage() {
    std::println(R"(Opus Compiler v0.1.0

Usage: opus [options] <file.op>
       opus repl

File Extensions:
  .op          Source code
  .opus        Compiled executable

Options:
  --run         Compile and run immediately
  --dll         Output as Windows DLL (injectable)
                (default: generates standalone .exe)
  --help        Show this help

Project Commands:
  build         Build project using opus.project
  build <path>  Build using specified project file

Syntax Modes (all can be mixed in one file!):
  C-style:   fn main() -> i32 {{ return 0; }}
  English:   define function main returning i32
               return 0
             end function
  AI:        fn:main>i32{{ret:0}})");
}

void print_banner() {
    std::println(R"(
   ____                  
  / __ \____  __  _______
 / / / / __ \/ / / / ___/
/ /_/ / /_/ / /_/ (__  ) 
\____/ .___/\__,_/____/  
    /_/                  

A Simple Programming Language
===================================
)");
}

// returns empty string on success, error message on failure
[[nodiscard]] static std::string write_binary_file(const std::string& path, const std::uint8_t* data, std::size_t size) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return std::format("cannot write to '{}' (file locked by another process?)", path);
    }
    out.write(reinterpret_cast<const char*>(data), size);
    out.close();
    if (!out) {
        return std::format("write failed for '{}'", path);
    }
    return {};
}

[[nodiscard]] int run_file(const std::string& filename, bool run_immediately, bool as_dll) {
    std::ifstream file(filename);
    if (!file) {
        std::print(std::cerr, "\033[1;91merror\033[0m: cannot open file: \033[1m{}\033[0m\n", filename);
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();

    opus::Compiler compiler;

    if (run_immediately) {
        auto result = compiler.compile_and_run(source);
        if (result) {
            std::println("Program returned: {}", *result);
            return 0;
        } else {
            std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", result.error());
            return 1;
        }
    } else {
        // compile - exe mode when not --dll
        auto result = compiler.compile({
            .source = source,
            .filename = filename,
            .dll_mode = as_dll,
            .healing_mode = opus::ast::HealingMode::Off,
            .exe_mode = !as_dll,
        });
        if (result.success) {
            std::string outname = filename;
            if (outname.ends_with(".op")) {
                outname = outname.substr(0, outname.size() - 3);
            }
            
            if (as_dll) {
                outname += ".dll";
                
                std::size_t main_offset = 0;
                if (auto it = result.function_offsets.find("main"); it != result.function_offsets.end()) {
                    main_offset = it->second;
                }
                
                opus::pe::DllGenerator dll_gen;
                auto dll_bytes = dll_gen.generate({
                    .code = result.code,
                    .main_offset = main_offset,
                    .alloc_console = true,
                    .iat_fixups = result.iat_fixups,
                    .healing_mode = opus::ast::HealingMode::Off,
                    .writable_text = result.needs_writable_text,
                });
                
                auto pe_errors = opus::pe::DllGenerator::validate_pe(dll_bytes);
                for (const auto& err : pe_errors) {
                    std::println("\033[1;93mPE warning:\033[0m {}", err);
                }
                
                auto write_err = write_binary_file(outname, dll_bytes.data(), dll_bytes.size());
                if (!write_err.empty()) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", write_err);
                    return 1;
                }
                
                std::println("Generated DLL: {} ({} bytes)", outname, dll_bytes.size());
                std::println("  Entry point: DllMain -> calls main()");
                std::println("  Inject with LoadLibrary or any injector");
                
            } else {
                outname += ".exe";
                
                std::size_t main_offset = 0;
                if (auto it = result.function_offsets.find("main"); it != result.function_offsets.end()) {
                    main_offset = it->second;
                }
                
                opus::pe::DllGenerator exe_gen;
                auto exe_bytes = exe_gen.generate({
                    .code = result.code,
                    .main_offset = main_offset,
                    .alloc_console = false,
                    .iat_fixups = result.iat_fixups,
                    .healing_mode = opus::ast::HealingMode::Off,
                    .exe_mode = true,
                    .writable_text = result.needs_writable_text,
                });
                
                auto pe_errors = opus::pe::DllGenerator::validate_pe(exe_bytes);
                for (const auto& err : pe_errors) {
                    std::println("\033[1;93mPE warning:\033[0m {}", err);
                }
                
                auto write_err = write_binary_file(outname, exe_bytes.data(), exe_bytes.size());
                if (!write_err.empty()) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", write_err);
                    return 1;
                }
                
                std::println("Generated EXE: {} ({} bytes)", outname, exe_bytes.size());
                std::println("  Entry point: exe startup -> calls main()");
            }
            
            std::println("Functions:");
            for (const auto& [name, offset] : result.function_offsets) {
                std::println("  {} @ 0x{:x}", name, offset);
            }
            
            return 0;
        } else {
            for (const auto& err : result.errors) {
                // try to extract file:line:col from error string
                std::print(std::cerr, "{}\n", err);
                
                std::size_t first_colon = err.find(':');
                if (first_colon != std::string::npos) {
                    std::size_t second_colon = err.find(':', first_colon + 1);
                    if (second_colon != std::string::npos) {
                        try {
                            std::size_t line_num = std::stoull(err.substr(first_colon + 1, second_colon - first_colon - 1));
                            std::string src_line = opus::get_source_line(source, line_num);
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

[[nodiscard]] int build_project(std::optional<std::string> project_path_arg) {
    std::filesystem::path project_file;
    
    if (project_path_arg) {
        project_file = *project_path_arg;
    } else {
        auto found = opus::find_project_file(std::filesystem::current_path());
        if (!found) {
            std::print(std::cerr, "\033[1;91merror\033[0m: no opus.project found in current directory or parents\n");
            return 1;
        }
        project_file = *found;
    }
    
    std::println("Loading project: {}", project_file.string());
    
    auto config_result = opus::load_project(project_file);
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
    std::ifstream in(entry_path);
    if (!in) {
        std::print(std::cerr, "\033[1;91merror\033[0m: cannot read entry file: {}\n", entry_path.string());
        return 1;
    }
    
    std::stringstream buffer;
    buffer << in.rdbuf();
    std::string source = buffer.str();
    std::vector<std::string> import_search_paths = config.includes;
    std::string project_root = std::filesystem::weakly_canonical(config.project_dir).string();
    
    opus::Compiler compiler;
    bool as_dll = (config.mode == "dll");
    bool as_exe = (config.mode == "exe");
    auto result = compiler.compile({
        .source = source,
        .filename = entry_path.string(),
        .project_root = project_root,
        .import_search_paths = import_search_paths,
        .dll_mode = as_dll,
        .healing_mode = config.healing.value_or(opus::ast::HealingMode::Off),
        .exe_mode = as_exe,
    });
    
    if (result.success) {
        std::string outname = config.output;
        if (outname.empty()) {
            outname = config.name;
            if (as_dll) outname += ".dll";
            else if (as_exe) outname += ".exe";
            else outname += ".opus";
        }
        
        std::filesystem::path output_path = config.project_dir / outname;
        
        if (as_dll || as_exe) {
            std::size_t main_offset = 0;
            if (auto it = result.function_offsets.find("main"); it != result.function_offsets.end()) {
                main_offset = it->second;
            }
            
            std::string debug_source = config.debug ? source : "";
            
            opus::pe::DllGenerator pe_gen;
            auto pe_bytes = pe_gen.generate({
                .code = result.code,
                .main_offset = main_offset,
                .alloc_console = as_dll,
                .iat_fixups = result.iat_fixups,
                .debug_source = debug_source,
                .line_map = result.line_map,
                .healing_mode = config.healing.value_or(opus::ast::HealingMode::Off),
                .exe_mode = as_exe,
                .writable_text = result.needs_writable_text,
            });
            
            auto pe_errors = opus::pe::DllGenerator::validate_pe(pe_bytes);
            for (const auto& err : pe_errors) {
                std::println("\033[1;93mPE warning:\033[0m {}", err);
            }
            
            auto write_err = write_binary_file(output_path.string(), pe_bytes.data(), pe_bytes.size());
            if (!write_err.empty()) {
                std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", write_err);
                return 1;
            }
            
            std::println("\n\033[1;92mBuild successful!\033[0m");
            std::println("Output: {} ({} bytes)", output_path.string(), pe_bytes.size());
            if (as_exe) {
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

    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }

    if (command == "repl") {
        print_banner();
        opus::Repl repl;
        repl.run();
        return 0;
    }

    if (command == "build") {
        std::optional<std::string> project_path;
        if (argc > 2) {
            project_path = argv[2];
        }
        return build_project(project_path);
    }

    bool run = false;
    bool dll = false;
    std::string filename;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--run" || arg == "-r") {
            run = true;
        } else if (arg == "--dll") {
            dll = true;
        } else if (!arg.starts_with("-")) {
            filename = arg;
        }
    }

    if (filename.empty()) {
        std::println(std::cerr, "Error: no input file specified");
        return 1;
    }

    return run_file(filename, run, dll);
}

