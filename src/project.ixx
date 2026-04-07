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
    auto result = parser.parse_project_file();
    
    if (!result) {
        std::string errors;
        for (const auto& err : result.error()) {
            errors += err.to_string() + "\n";
        }
        return std::unexpected(errors);
    }
    
    ProjectConfig config;
    std::error_code project_dir_ec;
    auto project_dir = std::filesystem::weakly_canonical(project_path.parent_path(), project_dir_ec);
    config.project_dir = project_dir_ec ? project_path.parent_path().lexically_normal() : project_dir;

    const auto& proj = *result;
    config.name = proj.name;
    config.entry = proj.entry;
    config.output = proj.output;
    config.mode = proj.mode;
    config.includes = proj.includes;
    config.debug = proj.debug;
    config.healing = proj.healing;
    
    // default: debug builds get auto healing, release gets none
    if (!config.healing.has_value()) {
        config.healing = config.debug ? ast::HealingMode::Auto : ast::HealingMode::Off;
    }

    for (auto& include_path : config.includes) {
        std::filesystem::path resolved = include_path;
        if (resolved.is_relative()) {
            resolved = config.project_dir / resolved;
        }
        include_path = resolved.lexically_normal().string();
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
