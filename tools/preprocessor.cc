#include <algorithm>
#include <cassert>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <mutex>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "kstd/basic.hh"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

enum struct Token_Kind : u8 {
    END,
    IDENTIFIER,
    NUMBER,
    LITERAL,
    COMMENT,
    PUNCTUATION,
};

struct Token {
    Token_Kind kind;
    usize      begin;
    usize      end;
};

struct Lexer {
    std::string_view input;
    usize            position;

    explicit Lexer(std::string_view input, usize position = 0)
        : input(input),
          position(position) {}

    auto next_token() -> Token {
        while (position < input.size() && std::isspace(static_cast<unsigned char>(input[position]))) position++;

        if (position >= input.size()) return {Token_Kind::END, input.size(), input.size()};

        usize begin = position;
        if (input.substr(position, 2) == "//") {
            position += 2;
            while (position < input.size() && input[position] != '\n') position++;
            return {Token_Kind::COMMENT, begin, position};
        }
        if (input.substr(position, 2) == "/*") {
            position += 2;
            while (position + 1 < input.size() && input.substr(position, 2) != "*/") position++;
            if (position + 1 < input.size())
                position += 2;
            else
                position = input.size();
            return {Token_Kind::COMMENT, begin, position};
        }

        usize quote = prefixed_quote_position();
        if (quote != std::string_view::npos) {
            if (quote > begin && (input[quote - 1] == 'R' || (quote >= 2 && input[quote - 2] == 'R')))
                return {Token_Kind::LITERAL, begin, scan_raw_literal(quote)};
            return {Token_Kind::LITERAL, begin, scan_quoted(quote)};
        }
        if (input[position] == '"' || input[position] == '\'')
            return {Token_Kind::LITERAL, begin, scan_quoted(position)};

        if (is_identifier_start(input[position])) {
            position++;
            while (position < input.size() && is_identifier_part(input[position])) position++;
            return {Token_Kind::IDENTIFIER, begin, position};
        }
        if (std::isdigit(static_cast<unsigned char>(input[position]))) {
            position++;
            while (position < input.size() && (std::isalnum(static_cast<unsigned char>(input[position])) ||
                                                input[position] == '.' || input[position] == '\''))
                position++;
            return {Token_Kind::NUMBER, begin, position};
        }

        for (std::string_view punctuation :
             {"->*", "<<=", ">>=", "...", "::", "->", "<=", ">=", "==", "!=", "++", "--",
              "&&",  "||",  "+=",  "-=",  "*=", "/=", "%=", "<<", ">>", "&=", "|=", "^="}) {
            if (input.substr(position, punctuation.size()) == punctuation) {
                position += punctuation.size();
                return {Token_Kind::PUNCTUATION, begin, position};
            }
        }

        position++;
        return {Token_Kind::PUNCTUATION, begin, position};
    }

    auto text(Token token) const -> std::string_view {
        return input.substr(token.begin, token.end - token.begin);
    }

    static auto is_identifier_start(char c) -> bool {
        return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
    }

    static auto is_identifier_part(char c) -> bool {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    }

    auto prefixed_quote_position() const -> usize {
        if (input[position] == '"') return position;
        for (std::string_view prefix : {"u8R", "uR", "UR", "LR", "u8", "u", "U", "L", "R"}) {
            if (input.substr(position, prefix.size()) == prefix && position + prefix.size() < input.size() &&
                input[position + prefix.size()] == '"')
                return position + prefix.size();
        }
        return std::string_view::npos;
    }

    auto scan_quoted(usize quote) -> usize {
        position = quote + 1;
        while (position < input.size()) {
            char c = input[position++];
            if (c == '\\' && position < input.size())
                position++;
            else if (c == input[quote])
                break;
        }
        return position;
    }

    auto scan_raw_literal(usize quote) -> usize {
        auto open = input.find('(', quote + 1);
        if (open == std::string_view::npos) return scan_quoted(quote);
        auto delimiter = input.substr(quote + 1, open - quote - 1);
        position = open + 1;
        while (position < input.size()) {
            if (input[position] == ')' && input.substr(position + 1, delimiter.size()) == delimiter &&
                position + delimiter.size() + 1 < input.size() && input[position + delimiter.size() + 1] == '"') {
                position += delimiter.size() + 2;
                return position;
            }
            position++;
        }
        return position;
    }
};

struct Balanced_Range {
    bool complete;
    usize close;
};

static auto scan_balanced(std::string_view source, usize body_start, char open, char close) -> Balanced_Range {
    Lexer lexer(source, body_start);
    s32 depth = 1;
    while (true) {
        auto token = lexer.next_token();
        if (token.kind == Token_Kind::END) return {false, source.size()};
        auto text = lexer.text(token);
        if (token.end == token.begin + 1 && text[0] == open)
            depth++;
        else if (token.end == token.begin + 1 && text[0] == close && --depth == 0)
            return {true, token.begin};
    }
}

using Resource_Size = std::pair<u32, u32>;

struct Resource {
    std::filesystem::path path;
    std::string           name;
    std::vector<u8>       data;
    Resource_Size         size;
};

static auto read_png_file(const std::string& path) -> std::pair<std::vector<u8>, Resource_Size> {
    std::ifstream file(path, std::ios::binary);
    s32 width, height, channels;

    u8* pixels = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!pixels) {
        std::println(stderr, "failed to load png {}: {}", path, stbi_failure_reason());
        return {};
    }
    std::println("    Getting pixels from {}, width: {}, height: {}", path, width, height);

    usize pixel_count = static_cast<usize>(width * height);
    std::vector<u8> result(pixels, pixels + pixel_count * 4);

    stbi_image_free(pixels);
    return {result, {width, height}};
}

template <typename T = std::string>
static T read_file(const std::string& path, std::ios::openmode mode = {}) {
    std::ifstream file(path, std::ios::binary | mode);
    assert(file.is_open() && "Cannot open file");

    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

static auto get_resource_data(const std::filesystem::path& path) -> std::pair<std::vector<u8>, Resource_Size> {
    const std::string extension = path.extension();
    if (extension == ".png") return read_png_file(path.string());
    return {read_file<std::vector<u8>>(path.string()), {}};
}

static auto embed_identifier(const std::filesystem::path& path) -> std::string {
    std::string result = "__embedded__";
    for (char c : path.filename().string()) {
        if (std::isalnum(static_cast<unsigned char>(c)))
            result += c;
        else
            result += '_';
    }
    return result;
}

// Guards concurrent access to the shared resource list, since @embed lookups
// and inserts happen from worker threads processing different input files.
struct Resource_Pool {
    std::mutex            mutex;
    std::vector<Resource> resources;

    auto find_or_create(const std::filesystem::path& asset_path, const std::string& name) -> Resource* {
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

};

struct Preprocessor {
    std::filesystem::path assets;
    Resource_Pool&        pool;
    enum struct State : u8 {
        NORMAL,
        STRING,
        CHAR,
        LINE_COMMENT,
        BLOCK_COMMENT,
    };

    State state = State::NORMAL;

    std::string_view input;
    usize            pos       = 0;
    usize            line_no   = 1;
    std::string      output;
    bool             has_embed = false;
    std::map<std::string, std::vector<std::string>> enum_map;
    std::map<std::string, s64>                      constants;

    Preprocessor(std::filesystem::path assets_dir, Resource_Pool& pool)
        : assets(assets_dir),
          pool(pool) {}

    auto run(std::string_view source) -> std::pair<bool, std::string> {
        clear_state(source);
        prescan_constants();
        prescan_enums();
        using enum State;
        while (pos < input.size()) {
            switch (state) {
                case NORMAL:        normal_state();       break;
                case STRING:        string_state();       break;
                case CHAR:          char_state();         break;
                case LINE_COMMENT:  line_comment_state(); break;
                case BLOCK_COMMENT: block_comment_state(); break;
            }
        }
        return {has_embed, output};
    }

    auto clear_state(std::string_view source) -> void {
        input = source;
        pos = 0;
        line_no = 1;
        state = State::NORMAL;
        output = "";
        has_embed = false;
        enum_map.clear();
        constants.clear();
    }

    auto peek(usize offset = 0) const -> char {
        if (pos + offset >= input.size()) return '\0';
        return input[pos + offset];
    }

    auto get() -> char {
        char c = input[pos++];
        if (c == '\n') line_no++;
        return c;
    }

    auto consume_to(usize target) -> void {
        while (pos < target) get();
    }

    auto match(std::string_view text) -> bool {
        if (input.substr(pos, text.size()) == text) {
            pos += text.size();
            return true;
        }
        return false;
    }

    auto normal_state() -> void {
        if (match("@embed(")) {
            embed_state();
            return;
        } else if (match("@enum_values(")) {
            enum_values_state();
            return;
        } else if (match("@enum_to_string(")) {
            enum_to_string_state();
            return;
        } else if (match("@T(")) {
            typename_state();
            return;
        } else if (input.substr(pos, 4) == "@for") {
            usize directive_pos = pos;
            pos += 4;
            while (std::isspace(static_cast<unsigned char>(peek()))) get();
            if (peek() == '(') {
                get();
                for_state(directive_pos);
                return;
            }
            pos = directive_pos;
        } else if (peek() == '"') {
            output += get();
            state = State::STRING;
            return;
        } else if (peek() == '\'') {
            output += get();
            state = State::CHAR;
            return;
        } else if (match("//")) {
            output += "//";
            state = State::LINE_COMMENT;
            return;
        } else if (match("/*")) {
            output += "/*";
            state = State::BLOCK_COMMENT;
            return;
        }

        output += get();
    }

    auto string_state() -> void {
        char c = get();
        output += c;
        if (c == '\\') {
            output += get();
            return;
        }
        if (c == '"') state = State::NORMAL;
    }

    auto char_state() -> void {
        char c = get();
        output += c;
        if (c == '\\') {
            output += get();
            return;
        }
        if (c == '\'') state = State::NORMAL;
    }

    auto line_comment_state() -> void {
        char c = get();
        output += c;
        if (c == '\n') state = State::NORMAL;
    }

    auto block_comment_state() -> void {
        char c = get();
        output += c;
        if (c == '*' && peek() == '/') {
            output += get();
            state = State::NORMAL;
        }
    }

    auto embed_state() -> void {
        while (std::isspace((unsigned char)peek()))  // remove spaces before string
            get();

        assert(get() == '"' && "@embed expects string");

        std::string path;
        while (peek() != '"' && peek() != '\0')  // read string path
            path += get();

        assert(get() == '"' && "Unterminated @embed");

        while (std::isspace((unsigned char)peek()))  // remove trailing spaces after string
            get();

        assert(get() == ')' && "Expected ')'");

        has_embed = true;
        std::filesystem::path asset_path = assets / path;
        Resource* resource = pool.find_or_create(asset_path, embed_identifier(path));

        output += resource->name;  // replace @embed with resource name
    }

    auto typename_state() -> void {
        auto range = scan_balanced(input, pos, '(', ')');
        assert(range.complete && "Unterminated @T");
        auto expr = std::string(input.substr(pos, range.close - pos));
        consume_to(range.close + 1);

        output += "typename " + expr;  // replace @T(expr) with "typename expr"
    }

    static auto trim(std::string value) -> std::string {
        auto first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        auto last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    auto constant_value(std::string expr) const -> s64 {
        expr = trim(std::move(expr));
        auto it = constants.find(expr);
        if (it != constants.end()) return it->second;

        usize end = 0;
        s64 value = 0;
        try {
            value = std::stoll(expr, &end, 0);
        } catch (...) {
            assert(false && "@for expects an integer expression");
        }
        assert(end == expr.size() && "@for expects an integer expression");
        return value;
    }

    auto prescan_constants() -> void {
        usize i = 0;
        while (i < input.size()) {
            usize line_end = input.find('\n', i);
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
                            usize end = 0;
                            auto number = std::stoll(value, &end, 0);
                            if (end == value.size()) constants[name] = number;
                        } catch (...) {
                        }
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
                            usize end = 0;
                            auto number = std::stoll(trim(value), &end, 0);
                            if (end == trim(value).size()) constants[name] = number;
                        } catch (...) {
                        }
                    }
                }
            }
            i = line_end == input.size() ? input.size() : line_end + 1;
        }
    }

    auto replace_for_variable(std::string_view body, std::string_view variable, s64 value) -> std::string {
        std::string result;
        Lexer lexer(body);
        usize copied = 0;
        while (true) {
            auto token = lexer.next_token();
            if (token.kind == Token_Kind::END) break;

            result.append(body.substr(copied, token.begin - copied));
            if (token.kind == Token_Kind::IDENTIFIER && lexer.text(token) == variable)
                result += std::to_string(value);
            else
                result.append(body.substr(token.begin, token.end - token.begin));
            copied = token.end;
        }
        result.append(body.substr(copied));
        return result;
    }

    auto for_state(usize directive_pos) -> void {
        auto header_range = scan_balanced(input, pos, '(', ')');
        assert(header_range.complete && "Unterminated @for header");
        auto header = std::string(input.substr(pos, header_range.close - pos));
        consume_to(header_range.close + 1);

        usize first = std::string::npos;
        usize second = std::string::npos;
        Lexer header_lexer(header);
        while (true) {
            auto token = header_lexer.next_token();
            if (token.kind == Token_Kind::END) break;
            if (header_lexer.text(token) != ";") continue;
            if (first == std::string::npos)
                first = token.begin;
            else {
                second = token.begin;
                break;
            }
        }
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
            if (p != std::string::npos) {
                comparator = candidate;
                condition = trim(condition.substr(p + comparator.size()));
                break;
            }
        }
        assert(!comparator.empty() && "@for condition expects <, <=, >, or >=");

        s64 start = constant_value(init.substr(equals + 1));
        s64 limit = constant_value(condition);
        s64 step = increment.find("++") != std::string::npos ? 1 : increment.find("--") != std::string::npos ? -1 : 0;
        assert(step != 0 && "@for increment expects ++variable or --variable");

        while (std::isspace(static_cast<unsigned char>(peek()))) get();
        assert(get() == '{' && "@for expects a braced body");
        usize body_line = line_no;
        usize body_start = pos;
        auto body_range = scan_balanced(input, body_start, '{', '}');
        assert(body_range.complete && "Unterminated @for body");
        auto body = input.substr(body_start, body_range.close - body_start);
        consume_to(body_range.close + 1);
        usize after_line = line_no;
        usize line_start = input.rfind('\n', directive_pos);
        line_start = line_start == std::string_view::npos ? 0 : line_start + 1;
        usize indent_end = line_start;
        while (indent_end < directive_pos && (input[indent_end] == ' ' || input[indent_end] == '\t')) indent_end++;
        std::string indent(input.substr(line_start, indent_end - line_start));

        auto condition_holds = [&](s64 value) {
            if (comparator == "<") return value < limit;
            if (comparator == "<=") return value <= limit;
            if (comparator == ">") return value > limit;
            return value >= limit;
        };
        assert(
            ((step > 0 && (comparator == "<" || comparator == "<=")) ||
             (step < 0 && (comparator == ">" || comparator == ">="))) &&
            "@for increment moves away from loop condition");
        for (s64 value = start; condition_holds(value); value += step) {
            output += "#line " + std::to_string(body_line) + "\n";
            output += indent + "{";
            auto expanded_body = replace_for_variable(body, variable, value);
            output += expanded_body;
            if (!expanded_body.empty() && expanded_body.back() == '\n') output += indent;
            output += "}\n";
        }
        output += "#line " + std::to_string(after_line) + "\n";
    }

    auto prescan_enums() -> void {
        Lexer lexer(input);
        while (true) {
            auto enum_token = lexer.next_token();
            if (enum_token.kind == Token_Kind::END) break;
            if (enum_token.kind != Token_Kind::IDENTIFIER || lexer.text(enum_token) != "enum") continue;

            auto kind = lexer.next_token();
            if (kind.kind != Token_Kind::IDENTIFIER || (lexer.text(kind) != "struct" && lexer.text(kind) != "class"))
                continue;

            auto name_token = lexer.next_token();
            if (name_token.kind != Token_Kind::IDENTIFIER) continue;
            auto name = std::string(lexer.text(name_token));

            auto token = lexer.next_token();
            if (lexer.text(token) == ":") {
                do token = lexer.next_token();
                while (token.kind != Token_Kind::END && lexer.text(token) != "{");
            }
            if (token.kind == Token_Kind::END || lexer.text(token) != "{") continue;

            std::vector<std::string> values;
            token = lexer.next_token();
            while (token.kind != Token_Kind::END && lexer.text(token) != "}") {
                if (lexer.text(token) == ",") {
                    token = lexer.next_token();
                    continue;
                }
                if (token.kind != Token_Kind::IDENTIFIER) {
                    token = lexer.next_token();
                    continue;
                }

                values.emplace_back(lexer.text(token));
                token = lexer.next_token();
                if (lexer.text(token) != "=") continue;

                s32 parentheses = 0;
                s32 brackets = 0;
                s32 braces = 0;
                s32 angles = 0;
                token = lexer.next_token();
                while (token.kind != Token_Kind::END) {
                    auto text = lexer.text(token);
                    if (text == "(")
                        parentheses++;
                    else if (text == ")" && parentheses > 0)
                        parentheses--;
                    else if (text == "[")
                        brackets++;
                    else if (text == "]" && brackets > 0)
                        brackets--;
                    else if (text == "{")
                        braces++;
                    else if (text == "}" && braces > 0)
                        braces--;
                    else if (text == "<")
                        angles++;
                    else if (text == ">" && angles > 0)
                        angles--;
                    else if (text == ">>" && angles > 0)
                        angles = std::max(0, angles - 2);
                    else if (
                        (text == "," || text == "}") && parentheses == 0 && brackets == 0 && braces == 0 && angles == 0)
                        break;
                    token = lexer.next_token();
                }
            }

            if (!values.empty()) {
                assert(enum_map.find(name) == enum_map.end() && "Duplicate enum name");
                enum_map[name] = values;
            }
        }
    }

    auto enum_values_state() -> void {
        std::string name;
        while (peek() != ')' && peek() != '\0') name += get();
        assert(get() == ')' && "Unterminated @enum_values");

        auto it = enum_map.find(name);
        assert(it != enum_map.end() && "enum_values: enum not found");

        output += "{ ";
        for (usize j = 0; j < it->second.size(); ++j) {
            if (j > 0) output += ", ";
            output += it->first + "::" + it->second[j];
        }
        output += " }";
    }

    auto enum_to_string_state() -> void {
        std::string name;
        while (peek() != ')' && peek() != '\0') name += get();
        assert(get() == ')' && "Unterminated @enum_to_string");

        auto it = enum_map.find(name);
        assert(it != enum_map.end() && "enum_to_string: enum not found");

        usize orig_line = line_no;

        output += "constexpr auto enum_to_string(" + it->first + " value) -> string {\n";
        output += "    switch (value) {\n";
        for (usize j = 0; j < it->second.size(); ++j) {
            auto qualified = it->first + "::" + it->second[j];
            output += "    case " + qualified + ": return \"" + qualified + "\";\n";
        }
        output += "    default: return string();\n";
        output += "    }\n";
        output += "}\n";
        output += "#line " + std::to_string(orig_line) + "\n";
    }
};

static auto write_resources_header(const std::string& filename, const std::vector<Resource>& resources) -> void {
    std::ofstream out(filename);
    out << "#pragma once\n\n";
    out << "#include \"kstd/resource.hh\"\n\n";

    for (const auto& r : resources) {
        std::println("    Writing {} as {} in resources.hh", r.path.string(), r.name);
        auto& [width, height] = r.size;
        out << "constexpr Resource<" << r.data.size() << "> " << r.name << " = {\n";
        out << "    .data = Static_Array<u8, " << r.data.size() << ">" << "{\n";
        for (usize i = 0; i < r.data.size(); i++) {
            if (i % 12 == 0) out << "        ";
            out << "0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<u32>(r.data[i]) << std::dec
                << ",";
            if (i % 12 != 11) out << " ";
            if (i % 12 == 11) out << "\n";
        }
        out << "    },\n";
        out << "    .width = " << width << ",\n";
        out << "    .height = " << height << ",\n";
        out << "};\n\n";
    }
}

// #line makes diagnostics, __FILE__ and debugger source paths point at
// the real file under src/ instead of its throwaway copy under build/.
static auto line_directive(s32 line, const std::filesystem::path& original) -> std::string {
    return "#line " + std::to_string(line) + " \"" + original.string() + "\"\n";
}

// Inserting the resources.hh include shifts every following line by
// one, so line_directive() re-syncs numbering back to the original file.
static auto add_resource_include(std::string source, const std::filesystem::path& original) -> std::string {
    constexpr std::string_view pragma = "#pragma once\n";
    const std::string include = "#include \"resources.hh\"\n";

    if (auto pos = source.find(pragma); pos != std::string::npos) {
        source.insert(pos + pragma.size(), include + line_directive(2, original));
    } else {
        source.insert(0, include + line_directive(1, original));
    }

    return source;
}

static auto process_file(
    const std::filesystem::path& assets_dir,
    const std::filesystem::path& input_dir,
    const std::filesystem::path& output_dir,
    Resource_Pool& pool,
    const std::filesystem::path& input) -> void {
    Preprocessor pp(assets_dir, pool);
    std::filesystem::path output = output_dir / std::filesystem::relative(input, input_dir);

    std::string result;
    bool has_embed = false;
    bool is_source = input.extension() == ".hh" || input.extension() == ".cc";
    bool is_asm = input.extension() == ".S";
    std::filesystem::path original = std::filesystem::absolute(input);

    if (is_source) {
        auto source = read_file(input);
        std::tie(has_embed, result) = pp.run(source);
        result = has_embed ? add_resource_include(result, original) : line_directive(1, original) + result;
    } else if (is_asm) {
        // .S is run through cpp, so #line applies here too.
        result = line_directive(1, original) + read_file(input);
    } else {
        result = read_file(input);
    }

    static std::mutex print_mutex;
    std::ofstream out(output);
    {
        std::lock_guard lock(print_mutex);
        std::println("Processing file: {}", input.string());
        std::println("    Output set to: {}", output.string());
        if (is_source) {
            if (has_embed) std::println("    {} has @embed, adding #include \"resources.hh\"", input.string());
        } else {
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
    for (auto const& file : std::filesystem::recursive_directory_iterator(input_dir)) {
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

    auto thread_count = std::max<u32>(1, std::thread::hardware_concurrency());
    thread_count = static_cast<u32>(std::min<usize>(thread_count, std::max<usize>(1, input_files.size())));

    {
        std::vector<std::jthread> workers;
        usize chunk = (input_files.size() + thread_count - 1) / thread_count;
        for (u32 t = 0; t < thread_count; ++t) {
            usize begin = t * chunk;
            usize end = std::min(input_files.size(), begin + chunk);
            if (begin >= end) continue;
            workers.emplace_back([&, begin, end] {
                for (usize i = begin; i < end; ++i)
                    process_file(assets_dir, input_dir, output_dir, pool, input_files[i]);
            });
        }
    }

    std::println("Writing {}", (output_dir / "resources.hh").string());
    write_resources_header(output_dir / "resources.hh", pool.get());
}
