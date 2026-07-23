#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
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
    for (size_t i = 0; i < pixel_count; ++i) {
        uint8_t* p = pixels + i * 4;
        std::swap(p[0], p[2]); // Swap red channel with blue, since we require bgra
    }
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
};

static void write_resources_header(
    const std::string& filename,
    const std::vector<Resource>& resources
) {
    std::ofstream out(filename);
    out << "#pragma once\n\n";
    out << "#include \"kstd/gfx.hh\"\n\n";

    for (const auto& r : resources) {
        std::println("    Writing {} as {} in resources.hh", r.path.string(), r.name);
        auto& [width, height] = r.size;
        out << "static const gfx::Resource<" << r.data.size() << "> " <<  r.name << " = {\n";
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

