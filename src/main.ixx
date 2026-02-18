// Opus Programming Language
// Entry Point

module;

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

export module main;

import opus;
import opus.pe;
import opus.project;
import std;

void print_usage() {
    std::println("Opus Compiler v0.1.0");
    std::println("");
    std::println("Usage: opus [options] <file.op>");
    std::println("       opus repl");
    std::println("");
    std::println("File Extensions:");
    std::println("  .op          Source code");
    std::println("  .opus        Compiled executable");
    std::println("");
    std::println("Options:");
    std::println("  -o <file>     Output file name");
    std::println("  -c            Compile only (don't link)");
    std::println("  -S            Output assembly");
    std::println("  -O<level>     Optimization level (0-3)");
    std::println("  --run         Compile and run immediately");
    std::println("  --dll         Output as Windows DLL (injectable)");
    std::println("                (default: generates standalone .exe)");
    std::println("  --help        Show this help");
    std::println("");
    std::println("Project Commands:");
    std::println("  build         Build project using opus.project");
    std::println("  build <path>  Build using specified project file");
    std::println("");
    std::println("Syntax Modes (all can be mixed in one file!):");
    std::println("  C-style:   fn main() -> i32 {{ return 0; }}");
    std::println("  English:   define function main returning i32");
    std::println("               return 0");
    std::println("             end function");
    std::println("  AI:        fn:main>i32{{ret:0}}");
}

void print_banner() {
    std::println(R"(
   ____                  
  / __ \____  __  _______
 / / / / __ \/ / / / ___/
/ /_/ / /_/ / /_/ (__  ) 
\____/ .___/\__,_/____/  
    /_/                  
    )");
    std::println("A Simple Programming Language");
    std::println("===================================");
    std::println("");
}

int run_file(const std::string& filename, bool run_immediately, bool as_dll) {
    // Read file
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
        // Compile - exe mode when not --dll
        auto result = compiler.compile(source, filename, as_dll, "off", !as_dll);
        if (result.success) {
            std::string outname = filename;
            if (outname.ends_with(".op")) {
                outname = outname.substr(0, outname.size() - 3);
            }
            
            if (as_dll) {
                outname += ".dll";
                
                std::size_t main_offset = 0;
                if (result.function_offsets.contains("main")) {
                    main_offset = result.function_offsets["main"];
                }
                
                opus::pe::DllGenerator dll_gen;
                auto dll_bytes = dll_gen.generate(result.code, main_offset, true, result.iat_fixups, "", {}, "off", false, result.needs_writable_text);
                
                auto pe_errors = opus::pe::DllGenerator::validate_pe(dll_bytes);
                for (const auto& err : pe_errors) {
                    std::println("\033[1;93mPE warning:\033[0m {}", err);
                }
                
                std::ofstream out(outname, std::ios::binary);
                if (!out) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: cannot write to '{}' (file locked by another process?)\n", outname);
                    return 1;
                }
                out.write(reinterpret_cast<const char*>(dll_bytes.data()), 
                          dll_bytes.size());
                out.close();
                if (!out) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: write failed for '{}'\n", outname);
                    return 1;
                }
                
                std::println("Generated DLL: {} ({} bytes)", outname, dll_bytes.size());
                std::println("  Entry point: DllMain -> calls main()");
                std::println("  Inject with LoadLibrary or any injector");
                
            } else {
                // exe mode
                outname += ".exe";
                
                std::size_t main_offset = 0;
                if (result.function_offsets.contains("main")) {
                    main_offset = result.function_offsets["main"];
                }
                
                opus::pe::DllGenerator exe_gen;
                auto exe_bytes = exe_gen.generate(result.code, main_offset, true, result.iat_fixups, "", {}, "off", true, result.needs_writable_text);
                
                auto pe_errors = opus::pe::DllGenerator::validate_pe(exe_bytes);
                for (const auto& err : pe_errors) {
                    std::println("\033[1;93mPE warning:\033[0m {}", err);
                }
                
                std::ofstream out(outname, std::ios::binary);
                if (!out) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: cannot write to '{}' (file locked by another process?)\n", outname);
                    return 1;
                }
                out.write(reinterpret_cast<const char*>(exe_bytes.data()), 
                          exe_bytes.size());
                out.close();
                if (!out) {
                    std::print(std::cerr, "\033[1;91merror\033[0m: write failed for '{}'\n", outname);
                    return 1;
                }
                
                std::println("Generated EXE: {} ({} bytes)", outname, exe_bytes.size());
                std::println("  Entry point: exe startup -> calls main()");
            }
            
            // Print function offsets
            std::println("Functions:");
            for (const auto& [name, offset] : result.function_offsets) {
                std::println("  {} @ 0x{:x}", name, offset);
            }
            
            return 0;
        } else {
            // Rich error output with source context
            for (const auto& err : result.errors) {
                // Parse the error to extract location
                // Format: "file:line:col: error: message"
                std::print(std::cerr, "{}\n", err);
                
                // Try to show source line
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
                                
                                // Get column for underline
                                std::size_t third_colon = err.find(':', second_colon + 1);
                                if (third_colon != std::string::npos) {
                                    std::size_t col = std::stoull(err.substr(second_colon + 1, third_colon - second_colon - 1));
                                    std::string spaces(col > 0 ? col - 1 : 0, ' ');
                                    std::print(std::cerr, " {} \033[34m|\033[0m {}\033[91m^\033[0m\n", padding, spaces);
                                }
                            }
                        } catch (...) {
                            // Failed to parse, just skip source display
                        }
                    }
                }
                std::print(std::cerr, "\n");
            }
            return 1;
        }
    }
}

// Build a project from opus.project
int build_project(std::optional<std::string> project_path_arg) {
    std::filesystem::path project_file;
    
    if (project_path_arg) {
        project_file = *project_path_arg;
    } else {
        // Find opus.project in current directory or parents
        auto found = opus::find_project_file(std::filesystem::current_path());
        if (!found) {
            std::print(std::cerr, "\033[1;91merror\033[0m: no opus.project found in current directory or parents\n");
            return 1;
        }
        project_file = *found;
    }
    
    std::println("Loading project: {}", project_file.string());
    
    // Load and parse project file
    auto config_result = opus::load_project(project_file);
    if (!config_result) {
        std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", config_result.error());
        return 1;
    }
    
    auto config = std::move(*config_result);
    
    // Discover source files
    auto files_result = opus::discover_project_files(std::move(config));
    if (!files_result) {
        std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", files_result.error());
        return 1;
    }
    
    config = std::move(*files_result);
    
    std::println("Project: {} ({})", config.name, config.mode);
    std::println("Sources: {} files", config.source_files.size());
    for (const auto& file : config.source_files) {
        std::println("  - {}", file.filename().string());
    }
    
    // Merge sources
    auto merged_result = opus::merge_sources(config.source_files);
    if (!merged_result) {
        std::print(std::cerr, "\033[1;91merror\033[0m: {}\n", merged_result.error());
        return 1;
    }
    
    std::string source = std::move(*merged_result);
    
    // Compile
    opus::Compiler compiler;
    bool as_dll = (config.mode == "dll");
    bool as_exe = (config.mode == "exe");
    auto result = compiler.compile(source, project_file.string(), as_dll, config.healing, as_exe);
    
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
            // Generate PE with pe generator
            std::size_t main_offset = 0;
            if (result.function_offsets.contains("main")) {
                main_offset = result.function_offsets["main"];
            }
            
            std::string debug_source = config.debug ? source : "";
            
            opus::pe::DllGenerator pe_gen;
            auto pe_bytes = pe_gen.generate(result.code, main_offset, true, result.iat_fixups, debug_source, result.line_map, config.healing, as_exe, result.needs_writable_text);
            
            auto pe_errors = opus::pe::DllGenerator::validate_pe(pe_bytes);
            for (const auto& err : pe_errors) {
                std::println("\033[1;93mPE warning:\033[0m {}", err);
            }
            
            std::ofstream out(output_path, std::ios::binary);
            if (!out) {
                std::print(std::cerr, "\033[1;91merror\033[0m: cannot write to '{}' (file locked by another process?)\n", output_path.string());
                return 1;
            }
            out.write(reinterpret_cast<const char*>(pe_bytes.data()), 
                      pe_bytes.size());
            out.close();
            if (!out) {
                std::print(std::cerr, "\033[1;91merror\033[0m: write failed for '{}'\n", output_path.string());
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
            std::ofstream out(output_path, std::ios::binary);
            if (!out) {
                std::print(std::cerr, "\033[1;91merror\033[0m: cannot write to '{}' (file locked by another process?)\n", output_path.string());
                return 1;
            }
            out.write(reinterpret_cast<const char*>(result.code.data()), 
                      result.code.size());
            out.close();
            if (!out) {
                std::print(std::cerr, "\033[1;91merror\033[0m: write failed for '{}'\n", output_path.string());
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
    // Enable ANSI colors on Windows
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    if (argc < 2) {
        print_usage();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--help" || arg1 == "-h") {
        print_usage();
        return 0;
    }

    if (arg1 == "repl") {
        print_banner();
        opus::REPL repl;
        repl.run();
        return 0;
    }

    // Build command: opus build [path]
    if (arg1 == "build") {
        std::optional<std::string> project_path;
        if (argc > 2) {
            project_path = argv[2];
        }
        return build_project(project_path);
    }

    // Check for flags
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

