// opus project system

export module opus.project;

import opus.lexer;
import opus.parser;
import opus.ast;
import std;

export namespace opus {

struct ProjectConfig {
    std::string name;
    std::string entry;
    std::string output;
    std::string mode = "dll";
    std::vector<std::string> includes;
    bool debug = false;
    std::string healing;  // "auto", "freeze", "off" - empty means resolve from debug flag
    
    std::filesystem::path project_dir;
    std::vector<std::filesystem::path> source_files;
};

std::expected<ProjectConfig, std::string> load_project(const std::filesystem::path& project_path) {
    std::ifstream file(project_path);
    if (!file) {
        return std::unexpected(std::format("cannot open project file: {}", project_path.string()));
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    
    Lexer lexer(source, project_path.string());
    auto tokens = lexer.tokenize_all();
    
    Parser parser(std::move(tokens), SyntaxMode::CStyle);
    auto result = parser.parse_module(project_path.stem().string());
    
    if (!result) {
        std::string errors;
        for (const auto& err : result.error()) {
            errors += err.to_string() + "\n";
        }
        std::println("parse errors:\n{}", errors);
        return std::unexpected(errors);
    }
    
    ProjectConfig config;
    config.project_dir = project_path.parent_path();
    
    bool found = false;
    for (const auto& decl : result->decls) {
        if (decl->is<ast::ProjectDecl>()) {
            const auto& proj = decl->as<ast::ProjectDecl>();
            config.name = proj.name;
            config.entry = proj.entry;
            config.output = proj.output;
            config.mode = proj.mode;
            config.includes = proj.includes;
            config.debug = proj.debug;
            config.healing = proj.healing;
            found = true;
            break;
        }
    }
    
    if (!found) {
        return std::unexpected("no project declaration found in opus.project");
    }
    
    // only validate if user explicitly set it
    if (!config.healing.empty() && config.healing != "auto" && config.healing != "freeze" && config.healing != "off") {
        return std::unexpected(std::format("invalid healing mode: '{}', expected 'auto', 'freeze', or 'off'", config.healing));
    }
    
    // default: debug builds get auto healing, release gets none
    if (config.healing.empty()) {
        config.healing = config.debug ? "auto" : "off";
    }
    
    return config;
}

std::vector<std::filesystem::path> discover_files(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;
    
    if (!std::filesystem::exists(dir)) {
        return files;
    }
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".op") {
            files.push_back(entry.path());
        }
    }
    
    std::sort(files.begin(), files.end());
    return files;
}

std::expected<ProjectConfig, std::string> discover_project_files(ProjectConfig config) {
    if (!config.entry.empty()) {
        std::filesystem::path entry_path = config.project_dir / config.entry;
        if (std::filesystem::exists(entry_path)) {
            config.source_files.push_back(entry_path);
        } else {
            return std::unexpected(std::format("entry file not found: {}", entry_path.string()));
        }
    }
    
    for (const auto& include : config.includes) {
        std::filesystem::path include_path = config.project_dir / include;
        auto files = discover_files(include_path);
        for (const auto& file : files) {
            if (std::find(config.source_files.begin(), config.source_files.end(), file) == config.source_files.end()) {
                config.source_files.push_back(file);
            }
        }
    }
    
    if (config.source_files.empty()) {
        return std::unexpected("no source files found for project");
    }
    
    return config;
}

std::expected<std::string, std::string> merge_sources(const std::vector<std::filesystem::path>& files) {
    std::string merged;
    
    for (const auto& file : files) {
        std::ifstream in(file);
        if (!in) {
            return std::unexpected(std::format("cannot read file: {}", file.string()));
        }
        
        merged += std::format("\n// === {} ===\n", file.filename().string());
        
        std::stringstream buffer;
        buffer << in.rdbuf();
        merged += buffer.str();
        merged += "\n";
    }
    
    return merged;
}

std::optional<std::filesystem::path> find_project_file(const std::filesystem::path& start_dir) {
    std::filesystem::path current = start_dir;
    
    while (!current.empty() && current.has_parent_path()) {
        std::filesystem::path project_file = current / "opus.project";
        if (std::filesystem::exists(project_file)) {
            return project_file;
        }
        
        auto parent = current.parent_path();
        if (parent == current) break;
        current = parent;
    }
    
    return std::nullopt;
}

} // namespace opus
