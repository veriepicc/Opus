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
    std::optional<ast::HealingMode> healing;
    
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
    
    // default: debug builds get auto healing, release gets none
    if (!config.healing.has_value()) {
        config.healing = config.debug ? ast::HealingMode::Auto : ast::HealingMode::Off;
    }
    
    return config;
}

std::vector<std::filesystem::path> discover_files(const std::filesystem::path& dir) {
    std::vector<std::filesystem::path> files;
    
    if (!std::filesystem::exists(dir)) {
        return files;
    }
    
    // use error code overload so permission errors dont blow up
    std::error_code ec;
    for (auto it = std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec);
         it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            std::println("warning: filesystem error during discovery: {}", ec.message());
            ec.clear();
            continue;
        }
        if (it->is_regular_file() && it->path().extension() == ".op") {
            files.push_back(it->path());
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
    
    // set-based dedup instead of linear scan
    std::set<std::filesystem::path> seen(config.source_files.begin(), config.source_files.end());
    
    for (const auto& include : config.includes) {
        std::filesystem::path include_path = config.project_dir / include;
        auto files = discover_files(include_path);
        for (auto& file : files) {
            if (seen.insert(file).second) {
                config.source_files.push_back(std::move(file));
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
    
    while (true) {
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
