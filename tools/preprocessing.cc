#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using resource_size = std::pair<uint32_t, uint32_t>;
struct Resource {
    std::filesystem::path path;
    std::string name;
    std::vector<uint8_t> data;
    resource_size size;
};

static auto read_png_file(const std::string& path) -> std::pair<std::vector<uint8_t>, resource_size> {
    std::ifstream file(path, std::ios::binary);
    int width, height, channels;

    uint8_t* pixels = stbi_load(
        path.c_str(),
        &width,
        &height,
        &channels,
        4
    );
    if (!pixels) {
        std::println(stderr, "failed to load png {}: {}", path, stbi_failure_reason());
        return {};
    }
    std::println("    Getting pixels from {}, width: {}, height: {}", path, width, height);

    size_t pixel_count = static_cast<size_t>(width * height);
    std::vector<uint8_t> result(pixels, pixels + pixel_count * 4);

    stbi_image_free(pixels);
    return {result, {width, height}};
}

template <typename T = std::string>
static T read_file(const std::string& path, std::ios::openmode mode = {}) {
    std::ifstream file(path, std::ios::binary | mode);
    assert(file.is_open() && "Cannot open file");

    return {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
}

static auto get_resource_data(const std::filesystem::path& path) -> std::pair<std::vector<uint8_t>, resource_size> {
    const std::string extension = path.extension();
    if (extension == ".png") return read_png_file(path.string());
    return {read_file<std::vector<uint8_t>>(path.string()), {}};
}

static auto embed_identifier(const std::filesystem::path& path) -> std::string {
    std::string result = "__embedded__";
    for (char c : path.filename().string()) {
        if (std::isalnum(static_cast<unsigned char>(c))) result += c;
        else result += '_';
    }
    return result;
}

// Guards concurrent access to the shared resource list, since @embed lookups
// and inserts happen from worker threads processing different input files.
class Resource_Pool {
public:
    auto find_or_create(
        const std::filesystem::path& asset_path,
        const std::string& name
    ) -> Resource* {
        std::lock_guard lock(mutex);
        for (auto& r : resources) {
            if (r.path == asset_path) return &r;
        }
        auto [data, size] = get_resource_data(asset_path);
        resources.push_back(Resource{asset_path, name, std::move(data), size});
        return &resources.back();
    }

    auto get() const -> const std::vector<Resource>& {
        return resources;
    }

private:
    std::mutex mutex;
    std::vector<Resource> resources;
};

class Preprocessor {
public:
    Preprocessor(std::filesystem::path assets_dir, Resource_Pool& pool) : assets(assets_dir), pool(pool) {};

    auto run(std::string_view source) -> std::pair<bool, std::string> {
        clear_state(source);
        prescan_constants();
        prescan_enums();
        using enum State;
        while (pos < input.size()) {
            switch (state) {
                case Normal:       normal_state();        break;
                case String:       string_state();        break;
                case Char:         char_state();          break;
                case LineComment:  line_comment_state();  break;
                case BlockComment: block_comment_state(); break;
            }
        }
        return {has_embed, output};
    }

private:
    std::filesystem::path assets;
    Resource_Pool& pool;
    enum class State {
        Normal,
        String,
        Char,
        LineComment,
        BlockComment
    };

    State state = State::Normal;

    std::string_view input;
    size_t pos     = 0;
    size_t line_no = 1;
    std::string output;
    bool has_embed = false;
    std::map<std::string, std::vector<std::string>> enum_map;
    std::map<std::string, long long> constants;

    void clear_state(std::string_view source) {
        input     = source;
        pos       = 0;
        line_no   = 1;
        output    = "";
        has_embed = false;
        constants.clear();
    }

    auto peek(size_t offset = 0) const -> char {
        if (pos + offset >= input.size())
            return '\0';
        return input[pos + offset];
    }

    auto get() -> char {
        char c = input[pos++];
        if (c == '\n') line_no++;
        return c;
    }

    auto match(std::string_view text) -> bool {
        if (input.substr(pos, text.size()) == text) {
            pos += text.size();
            return true;
        }
        return false;
    }

    void normal_state() {
        if (match("@embed(")) {
            embed_state();
            return;
        }
        else if (match("@enum_values(")) {
            enum_values_state();
            return;
        }
        else if (match("@enum_to_string(")) {
            enum_to_string_state();
            return;
        }
        else if (match("@T(")) {
            typename_state();
            return;
        }
        else if (input.substr(pos, 4) == "@for") {
            size_t directive_pos = pos;
            pos += 4;
            while (std::isspace(static_cast<unsigned char>(peek()))) get();
            if (peek() == '(') {
                get();
                for_state(directive_pos);
                return;
            }
            pos = directive_pos;
        }
        else if (peek() == '"') {
            output += get();
            state = State::String;
            return;
        }
        else if (peek() == '\'') {
            output += get();
            state = State::Char;
            return;
        }
        else if (match("//")) {
            output += "//";
            state = State::LineComment;
            return;
        }
        else if (match("/*")) {
            output += "/*";
            state = State::BlockComment;
            return;
        }

        output += get();
    }

    void string_state() {
        char c = get();
        output += c;
        if (c == '\\') {
            output += get();
            return;
        }
        if (c == '"')
            state = State::Normal;
    }

    void char_state() {
        char c = get();
        output += c;
        if (c == '\\') {
            output += get();
            return;
        }
        if (c == '\'')
            state = State::Normal;
    }

    void line_comment_state() {
        char c = get();
        output += c;
        if (c == '\n')
            state = State::Normal;
    }


    void block_comment_state() {
        char c = get();
        output += c;
        if (c == '*' && peek() == '/') {
            output += get();
            state = State::Normal;
        }
    }

    void embed_state() {
        while (std::isspace((unsigned char)peek())) // remove spaces before string
            get();

        assert(get() == '"' && "@embed expects string");

        std::string path;
        while (peek() != '"' && peek() != '\0') // read string path
            path += get();

        assert(get() == '"' && "Unterminated @embed");

        while (std::isspace((unsigned char)peek())) // remove trailing spaces after string
            get();

        assert(get() == ')' && "Expected ')'");

        has_embed = true;
        std::filesystem::path asset_path = assets / path;
        Resource* resource = pool.find_or_create(asset_path, embed_identifier(path));

        output += resource->name; // replace @embed with resource name
    }

    void typename_state() {
        std::string expr;
        int depth = 1;
        while (peek() != '\0') {
            char c = get();
            if (c == '(') depth++;
            else if (c == ')') {
                if (--depth == 0) break;
            }
            expr += c;
        }
        assert(depth == 0 && "Unterminated @T");

        output += "typename " + expr; // replace @T(expr) with "typename expr"
    }

    static auto trim(std::string value) -> std::string {
        auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    auto constant_value(std::string expr) const -> long long {
        expr = trim(std::move(expr));
        auto it = constants.find(expr);
        if (it != constants.end()) return it->second;

        size_t end = 0;
        long long value = 0;
        try {
            value = std::stoll(expr, &end, 0);
        }
        catch (...) {
            assert(false && "@for expects an integer expression");
        }
        assert(end == expr.size() && "@for expects an integer expression");
        return value;
    }

    void prescan_constants() {
        size_t i = 0;
        while (i < input.size()) {
            size_t line_end = input.find('\n', i);
            if (line_end == std::string_view::npos) line_end = input.size();
            std::string line = trim(std::string(input.substr(i, line_end - i)));

            if (line.starts_with("#define ")) {
                std::string definition = trim(line.substr(8));
                auto split = definition.find_first_of(" \t");
                if (split != std::string::npos) {
                    auto name = definition.substr(0, split);
                    auto value = trim(definition.substr(split + 1));
                    if (!name.empty() && !value.empty()) {
                        try {
                            size_t end = 0;
                            auto number = std::stoll(value, &end, 0);
                            if (end == value.size()) constants[name] = number;
                        }
                        catch (...) {}
                    }
                }
            }

            auto constexpr_pos = line.find("constexpr ");
            if (constexpr_pos != std::string::npos) {
                auto equals = line.find('=', constexpr_pos + 10);
                if (equals != std::string::npos) {
                    auto declaration = trim(line.substr(constexpr_pos + 10, equals - constexpr_pos - 10));
                    auto space = declaration.find_last_of(" \t");
                    auto name = space == std::string::npos ? declaration : declaration.substr(space + 1);
                    auto value = trim(line.substr(equals + 1));
                    if (!value.empty() && value.back() == ';') value.pop_back();
                    if (!name.empty()) {
                        try {
                            size_t end = 0;
                            auto number = std::stoll(trim(value), &end, 0);
                            if (end == trim(value).size()) constants[name] = number;
                        }
                        catch (...) {}
                    }
                }
            }
            i = line_end == input.size() ? input.size() : line_end + 1;
        }
    }

    auto replace_for_variable(std::string_view body, std::string_view variable, long long value) -> std::string {
        std::string result;
        size_t i = 0;
        while (i < body.size()) {
            if (body[i] == '"' || body[i] == '\'') {
                char quote = body[i++];
                result += quote;
                while (i < body.size()) {
                    char c = body[i++];
                    result += c;
                    if (c == '\\' && i < body.size()) result += body[i++];
                    else if (c == quote) break;
                }
                continue;
            }
            if (i + 1 < body.size() && body[i] == '/' && body[i + 1] == '/') {
                auto end = body.find('\n', i);
                if (end == std::string_view::npos) end = body.size();
                result.append(body.substr(i, end - i));
                i = end;
                continue;
            }
            if (i + 1 < body.size() && body[i] == '/' && body[i + 1] == '*') {
                auto end = body.find("*/", i + 2);
                end = end == std::string_view::npos ? body.size() : end + 2;
                result.append(body.substr(i, end - i));
                i = end;
                continue;
            }
            if ((std::isalpha(static_cast<unsigned char>(body[i])) || body[i] == '_')) {
                size_t start = i++;
                while (i < body.size() && (std::isalnum(static_cast<unsigned char>(body[i])) || body[i] == '_')) i++;
                auto token = body.substr(start, i - start);
                if (token == variable) result += std::to_string(value);
                else result.append(token);
                continue;
            }
            result += body[i++];
        }
        return result;
    }

    void for_state(size_t directive_pos) {
        std::string header;
        int depth = 1;
        while (peek() != '\0') {
            char c = get();
            if (c == '(') depth++;
            else if (c == ')' && --depth == 0) break;
            header += c;
        }
        assert(depth == 0 && "Unterminated @for header");

        auto first = header.find(';');
        auto second = header.find(';', first + 1);
        assert(first != std::string::npos && second != std::string::npos && "@for expects init; condition; increment");

        auto init = trim(header.substr(0, first));
        auto condition = trim(header.substr(first + 1, second - first - 1));
        auto increment = trim(header.substr(second + 1));
        auto equals = init.find('=');
        assert(equals != std::string::npos && "@for init expects variable = value");
        auto variable = trim(init.substr(0, equals));
        auto space = variable.find_last_of(" \t");
        if (space != std::string::npos) variable = variable.substr(space + 1);

        std::string comparator;
        for (auto candidate : {"<=", ">=", "<", ">"}) {
            auto p = condition.find(candidate);
            if (p != std::string::npos) { comparator = candidate; condition = trim(condition.substr(p + comparator.size())); break; }
        }
        assert(!comparator.empty() && "@for condition expects <, <=, >, or >=");

        long long start = constant_value(init.substr(equals + 1));
        long long limit = constant_value(condition);
        long long step = increment.find("++") != std::string::npos ? 1 : increment.find("--") != std::string::npos ? -1 : 0;
        assert(step != 0 && "@for increment expects ++variable or --variable");

        while (std::isspace(static_cast<unsigned char>(peek()))) get();
        assert(get() == '{' && "@for expects a braced body");
        size_t body_line = line_no;
        size_t body_start = pos;
        int braces = 1;
        while (pos < input.size() && braces > 0) {
            char c = get();
            if (c == '"' || c == '\'') {
                char quote = c;
                while (pos < input.size()) { c = get(); if (c == '\\') get(); else if (c == quote) break; }
            }
            else if (c == '{') braces++;
            else if (c == '}') braces--;
        }
        assert(braces == 0 && "Unterminated @for body");
        size_t after_line = line_no;
        auto body = input.substr(body_start, pos - body_start - 1);
        size_t line_start = input.rfind('\n', directive_pos);
        line_start = line_start == std::string_view::npos ? 0 : line_start + 1;
        size_t indent_end = line_start;
        while (indent_end < directive_pos && (input[indent_end] == ' ' || input[indent_end] == '\t')) indent_end++;
        std::string indent(input.substr(line_start, indent_end - line_start));

        auto condition_holds = [&](long long value) {
            if (comparator == "<") return value < limit;
            if (comparator == "<=") return value <= limit;
            if (comparator == ">") return value > limit;
            return value >= limit;
        };
        assert(((step > 0 && (comparator == "<" || comparator == "<=")) ||
                (step < 0 && (comparator == ">" || comparator == ">="))) &&
               "@for increment moves away from loop condition");
        for (long long value = start; condition_holds(value); value += step) {
            output += "#line " + std::to_string(body_line) + "\n";
            output += indent + "{";
            auto expanded_body = replace_for_variable(body, variable, value);
            output += expanded_body;
            if (!expanded_body.empty() && expanded_body.back() == '\n') output += indent;
            output += "}\n";
        }
        output += "#line " + std::to_string(after_line) + "\n";
    }

    void prescan_enums() {
        auto is_ws = [](char c) -> bool { return std::isspace(static_cast<unsigned char>(c)); };
        auto is_id = [](char c) -> bool {
            return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
        };

        size_t i = 0;
        while (i < input.size()) {
            if (input[i] != 'e') { i++; continue; }
            if (i + 4 > input.size() || input.substr(i, 4) != "enum") { i++; continue; }

            i += 4;

            while (i < input.size() && is_ws(input[i])) i++;
            if (i + 5 < input.size() && input.substr(i, 6) == "struct") i += 6;
            else if (i + 4 < input.size() && input.substr(i, 5) == "class") i += 5;
            else { continue; }

            while (i < input.size() && is_ws(input[i])) i++;
            std::string name;
            while (i < input.size() && is_id(input[i])) name += input[i++];
            if (name.empty()) continue;

            while (i < input.size() && is_ws(input[i])) i++;
            if (i < input.size() && input[i] == ':') {
                i++;
                while (i < input.size() && is_ws(input[i])) i++;
                while (i < input.size() && !is_ws(input[i]) && input[i] != '{') i++;
            }

            while (i < input.size() && is_ws(input[i])) i++;
            if (i >= input.size() || input[i] != '{') continue;
            i++;

            std::vector<std::string> values;
            int brace_depth = 1;
            while (i < input.size() && brace_depth > 0) {
                while (i < input.size() && is_ws(input[i])) i++;
                if (i >= input.size()) break;

                char c = input[i];
                if (c == '}') { brace_depth--; i++; continue; }
                if (c == '{') { brace_depth++; i++; continue; }
                if (c == ',') { i++; continue; }

                if (c == '/' && i + 1 < input.size()) {
                    if (input[i+1] == '/') { while (i < input.size() && input[i] != '\n') i++; continue; }
                    if (input[i+1] == '*') {
                        i += 2;
                        while (i + 1 < input.size() && !(input[i] == '*' && input[i+1] == '/')) i++;
                        if (i < input.size()) i += 2;
                        continue;
                    }
                }

                if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                    std::string val;
                    while (i < input.size() && is_id(input[i])) val += input[i++];
                    if (!val.empty()) values.push_back(val);

                    while (i < input.size() && is_ws(input[i])) i++;
                    if (i < input.size() && input[i] == '=') {
                        i++;
                        int depth = 0;
                        while (i < input.size()) {
                            c = input[i];
                            if (c == ',' && depth == 0) break;
                            if (c == '}') {
                                if (depth == 0) break;
                                depth--;
                            }
                            if (c == '{' || c == '(' || c == '<' || c == '[') depth++;
                            if (c == '}' || c == ')' || c == '>' || c == ']') {
                                if (depth > 0) depth--;
                            }
                            if (c == '/' && i + 1 < input.size()) {
                                if (input[i+1] == '/') { while (i < input.size() && input[i] != '\n') i++; continue; }
                                if (input[i+1] == '*') {
                                    i += 2;
                                    while (i + 1 < input.size() && !(input[i] == '*' && input[i+1] == '/')) i++;
                                    if (i < input.size()) i += 2;
                                    continue;
                                }
                            }
                            i++;
                        }
                    }
                    continue;
                }

                i++;
            }

            if (!name.empty() && !values.empty()) {
                assert(enum_map.find(name) == enum_map.end() && "Duplicate enum name");
                enum_map[name] = values;
            }
        }
    }

    void enum_values_state() {
        std::string name;
        while (peek() != ')' && peek() != '\0') name += get();
        assert(get() == ')' && "Unterminated @enum_values");

        auto it = enum_map.find(name);
        assert(it != enum_map.end() && "enum_values: enum not found");

        output += "{ ";
        for (size_t j = 0; j < it->second.size(); ++j) {
            if (j > 0) output += ", ";
            output += it->first + "::" + it->second[j];
        }
        output += " }";
    }

    void enum_to_string_state() {
        std::string name;
        while (peek() != ')' && peek() != '\0') name += get();
        assert(get() == ')' && "Unterminated @enum_to_string");

        auto it = enum_map.find(name);
        assert(it != enum_map.end() && "enum_to_string: enum not found");

        size_t orig_line = line_no;

        output += "constexpr auto enum_to_string(" + it->first + " value) -> string {\n";
        output += "    switch (value) {\n";
        for (size_t j = 0; j < it->second.size(); ++j) {
            auto qualified = it->first + "::" + it->second[j];
            output += "    case " + qualified + ": return \"" + qualified + "\";\n";
        }
        output += "    default: return string();\n";
        output += "    }\n";
        output += "}\n";
        output += "#line " + std::to_string(orig_line) + "\n";
    }
};

static void write_resources_header(
    const std::string& filename,
    const std::vector<Resource>& resources
) {
    std::ofstream out(filename);
    out << "#pragma once\n\n";
    out << "#include \"kstd/resource.hh\"\n\n";

    for (const auto& r : resources) {
        std::println("    Writing {} as {} in resources.hh", r.path.string(), r.name);
        auto& [width, height] = r.size;
        out << "constexpr Resource<" << r.data.size() << "> " <<  r.name << " = {\n";
        out << "    .data = Static_Array<u8, " << r.data.size() << ">" << "{\n";
        for (size_t i = 0; i < r.data.size(); i++) {
            if (i % 12 == 0)
                out << "        ";
            out << "0x"
                << std::hex
                << std::setw(2)
                << std::setfill('0')
                << static_cast<uint32_t>(r.data[i])
                << std::dec
                << ",";
            if (i % 12 != 11)
                out << " ";
            if (i % 12 == 11)
                out << "\n";
        }
        out << "    },\n";
        out << "    .width = " << width << ",\n";
        out << "    .height = " << height << ",\n";
        out << "};\n\n";
    }
}

// #line makes diagnostics, __FILE__ and debugger source paths point at
// the real file under src/ instead of its throwaway copy under build/.
static auto line_directive(int line, const std::filesystem::path& original) -> std::string {
    return "#line " + std::to_string(line) + " \"" + original.string() + "\"\n";
}

// Inserting the resources.hh include shifts every following line by
// one, so line_directive() re-syncs numbering back to the original file.
static auto add_resource_include(std::string source, const std::filesystem::path& original) -> std::string {
    constexpr std::string_view pragma = "#pragma once\n";
    const std::string include = "#include \"resources.hh\"\n";

    if (auto pos = source.find(pragma); pos != std::string::npos) {
        source.insert(pos + pragma.size(), include + line_directive(2, original));
    }
    else {
        source.insert(0, include + line_directive(1, original));
    }

    return source;
}

static void process_file(
    const std::filesystem::path& assets_dir,
    const std::filesystem::path& input_dir,
    const std::filesystem::path& output_dir,
    Resource_Pool& pool,
    const std::filesystem::path& input
) {
    Preprocessor pp(assets_dir, pool);
    std::filesystem::path output = output_dir / std::filesystem::relative(input, input_dir);

    std::string result;
    bool has_embed = false;
    bool is_source = input.extension() == ".hh" || input.extension() == ".cc";
    bool is_asm    = input.extension() == ".S";
    std::filesystem::path original = std::filesystem::absolute(input);

    if (is_source) {
        auto source = read_file(input);
        std::tie(has_embed, result) = pp.run(source);
        result = has_embed ? add_resource_include(result, original)
                            : line_directive(1, original) + result;
    }
    else if (is_asm) {
        // .S is run through cpp, so #line applies here too.
        result = line_directive(1, original) + read_file(input);
    }
    else {
        result = read_file(input);
    }

    static std::mutex print_mutex;
    std::ofstream out(output);
    {
        std::lock_guard lock(print_mutex);
        std::println("Processing file: {}", input.string());
        std::println("    Output set to: {}", output.string());
        if (is_source) {
            if (has_embed)
                std::println("    {} has @embed, adding #include \"resources.hh\"", input.string());
        }
        else {
            std::println("    Omitting non .hh/.cc file: {}", input.string());
        }
        if (!out) std::println(stderr, "    failed to write {}", output.string());
        std::println("    Writing to {}", output.string());
    }
    out << result;
}

auto main(int argc, char** argv) -> int {
    if (argc < 4) {
        std::println(stderr, "usage: preprocessing assets_directory input_directory output_directory");
        return 1;
    }

    std::filesystem::path assets_dir = argv[1];
    std::filesystem::path input_dir = argv[2];
    std::filesystem::path output_dir = argv[3];
    std::filesystem::create_directories(output_dir);
    std::println("Assets dir: {}", assets_dir.string());
    std::println("Input dir: {}", input_dir.string());
    std::println("Output dir: {}", output_dir.string());
    assert(std::filesystem::exists(assets_dir) && "Assets dir does not exist");
    assert(std::filesystem::exists(input_dir) && "Input dir does not exist");
    assert(std::filesystem::exists(output_dir) && "Output dir does not exist");

    std::vector<std::filesystem::path> input_files;
    for (auto const& file: std::filesystem::recursive_directory_iterator(input_dir)) {
        if (std::filesystem::is_directory(file.path())) continue;
        input_files.push_back(file.path());
    }

    // Create output directories up front; create_directories races if called
    // concurrently for shared parent directories.
    for (auto const& input : input_files) {
        std::filesystem::path output = output_dir / std::filesystem::relative(input, input_dir);
        std::filesystem::create_directories(output.parent_path());
    }

    Resource_Pool pool;

    auto thread_count = std::max<uint32_t>(1, std::thread::hardware_concurrency());
    thread_count = static_cast<uint32_t>(std::min<size_t>(thread_count, std::max<size_t>(1, input_files.size())));

    {
        std::vector<std::jthread> workers;
        size_t chunk = (input_files.size() + thread_count - 1) / thread_count;
        for (uint32_t t = 0; t < thread_count; ++t) {
            size_t begin = t * chunk;
            size_t end   = std::min(input_files.size(), begin + chunk);
            if (begin >= end) continue;
            workers.emplace_back([&, begin, end] {
                for (size_t i = begin; i < end; ++i)
                    process_file(assets_dir, input_dir, output_dir, pool, input_files[i]);
            });
        }
    }

    std::println("Writing {}", (output_dir / "resources.hh").string());
    write_resources_header(output_dir / "resources.hh", pool.get());
}
