#include <cassert>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <string>
#include <string_view>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct Resource {
    std::filesystem::path path;
    std::string name;
    std::vector<uint8_t> data;
};

static auto read_png_file(const std::string& path) -> std::vector<uint8_t> {
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

    std::vector<uint8_t> result(pixels, pixels + static_cast<size_t>(width * height * 4));

    stbi_image_free(pixels);
    return result;
}

template <typename T = std::string>
static T read_file(const std::string& path, std::ios::openmode mode = {}) {
    std::ifstream file(path, std::ios::binary | mode);
    if (!file)
        throw std::runtime_error("cannot open: " + path);

    return {
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>()
    };
}

static auto get_resource_data(const std::string& path) -> std::vector<uint8_t> {
    const std::string extension = std::filesystem::path{path}.extension();
    if (extension == ".png") return read_png_file(path);
    return read_file<std::vector<uint8_t>>(path);
}

static auto embed_identifier(const std::filesystem::path& path) -> std::string {
    std::string result = "__embedded__";
    for (char c : path.filename().string()) {
        if (std::isalnum(static_cast<unsigned char>(c))) result += c;
        else result += '_';
    }
    return result;
}

class Preprocessor {
public:
    auto run(std::string_view source) -> std::pair<bool, std::string> {
        clear_state(source);
        while (pos < input.size()) {
            switch (state) {
                case State::Normal:
                    normal_state();
                    break;
                case State::String:
                    string_state();
                    break;
                case State::Char:
                    char_state();
                    break;
                case State::LineComment:
                    line_comment_state();
                    break;
                case State::BlockComment:
                    block_comment_state();
                    break;
            }
        }
        return {has_embed, output};
    }

    auto get_resources() const -> const std::vector<Resource>& {
        return resources;
    }
private:
    enum class State {
        Normal,
        String,
        Char,
        LineComment,
        BlockComment
    };

    State state = State::Normal;
    std::vector<Resource> resources;

    std::string_view input;
    size_t pos = 0;
    std::string output;
    bool has_embed = false;

    void clear_state(std::string_view source) {
        input = source;
        pos = 0;
        output = "";
        has_embed = false;
    }

    auto peek(size_t offset = 0) const -> char {
        if (pos + offset >= input.size())
            return '\0';
        return input[pos + offset];
    }

    auto get() -> char {
        return input[pos++];
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

        if (get() != '"')
            throw std::runtime_error("@embed expects string");

        std::string path;
        while (peek() != '"' && peek() != '\0') // read string path
            path += get();

        if (get() != '"')
            throw std::runtime_error("unterminated @embed");

        while (std::isspace((unsigned char)peek())) // remove trailing spaces after string
            get();

        if (get() != ')')
            throw std::runtime_error("expected ')'");

        has_embed = true;
        Resource* resource = nullptr;
        for (auto& r : resources) {
            if (r.path == path) {
                resource = &r;
                break;
            }
        }

        if (!resource) {
            std::vector<uint8_t> data = get_resource_data(path);
            resources.push_back(Resource{
                path,
                embed_identifier(path),
                data,
            });
            resource = &resources.back();
        }

        output += resource->name; // replace @embed with resource name
    }
};

static void write_resources_header(
    const std::string& filename,
    const std::vector<Resource>& resources
) {
    std::ofstream out(filename);
    out << "#pragma once\n\n";

    for (const auto& r : resources) {
        std::println("    Writing {} as {} in resources.hh", r.path.string(), r.name);
        out << "static const Static_array<u8, " << r.data.size() << "> " << r.name  << "{\n";
        for (size_t i = 0; i < r.data.size(); i++) {
            if (i % 12 == 0)
                out << "    ";
            out << "0x"
                << std::hex
                << std::setw(2)
                << std::setfill('0')
                << (int)r.data[i]
                << std::dec
                << ",";
            if (i % 12 != 11)
                out << " ";
            if (i % 12 == 11)
                out << "\n";
        }
        out << "\n};\n\n";
    }
}

static auto add_resource_include(std::string source) -> std::string {
    constexpr std::string_view pragma = "#pragma once\n";

    if (auto pos = source.find(pragma); pos != std::string::npos)
        source.insert(pos + pragma.size(), "#include \"resources.hh\"\n");
    else
        source.insert(0, "#include \"resources.hh\"\n");

    return source;
}

auto main(int argc, char** argv) -> int {
    if (argc < 3) {
        std::println(stderr, "usage: preprocessing input_directory output_directory");
        return 1;
    }

    std::filesystem::path input_dir = argv[1];
    std::filesystem::path output_dir = argv[2];
    std::filesystem::create_directories(output_dir);
    std::println("Input dir: {}", input_dir.string());
    std::println("Output dir: {}", output_dir.string());
    assert((void("Input dir does not exist"), std::filesystem::exists(input_dir)));
    assert((void("Output dir does not exist"), std::filesystem::exists(output_dir)));

    Preprocessor pp;

    for (auto const& file: std::filesystem::recursive_directory_iterator(input_dir)) {
        std::filesystem::path input = file.path();
        if (std::filesystem::is_directory(input)) continue;
        std::println("Processing file: {}", input.string());

        std::filesystem::path output = output_dir / std::filesystem::relative(input, input_dir);
        std::println("    Output set to: {}", output.string());
        std::filesystem::create_directories(output.parent_path());
        std::ofstream out(output);

        std::string result;
        result.clear();
        bool has_embed;

        if (input.extension() == ".hh" || input.extension() == ".cc") {
            auto source = read_file(input);
            std::tie(has_embed, result) = pp.run(source);
            if (has_embed) {
                std::println("    {} has @embed, adding #include \"resources.hh\"", input.string());
                result = add_resource_include(result);
            }
        }
        else {
            std::println("    Omitting non .hh/.cc file: {}", input.string());
            result = read_file(input);
        }

        if (!out) {
            std::println(stderr, "    failed to write {}", output.string());
        }
        std::println("    Writing to {}", output.string());
        out << result;
    }
    std::println("Writing {}", (output_dir / "resources.hh").string());
    write_resources_header(
        output_dir / "resources.hh",
        pp.get_resources()
    );
}

