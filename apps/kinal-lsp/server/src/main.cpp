#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <fcntl.h>
#include <io.h>
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameW(void *hModule, wchar_t *lpFilename, unsigned long nSize);
#elif defined(__linux__)
#include <unistd.h>
#include <climits>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <climits>
#endif

extern "C" {
#include "kn/builtins.h"
#include "kn/diag.h"
#include "kn/lexer.h"
#include "kn/parser.h"
#include "kn/sema.h"
#include "kn/source.h"
#include "kn/std.h"
#include "kn/version.h"
}

namespace {

struct Json {
    enum class Kind { Null, Bool, Number, String, Object, Array };
    Kind kind = Kind::Null;
    bool b = false;
    double n = 0.0;
    std::string s;
    std::vector<std::pair<std::string, std::unique_ptr<Json>>> o;
    std::vector<Json> a;

    const Json *find(std::string_view key) const {
        if (kind != Kind::Object) return nullptr;
        for (const auto &kv : o) {
            if (kv.first == key) return kv.second.get();
        }
        return nullptr;
    }

    std::string str(std::string_view fb = "") const { return kind == Kind::String ? s : std::string(fb); }
    int i32(int fb = 0) const {
        if (kind != Kind::Number) return fb;
        if (n > (double)std::numeric_limits<int>::max()) return std::numeric_limits<int>::max();
        if (n < (double)std::numeric_limits<int>::min()) return std::numeric_limits<int>::min();
        return (int)n;
    }
    bool bval(bool fb = false) const { return kind == Kind::Bool ? b : fb; }
};

class JsonParser {
public:
    explicit JsonParser(std::string_view src) : src_(src) {}
    bool parse(Json *out) {
        if (!out) return false;
        ws();
        if (!value(*out)) return false;
        ws();
        return pos_ == src_.size();
    }

private:
    std::string_view src_;
    size_t pos_ = 0;

    void ws() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
                continue;
            }
            break;
        }
    }

    bool eat(char c) {
        ws();
        if (pos_ >= src_.size() || src_[pos_] != c) return false;
        ++pos_;
        return true;
    }

    bool lit(std::string_view t) {
        if (pos_ + t.size() > src_.size()) return false;
        if (src_.substr(pos_, t.size()) != t) return false;
        pos_ += t.size();
        return true;
    }

    static int hex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static void utf8(std::string &out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back((char)cp);
            return;
        }
        if (cp <= 0x7FF) {
            out.push_back((char)(0xC0 | (cp >> 6)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
            return;
        }
        if (cp <= 0xFFFF) {
            out.push_back((char)(0xE0 | (cp >> 12)));
            out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back((char)(0x80 | (cp & 0x3F)));
            return;
        }
        out.push_back((char)(0xF0 | (cp >> 18)));
        out.push_back((char)(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back((char)(0x80 | (cp & 0x3F)));
    }

    bool str(std::string &out) {
        if (!eat('"')) return false;
        out.clear();
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= src_.size()) return false;
            char e = src_[pos_++];
            switch (e) {
            case '"': out.push_back('"'); break;
            case '\\': out.push_back('\\'); break;
            case '/': out.push_back('/'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'u': {
                if (pos_ + 4 > src_.size()) return false;
                uint32_t cp = 0;
                for (int i = 0; i < 4; ++i) {
                    int h = hex(src_[pos_ + i]);
                    if (h < 0) return false;
                    cp = (cp << 4) | (uint32_t)h;
                }
                pos_ += 4;
                utf8(out, cp);
                break;
            }
            default:
                return false;
            }
        }
        return false;
    }

    bool num(double &out) {
        size_t st = pos_;
        if (pos_ >= src_.size()) return false;
        if (src_[pos_] == '-') ++pos_;
        if (pos_ >= src_.size()) return false;
        if (src_[pos_] == '0') {
            ++pos_;
        } else {
            if (!(src_[pos_] >= '1' && src_[pos_] <= '9')) return false;
            while (pos_ < src_.size() && std::isdigit((unsigned char)src_[pos_])) ++pos_;
        }
        if (pos_ < src_.size() && src_[pos_] == '.') {
            ++pos_;
            if (pos_ >= src_.size() || !std::isdigit((unsigned char)src_[pos_])) return false;
            while (pos_ < src_.size() && std::isdigit((unsigned char)src_[pos_])) ++pos_;
        }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            if (pos_ >= src_.size() || !std::isdigit((unsigned char)src_[pos_])) return false;
            while (pos_ < src_.size() && std::isdigit((unsigned char)src_[pos_])) ++pos_;
        }
        std::string tmp(src_.substr(st, pos_ - st));
        out = std::strtod(tmp.c_str(), nullptr);
        return true;
    }

    bool object(Json &out) {
        if (!eat('{')) return false;
        out.kind = Json::Kind::Object;
        out.o.clear();
        ws();
        if (eat('}')) return true;
        while (true) {
            std::string k;
            if (!str(k)) return false;
            if (!eat(':')) return false;
            auto v = std::make_unique<Json>();
            if (!value(*v)) return false;
            out.o.emplace_back(std::move(k), std::move(v));
            ws();
            if (eat('}')) break;
            if (!eat(',')) return false;
        }
        return true;
    }

    bool array(Json &out) {
        if (!eat('[')) return false;
        out.kind = Json::Kind::Array;
        out.a.clear();
        ws();
        if (eat(']')) return true;
        while (true) {
            Json v;
            if (!value(v)) return false;
            out.a.push_back(std::move(v));
            ws();
            if (eat(']')) break;
            if (!eat(',')) return false;
        }
        return true;
    }

    bool value(Json &out) {
        ws();
        if (pos_ >= src_.size()) return false;
        char c = src_[pos_];
        if (c == '{') return object(out);
        if (c == '[') return array(out);
        if (c == '"') {
            out.kind = Json::Kind::String;
            return str(out.s);
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            out.kind = Json::Kind::Number;
            return num(out.n);
        }
        if (lit("true")) {
            out.kind = Json::Kind::Bool;
            out.b = true;
            return true;
        }
        if (lit("false")) {
            out.kind = Json::Kind::Bool;
            out.b = false;
            return true;
        }
        if (lit("null")) {
            out.kind = Json::Kind::Null;
            return true;
        }
        return false;
    }
};

std::string esc(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out.push_back(c); break;
        }
    }
    return out;
}

bool read_message(std::string &out) {
    out.clear();
    std::string line;
    size_t len = 0;
    bool have = false;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) break;
        constexpr std::string_view p = "Content-Length:";
        if (line.rfind(p.data(), 0) == 0) {
            std::string n = line.substr(p.size());
            while (!n.empty() && std::isspace((unsigned char)n.front())) n.erase(n.begin());
            len = (size_t)std::strtoull(n.c_str(), nullptr, 10);
            have = true;
        }
    }
    if (!have) return false;
    out.resize(len);
    std::cin.read(out.data(), (std::streamsize)len);
    return (size_t)std::cin.gcount() == len;
}

void write_message(const std::string &payload) {
    std::cout << "Content-Length: " << payload.size() << "\r\n\r\n";
    std::cout << payload;
    std::cout.flush();
}

std::string find_id_raw(const std::string &json) {
    const std::string key = "\"id\"";
    size_t p = json.find(key);
    if (p == std::string::npos) return {};
    p = json.find(':', p + key.size());
    if (p == std::string::npos) return {};
    ++p;
    while (p < json.size() && std::isspace((unsigned char)json[p])) ++p;
    if (p >= json.size()) return {};
    if (json[p] == '"') {
        size_t q = p + 1;
        while (q < json.size()) {
            if (json[q] == '"' && json[q - 1] != '\\') break;
            ++q;
        }
        if (q < json.size()) return json.substr(p, q - p + 1);
        return {};
    }
    size_t q = p;
    while (q < json.size() && (json[q] == '-' || std::isdigit((unsigned char)json[q]))) ++q;
    return json.substr(p, q - p);
}

std::string response(const std::string &id_raw, const std::string &result_json) {
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"2.0\",\"id\":" << id_raw << ",\"result\":" << result_json << "}";
    return oss.str();
}

void notify(std::string_view method, const std::string &params_json) {
    std::ostringstream oss;
    oss << "{\"jsonrpc\":\"2.0\",\"method\":\"" << esc(method) << "\",\"params\":" << params_json << "}";
    write_message(oss.str());
}

struct Range {
    int sl = 0;
    int sc = 0;
    int el = 0;
    int ec = 0;

    bool contains(int l, int c) const {
        if (l < sl || l > el) return false;
        if (l == sl && c < sc) return false;
        if (l == el && c >= ec) return false;
        return true;
    }

    int score() const {
        if (el == sl) return std::max(1, ec - sc);
        return (el - sl) * 100000 + std::max(1, ec - sc);
    }
};

Range mk_range(int line1, int col1, int len) {
    int line = std::max(1, line1);
    int col = std::max(1, col1);
    int n = std::max(1, len);
    return Range{line - 1, col - 1, line - 1, col - 1 + n};
}

std::string range_json(const Range &r) {
    std::ostringstream oss;
    oss << "{\"start\":{\"line\":" << r.sl << ",\"character\":" << r.sc
        << "},\"end\":{\"line\":" << r.el << ",\"character\":" << r.ec << "}}";
    return oss.str();
}

int hexv(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

std::string decode_pct(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        char c = s[i];
        if (c == '%' && i + 2 < s.size()) {
            int hi = hexv(s[i + 1]);
            int lo = hexv(s[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back((char)((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(c);
    }
    return out;
}

std::string uri_to_path(std::string_view uri) {
    constexpr std::string_view p = "file://";
    if (uri.rfind(p, 0) != 0) return std::string(uri);
    std::string path = decode_pct(uri.substr(p.size()));
#if defined(_WIN32)
    if (path.size() >= 3 && path[0] == '/' && std::isalpha((unsigned char)path[1]) && path[2] == ':') {
        path.erase(path.begin());
    }
    std::replace(path.begin(), path.end(), '/', '\\');
#endif
    return path;
}

bool uri_unreserved(unsigned char c) {
    if (c >= 'a' && c <= 'z') return true;
    if (c >= 'A' && c <= 'Z') return true;
    if (c >= '0' && c <= '9') return true;
    switch (c) {
    case '-':
    case '.':
    case '_':
    case '~':
    case '/':
        return true;
    default:
        return false;
    }
}

std::string path_to_uri(const std::string &path) {
    std::string p = path;
#if defined(_WIN32)
    std::replace(p.begin(), p.end(), '\\', '/');
    if (p.size() >= 2 && std::isalpha((unsigned char)p[0]) && p[1] == ':') {
        p.insert(p.begin(), '/');
        p[1] = (char)std::tolower((unsigned char)p[1]);
    }
#endif

    std::ostringstream oss;
#if defined(_WIN32)
    oss << "file://";
#else
    oss << "file://";
#endif
    for (unsigned char c : p) {
        if (uri_unreserved(c)) {
            oss << (char)c;
        } else {
            static const char *hex = "0123456789ABCDEF";
            oss << '%' << hex[(c >> 4) & 0xF] << hex[c & 0xF];
        }
    }
    return oss.str();
}

struct Tok {
    TokenType type = TOK_EOF;
    std::string text;
    int line = 1;
    int col = 1;
    int len = 1;
    int idx = 0;
};

enum class SymKind {
    Class,
    Struct,
    Interface,
    Enum,
    EnumMember,
    Function,
    Method,
    Field,
    Property,
    Variable,
    Parameter
};

struct Symbol {
    int id = 0;
    SymKind kind = SymKind::Variable;
    std::string name;
    std::string container;
    std::string type_text; // var/field/param type, if known
    std::string ret_text;  // function/method return type, if known
    std::vector<std::pair<std::string, std::string>> params; // (type, name)
    std::string key;
    Range decl;
    int decl_tok_idx = -1;
    bool is_static = false;
    bool has_const_value = false;
    int64_t const_value = 0;
};

struct Occ {
    int sym_id = 0;
    std::string key;
    Range range;
    bool decl = false;
    int tok_idx = -1;
};

struct Diag {
    std::string code;
    std::string message;
    std::string source;
    int severity = 1;
    Range range;
    bool unnecessary = false;
};

struct Completion {
    std::string label;
    int kind = 14;
    std::string detail;
    std::string insert_text;
    int insert_text_format = 0; // 0=plain, 2=snippet
};

static bool is_builtin_meta_attr(std::string_view s) {
    return s == "Docs" || s == "Deprecated" || s == "Since" ||
           s == "Example" || s == "Experimental";
}

static bool is_string_prefix_name(std::string_view s) {
    return s == "r" || s == "raw" ||
           s == "f" || s == "format" ||
           s == "c" || s == "chars";
}

struct Analysis {
    std::vector<Tok> toks;
    std::vector<Symbol> syms;
    std::vector<Occ> occs;
    std::vector<Diag> diags;
    std::vector<Range> comment_ranges; // per-line comment spans, 0-based
    std::vector<Range> unnecessary_ranges;
    std::unordered_map<std::string, std::string> import_alias_to_module;
    std::unordered_map<std::string, std::string> import_symbol_to_qualified;
    std::vector<std::string> import_open_modules;
    std::vector<uint32_t> sem_data;
    std::vector<Completion> completions; // precomputed global-ish list, completion requests still filter by context
};

struct SemTokSpan {
    int line = 0;
    int col = 0;
    int len = 1;
    int type = 0;
    int mods = 0;
};

struct DeclInfo {
    std::string uri;
    std::string unit;
    std::string name;
    std::string qname;
    std::string container;
    std::string owner_qname;
    std::string type_text;
    std::string ret_text;
    std::vector<std::pair<std::string, std::string>> params;
    SymKind kind = SymKind::Variable;
    Range decl;
    bool is_static = false;
    bool has_const_value = false;
    int64_t const_value = 0;
};

struct DeclDocIndex {
    std::string unit;
    std::vector<DeclInfo> decls;
};

bool is_hidden_internal_name(std::string_view s);

struct ServerConfig {
    std::string diag_lang = "en"; // "en" or "zh"
    std::string locale_file;
};

extern ServerConfig g_cfg;

bool is_kw(TokenType t) {
    switch (t) {
    case TOK_UNIT:
    case TOK_GET:
    case TOK_SET:
    case TOK_BY:
    case TOK_PROPERTY:
    case TOK_FUNCTION:
    case TOK_RETURN:
    case TOK_IF:
    case TOK_ELSE:
    case TOK_ELSEIF:
    case TOK_WHILE:
    case TOK_FOR:
    case TOK_SWITCH:
    case TOK_CASE:
    case TOK_BREAK:
    case TOK_CONTINUE:
    case TOK_TRY:
    case TOK_CATCH:
    case TOK_THROW:
    case TOK_CONST:
    case TOK_VAR:
    case TOK_SAFE:
    case TOK_TRUSTED:
    case TOK_UNSAFE:
    case TOK_ASYNC:
    case TOK_AWAIT:
    case TOK_EXTERN:
    case TOK_CLASS:
    case TOK_INTERFACE:
    case TOK_STRUCT:
    case TOK_ENUM:
    case TOK_PUBLIC:
    case TOK_PRIVATE:
    case TOK_PROTECTED:
    case TOK_INTERNAL:
    case TOK_STATIC:
    case TOK_VIRTUAL:
    case TOK_OVERRIDE:
    case TOK_ABSTRACT:
    case TOK_SEALED:
    case TOK_CONSTRUCTOR:
    case TOK_NEW:
    case TOK_THIS:
    case TOK_BASE:
    case TOK_BLOCK:
    case TOK_RECORD:
    case TOK_JUMP:
    case TOK_DELEGATE:
    case TOK_TRUE:
    case TOK_FALSE:
    case TOK_IS:
    case TOK_DEFAULT:
    case TOK_NULL:
        return true;
    default:
        return false;
    }
}
bool is_type_kw(TokenType t) { return t >= TOK_TYPE_VOID && t <= TOK_TYPE_ANY; }

bool is_op(TokenType t) {
    switch (t) {
    case TOK_PLUS:
    case TOK_MINUS:
    case TOK_STAR:
    case TOK_SLASH:
    case TOK_PERCENT:
    case TOK_ASSIGN:
    case TOK_PLUSPLUS:
    case TOK_MINUSMINUS:
    case TOK_PLUS_ASSIGN:
    case TOK_MINUS_ASSIGN:
    case TOK_STAR_ASSIGN:
    case TOK_SLASH_ASSIGN:
    case TOK_PERCENT_ASSIGN:
    case TOK_EQ:
    case TOK_NE:
    case TOK_LT:
    case TOK_LE:
    case TOK_GT:
    case TOK_GE:
    case TOK_ANDAND:
    case TOK_OROR:
    case TOK_NOT:
    case TOK_AMP:
    case TOK_BITOR:
    case TOK_XOR:
    case TOK_SHL:
    case TOK_SHR:
    case TOK_TILDE:
    case TOK_QUESTION:
    case TOK_COLON:
    case TOK_AT:
        return true;
    default:
        return false;
    }
}

std::vector<Tok> lex_all(const std::string &text) {
    std::vector<Tok> out;
    Lexer l;
    lex_init(&l, text.c_str());
    int idx = 0;
    for (;;) {
        Token t = lex_next(&l);
        if (t.type == TOK_EOF) break;
        Tok tk;
        tk.type = t.type;
        tk.text.assign(t.start, t.length);
        tk.line = t.line;
        tk.col = t.col;
        tk.len = std::max(1, (int)t.length);
        tk.idx = idx++;
        out.push_back(std::move(tk));
    }
    return out;
}

// The compiler lexer discards comments. For semantic tokens we still want comment styling,
// so we rescan the raw text to produce per-line comment spans.
std::vector<Range> scan_comment_ranges(const std::string &text) {
    std::vector<Range> out;
    out.reserve(64);

    enum class Mode { Normal, String, Char, LineComment, BlockComment };
    Mode m = Mode::Normal;

    int line = 0;
    int col = 0;
    int start_line = 0;
    int start_col = 0;

    auto push_seg = [&](int sl, int sc, int el, int ec) {
        if (sl != el) return; // semantic tokens can't span lines
        if (ec <= sc) ec = sc + 1;
        out.push_back(Range{sl, sc, el, ec});
    };

    const size_t n = text.size();
    for (size_t i = 0; i < n; ++i) {
        const char c = text[i];
        const char n1 = (i + 1 < n) ? text[i + 1] : 0;

        if (m == Mode::LineComment) {
            if (c == '\n') {
                push_seg(start_line, start_col, start_line, col);
                m = Mode::Normal;
                line++;
                col = 0;
                continue;
            }
            col++;
            continue;
        }

        if (m == Mode::BlockComment) {
            if (c == '*' && n1 == '/') {
                col += 2;
                i++;
                push_seg(start_line, start_col, start_line, col);
                m = Mode::Normal;
                continue;
            }
            if (c == '\n') {
                push_seg(start_line, start_col, start_line, col);
                line++;
                col = 0;
                start_line = line;
                start_col = 0;
                continue;
            }
            col++;
            continue;
        }

        if (m == Mode::String) {
            if (c == '\\' && i + 1 < n) {
                // Skip escaped char.
                col += 2;
                i++;
                continue;
            }
            if (c == '"') {
                m = Mode::Normal;
                col++;
                continue;
            }
            if (c == '\n') {
                // Unterminated string; stop treating as string after line break.
                m = Mode::Normal;
                line++;
                col = 0;
                continue;
            }
            col++;
            continue;
        }

        if (m == Mode::Char) {
            if (c == '\\' && i + 1 < n) {
                col += 2;
                i++;
                continue;
            }
            if (c == '\'') {
                m = Mode::Normal;
                col++;
                continue;
            }
            if (c == '\n') {
                m = Mode::Normal;
                line++;
                col = 0;
                continue;
            }
            col++;
            continue;
        }

        // Normal mode.
        if (c == '/' && n1 == '/') {
            m = Mode::LineComment;
            start_line = line;
            start_col = col;
            col += 2;
            i++;
            continue;
        }
        if (c == '/' && n1 == '*') {
            m = Mode::BlockComment;
            start_line = line;
            start_col = col;
            col += 2;
            i++;
            continue;
        }
        if (c == '"') {
            m = Mode::String;
            col++;
            continue;
        }
        if (c == '\'') {
            m = Mode::Char;
            col++;
            continue;
        }
        // Treat CRLF as a single newline for VSCode/LSP position semantics.
        if (c == '\r') {
            if (n1 == '\n') continue;
            line++;
            col = 0;
            continue;
        }
        if (c == '\n') {
            line++;
            col = 0;
            continue;
        }
        col++;
    }

    // Flush unterminated comments at EOF.
    if (m == Mode::LineComment || m == Mode::BlockComment) {
        push_seg(start_line, start_col, start_line, col);
    }

    return out;
}

int sem_type_for_kind(SymKind k) {
    switch (k) {
    case SymKind::Class: return 2;
    case SymKind::Struct: return 3;
    case SymKind::Interface: return 4;
    case SymKind::Enum: return 5;
    case SymKind::EnumMember: return 6;
    case SymKind::Function: return 7;
    case SymKind::Method: return 8;
    case SymKind::Field: return 11;
    case SymKind::Property: return 11;
    case SymKind::Variable: return 9;
    case SymKind::Parameter: return 10;
    }
    return 9;
}

int completion_kind_for(SymKind k) {
    switch (k) {
    case SymKind::Class: return 7;
    case SymKind::Struct: return 22;
    case SymKind::Interface: return 8;
    case SymKind::Enum: return 13;
    case SymKind::EnumMember: return 20;
    case SymKind::Function: return 3;
    case SymKind::Method: return 2;
    case SymKind::Field: return 10;
    case SymKind::Property: return 10;
    case SymKind::Variable: return 6;
    case SymKind::Parameter: return 6;
    }
    return 1;
}

bool prev_allows_decl(const std::vector<Tok> &toks, int i) {
    if (i <= 0) return true;
    TokenType p = toks[(size_t)(i - 1)].type;
    return p == TOK_SEMI || p == TOK_LBRACE || p == TOK_RBRACE || p == TOK_COMMA;
}

bool typeish(TokenType t) {
    return t == TOK_ID || is_type_kw(t) || t == TOK_DOT || t == TOK_STAR || t == TOK_AMP ||
           t == TOK_LT || t == TOK_GT || t == TOK_LBRACKET || t == TOK_RBRACKET || t == TOK_NUMBER;
}

bool qual_segment_tok(TokenType t) {
    if (t == TOK_ID || is_type_kw(t)) return true;
    switch (t) {
    case TOK_FUNCTION:
    case TOK_BLOCK:
    case TOK_CLASS:
    case TOK_INTERFACE:
    case TOK_STRUCT:
    case TOK_ENUM:
    case TOK_ASYNC:
    case TOK_RECORD:
    case TOK_JUMP:
        return true;
    default:
        return false;
    }
}

std::string join_tok_text(const std::vector<Tok> &toks, int start, int end) {
    if (start < 0) start = 0;
    if (end >= (int)toks.size()) end = (int)toks.size() - 1;
    if (start > end) return {};
    std::string out;
    for (int i = start; i <= end; ++i) out += toks[(size_t)i].text;
    return out;
}

std::string normalize_base_type(std::string t) {
    while (!t.empty() && std::isspace((unsigned char)t.back())) t.pop_back();
    while (!t.empty() && (t.back() == '*' || std::isspace((unsigned char)t.back()))) t.pop_back();
    size_t p = t.find('[');
    if (p != std::string::npos) t.resize(p);
    p = t.find('<');
    if (p != std::string::npos) t.resize(p);
    while (!t.empty() && std::isspace((unsigned char)t.back())) t.pop_back();
    return t;
}

int64_t parse_i64_text(std::string_view text, int64_t fallback = 0) {
    if (text.empty()) return fallback;
    std::string tmp(text);
    char *end = nullptr;
    long long v = std::strtoll(tmp.c_str(), &end, 0);
    if (!end || *end != 0) return fallback;
    return (int64_t)v;
}

bool text_starts_with(std::string_view s, std::string_view pfx) {
    return pfx.empty() || (s.size() >= pfx.size() && s.substr(0, pfx.size()) == pfx);
}

std::vector<std::string> project_child_unit_segments(std::string_view prefix);
std::vector<DeclInfo> project_top_level_decls_for_module(const std::string &module);
std::vector<DeclInfo> project_member_decls_for_owner(const std::string &owner_qname);
std::optional<DeclInfo> project_top_level_decl(const std::string &module, std::string_view name);
std::optional<DeclInfo> project_member_decl(const std::string &owner_qname, std::string_view name);
std::optional<DeclInfo> project_qualified_top_level_decl(std::string_view qualified);
std::optional<DeclInfo> project_imported_symbol_decl(const Analysis &an, std::string_view local);
std::optional<DeclInfo> project_visible_top_level_decl_in_analysis(const Analysis &an,
                                                                   std::string_view current_unit,
                                                                   std::string_view name);
std::string decl_completion_detail(const DeclInfo &d);
std::string decl_sig(const DeclInfo &d);
std::vector<std::string> discover_declaring_unit_uris_from_roots(std::string_view unit,
                                                                 const std::vector<std::filesystem::path> &roots);
extern std::string g_exe_dir;
struct Doc;
const Symbol *sym_by_id(const Analysis &an, int id);
std::optional<DeclInfo> project_member_decl_via_receiver(const Doc &d,
                                                         const std::vector<int> &seg_tok_i,
                                                         const std::vector<std::string> &segs,
                                                         int seg_idx);

bool parse_qualified_name(const std::vector<Tok> &toks, int &i, std::string &out) {
    out.clear();
    if (i < 0 || i >= (int)toks.size()) return false;
    if (!qual_segment_tok(toks[(size_t)i].type)) return false;
    out += toks[(size_t)i].text;
    ++i;
    while (i + 1 < (int)toks.size() && toks[(size_t)i].type == TOK_DOT) {
        if (!qual_segment_tok(toks[(size_t)(i + 1)].type)) break;
        out.push_back('.');
        out += toks[(size_t)(i + 1)].text;
        i += 2;
    }
    return true;
}

Range range_from_tokens(const Tok &a, const Tok &b);

bool parse_qualified_name_span(const std::vector<Tok> &toks, int &i, std::string &out, Range &span, int *out_start_i = nullptr,
                              int *out_end_i = nullptr) {
    span = Range{};
    out.clear();
    if (i < 0 || i >= (int)toks.size()) return false;
    if (!qual_segment_tok(toks[(size_t)i].type)) return false;
    int start = i;
    out += toks[(size_t)i].text;
    ++i;
    while (i + 1 < (int)toks.size() && toks[(size_t)i].type == TOK_DOT) {
        if (!qual_segment_tok(toks[(size_t)(i + 1)].type)) break;
        out.push_back('.');
        out += toks[(size_t)(i + 1)].text;
        i += 2;
    }
    int end = i - 1;
    span = range_from_tokens(toks[(size_t)start], toks[(size_t)end]);
    if (out_start_i) *out_start_i = start;
    if (out_end_i) *out_end_i = end;
    return true;
}

std::string last_segment(std::string_view q) {
    size_t p = q.rfind('.');
    if (p == std::string_view::npos) return std::string(q);
    return std::string(q.substr(p + 1));
}

std::string prefix_before_last_segment(std::string_view q) {
    size_t p = q.rfind('.');
    if (p == std::string_view::npos) return {};
    return std::string(q.substr(0, p));
}

struct BuiltinObjectTypeInfo {
    const char *name;
    const char *qname;
    const char *base_qname;
};

const BuiltinObjectTypeInfo *builtin_object_type_info_by_name(std::string_view name) {
    static const BuiltinObjectTypeInfo kInfos[] = {
        {"Class", "IO.Type.Object.Class", nullptr},
        {"Function", "IO.Type.Object.Function", "IO.Type.Object.Class"},
        {"Block", "IO.Type.Object.Block", "IO.Type.Object.Class"},
    };
    for (const auto &it : kInfos) {
        if (name == it.name) return &it;
    }
    return nullptr;
}

std::optional<std::string> canonical_builtin_object_qname(std::string_view q) {
    if (q.empty()) return std::nullopt;
    constexpr std::string_view short_pfx = "Object.";
    if (q.size() > short_pfx.size() && q.substr(0, short_pfx.size()) == short_pfx) {
        if (const BuiltinObjectTypeInfo *it = builtin_object_type_info_by_name(q.substr(short_pfx.size())))
            return std::string(it->qname);
    }
    constexpr std::string_view full_pfx = "IO.Type.Object.";
    if (q.size() > full_pfx.size() && q.substr(0, full_pfx.size()) == full_pfx) {
        if (const BuiltinObjectTypeInfo *it = builtin_object_type_info_by_name(q.substr(full_pfx.size())))
            return std::string(it->qname);
    }
    return std::nullopt;
}

bool builtin_object_is_namespace(std::string_view q) {
    return q == "IO.Type.Object";
}

bool builtin_object_namespace_has_children(std::string_view q) {
    return q == "Object" || q == "IO.Type" || q == "IO.Type.Object";
}

std::vector<std::pair<std::string, int>> builtin_object_children(std::string_view prefix) {
    std::vector<std::pair<std::string, int>> out;
    if (prefix == "IO.Type") {
        out.push_back({"Object", 9});
        return out;
    }
    if (prefix == "Object" || prefix == "IO.Type.Object") {
        out.push_back({"Class", 7});
        out.push_back({"Function", 7});
        out.push_back({"Block", 7});
    }
    return out;
}

std::string builtin_object_decl_sig(std::string_view qname) {
    auto cq = canonical_builtin_object_qname(qname);
    if (!cq) return {};
    std::string name = last_segment(*cq);
    if (const BuiltinObjectTypeInfo *it = builtin_object_type_info_by_name(name)) {
        if (it->base_qname && it->base_qname[0])
            return "Class " + *cq + " By " + std::string(it->base_qname);
        return "Class " + *cq;
    }
    return {};
}

static constexpr const char *k_unit_key_prefix = "unit|";

struct UnitIndexItem {
    std::string unit; // fully-qualified (e.g. App.Test, IO.Console)
    Range range;
    bool decl = false; // Unit decl vs Get ref
};

struct UnitDocIndex {
    std::vector<UnitIndexItem> items;
};

std::unordered_map<std::string, UnitDocIndex> g_unit_docs; // uri -> unit index (decls + Get refs)
std::unordered_set<std::string> g_known_units;             // unit names declared in workspace
std::vector<std::string> g_workspace_roots;                // filesystem paths
extern std::unordered_map<std::string, DeclDocIndex> g_decl_doc_cache;

struct GetQualifiedInfo {
    std::string module;
    std::string member;
    bool symbol_import = false;
};

bool unit_decl_exists(std::string_view unit);
GetQualifiedInfo classify_get_qualified(const std::string &q, bool force_module);

void parse_imports_from_tokens(Analysis &an) {
    an.import_alias_to_module.clear();
    an.import_symbol_to_qualified.clear();
    an.import_open_modules.clear();

    int brace_depth = 0;
    for (int i = 0; i < (int)an.toks.size(); ++i) {
        TokenType tt = an.toks[(size_t)i].type;
        if (tt == TOK_LBRACE) brace_depth++;
        if (tt == TOK_RBRACE) brace_depth = std::max(0, brace_depth - 1);
        if (brace_depth != 0) continue;

        if (tt != TOK_GET) continue;
        int j = i + 1;
        std::string first;
        if (!parse_qualified_name(an.toks, j, first)) continue;

        if (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_BY) {
            ++j;
            std::string target;
            if (!parse_qualified_name(an.toks, j, target)) continue;

            GetQualifiedInfo info = classify_get_qualified(target, false);
            if (info.symbol_import) {
                an.import_symbol_to_qualified[first] = info.module + "." + info.member;
            } else if (kn_std_module_has(target.c_str()) || unit_decl_exists(target)) {
                an.import_alias_to_module[first] = target;
            } else {
                std::string module = prefix_before_last_segment(target);
                std::string member = last_segment(target);
                if (!module.empty() && kn_std_module_has(module.c_str())) {
                    an.import_symbol_to_qualified[first] = module + "." + member;
                } else {
                    an.import_alias_to_module[first] = target;
                }
            }
        } else if (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_LBRACE) {
            ++j;
            std::string module = first;
            while (j < (int)an.toks.size() && an.toks[(size_t)j].type != TOK_RBRACE) {
                std::string local;
                if (!parse_qualified_name(an.toks, j, local)) break;
                std::string remote = local;
                if (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_BY) {
                    ++j;
                    std::string rem;
                    if (parse_qualified_name(an.toks, j, rem)) remote = rem;
                }
                an.import_symbol_to_qualified[last_segment(local)] = module + "." + last_segment(remote);
                if (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_COMMA) ++j;
            }
        } else {
            GetQualifiedInfo info = classify_get_qualified(first, false);
            if (info.symbol_import) an.import_symbol_to_qualified[info.member] = info.module + "." + info.member;
            else an.import_open_modules.push_back(info.module);
        }
    }
}

bool is_unit_key(std::string_view key) { return key.rfind(k_unit_key_prefix, 0) == 0; }

std::string unit_name_from_key(std::string_view key) {
    if (!is_unit_key(key)) return std::string(key);
    return std::string(key.substr(std::string_view(k_unit_key_prefix).size()));
}

bool unit_decl_exists(std::string_view unit) {
    return g_known_units.find(std::string(unit)) != g_known_units.end();
}

GetQualifiedInfo classify_get_qualified(const std::string &q, bool force_module) {
    GetQualifiedInfo out;
    out.module = q;
    if (force_module || q.empty()) return out;
    if (kn_std_module_has(q.c_str())) return out;

    std::string module = prefix_before_last_segment(q);
    std::string member = last_segment(q);
    if (!module.empty() && !member.empty()) {
        if (kn_std_module_has(module.c_str())) {
            out.module = module;
            out.member = member;
            out.symbol_import = true;
            return out;
        }
        if (unit_decl_exists(module) && project_top_level_decl(module, member).has_value()) {
            out.module = module;
            out.member = member;
            out.symbol_import = true;
            return out;
        }
    }

    return out;
}

std::string module_from_get_qualified(const std::string &q, bool force_module) {
    return classify_get_qualified(q, force_module).module;
}

UnitDocIndex index_units_from_tokens(const std::vector<Tok> &toks) {
    UnitDocIndex idx;
    idx.items.clear();

    auto add_seg_items = [&](int start, int end, bool decl_full, const std::string &override_unit) {
        if (start < 0 || end < 0 || start >= (int)toks.size() || end >= (int)toks.size()) return;
        std::string prefix;
        for (int k = start; k <= end; ++k) {
            const Tok &t = toks[(size_t)k];
            if (!qual_segment_tok(t.type)) continue;
            if (!prefix.empty()) prefix.push_back('.');
            prefix += t.text;
            const bool is_full = (k == end);
            const std::string unit_name = override_unit.empty() ? prefix : override_unit;
            idx.items.push_back(UnitIndexItem{unit_name, mk_range(t.line, t.col, t.len), decl_full && is_full});
        }
    };

    int brace_depth = 0;
    for (int i = 0; i < (int)toks.size(); ++i) {
        TokenType tt = toks[(size_t)i].type;
        if (tt == TOK_LBRACE) brace_depth++;
        if (tt == TOK_RBRACE) brace_depth = std::max(0, brace_depth - 1);
        if (brace_depth != 0) continue;

        if (tt == TOK_UNIT) {
            int j = i + 1;
            std::string unit;
            Range span;
            int start = -1, end = -1;
            if (!parse_qualified_name_span(toks, j, unit, span, &start, &end)) continue;
            add_seg_items(start, end, true, std::string{});
            continue;
        }

        if (tt == TOK_GET) {
            int j = i + 1;
            std::string first;
            Range first_span;
            int first_start = -1, first_end = -1;
            if (!parse_qualified_name_span(toks, j, first, first_span, &first_start, &first_end)) continue;
            bool first_has_list = (j < (int)toks.size() && toks[(size_t)j].type == TOK_LBRACE);

            if (j < (int)toks.size() && toks[(size_t)j].type == TOK_BY) {
                ++j;
                std::string target;
                Range target_span;
                int target_start = -1, target_end = -1;
                if (!parse_qualified_name_span(toks, j, target, target_span, &target_start, &target_end)) continue;
                bool target_has_list = (j < (int)toks.size() && toks[(size_t)j].type == TOK_LBRACE);
                GetQualifiedInfo info = classify_get_qualified(target, target_has_list);
                std::string module = info.module;
                std::string local_target = info.symbol_import ? (info.module + "." + info.member) : module;
                // Clickable local alias/name should jump to the target module.
                add_seg_items(first_start, first_end, false, local_target);
                // Clicking the target path segments should jump by prefix.
                add_seg_items(target_start, target_end, false, std::string{});
                continue;
            }

            std::string module = module_from_get_qualified(first, first_has_list);
            (void)module;
            add_seg_items(first_start, first_end, false, std::string{});
            continue;
        }
    }

    return idx;
}

void rebuild_known_units(void) {
    g_known_units.clear();
    for (const auto &kv : g_unit_docs) {
        for (const UnitIndexItem &it : kv.second.items) {
            if (it.decl) g_known_units.insert(it.unit);
        }
    }
}

void update_unit_index_for_uri(const std::string &uri, const std::vector<Tok> &toks) {
    std::string key = uri;
    if (key.rfind("file://", 0) == 0) key = path_to_uri(uri_to_path(key));
    g_unit_docs[key] = index_units_from_tokens(toks);
    g_decl_doc_cache.erase(key);
    rebuild_known_units();
}

bool should_skip_index_dir(std::string_view name) {
    if (name == ".git" || name == ".github") return true;
    if (name == "build" || name == "release") return true;
    if (name == "third_party") return true;
    if (name == "node_modules") return true;
    if (name == "out") return true;
    if (name == "stdpkg" || name == "kpkg") return true;
    if (name == "build-clang" || name == "build-clang-release") return true;
    return false;
}

bool is_indexable_source_path(const std::filesystem::path &p) {
    auto ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return ext == ".kn" || ext == ".kinal" || ext == ".fx";
}

std::optional<std::string> read_file_text(const std::filesystem::path &p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec) || ec) return std::nullopt;
    auto sz = std::filesystem::file_size(p, ec);
    if (ec) return std::nullopt;
    if (sz > 4 * 1024 * 1024) return std::nullopt; // avoid indexing huge files
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::nullopt;
    std::string s;
    s.resize((size_t)sz);
    f.read(s.data(), (std::streamsize)sz);
    if (!f) return std::nullopt;
    return s;
}

std::string normalize_fs_path(const std::string &path) {
    if (path.empty()) return {};
    std::filesystem::path p(path);
    p = p.lexically_normal();
    std::string s = p.make_preferred().string();
#if defined(_WIN32)
    std::replace(s.begin(), s.end(), '/', '\\');
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
#endif
    return s;
}

std::string canonical_file_uri(const std::string &uri) {
    if (uri.empty()) return {};
    if (uri.rfind("file://", 0) == 0) return path_to_uri(uri_to_path(uri));
    return uri;
}

bool read_u32_le_bytes(const std::vector<unsigned char> &data, size_t &off, uint32_t &out) {
    if (off + 4 > data.size()) return false;
    out = (uint32_t)data[off] |
          ((uint32_t)data[off + 1] << 8) |
          ((uint32_t)data[off + 2] << 16) |
          ((uint32_t)data[off + 3] << 24);
    off += 4;
    return true;
}

bool read_u64_le_bytes(const std::vector<unsigned char> &data, size_t &off, uint64_t &out) {
    if (off + 8 > data.size()) return false;
    out = (uint64_t)data[off] |
          ((uint64_t)data[off + 1] << 8) |
          ((uint64_t)data[off + 2] << 16) |
          ((uint64_t)data[off + 3] << 24) |
          ((uint64_t)data[off + 4] << 32) |
          ((uint64_t)data[off + 5] << 40) |
          ((uint64_t)data[off + 6] << 48) |
          ((uint64_t)data[off + 7] << 56);
    off += 8;
    return true;
}

std::string sanitize_cache_segment(std::string s) {
    for (char &c : s) {
        switch (c) {
        case '\\':
        case '/':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
            c = '_';
            break;
        default:
            break;
        }
    }
    if (s.empty()) s = "_";
    return s;
}

// ---- klib virtual URI support ----
// Cache root for klib extraction (system cache directory).
std::filesystem::path g_klib_cache_base;
// Set of normalized klib cache source root paths (used to identify klib files).
std::unordered_set<std::string> g_klib_cache_source_roots;

std::filesystem::path lsp_system_cache_dir() {
#if defined(_WIN32)
    const char *local = std::getenv("LOCALAPPDATA");
    if (local && local[0]) return std::filesystem::path(local) / "kinal" / "lsp-klib";
    const char *appdata = std::getenv("APPDATA");
    if (appdata && appdata[0]) return std::filesystem::path(appdata) / "kinal" / "lsp-klib";
#elif defined(__APPLE__)
    const char *home = std::getenv("HOME");
    if (home && home[0]) return std::filesystem::path(home) / "Library" / "Caches" / "kinal" / "lsp-klib";
#else
    const char *xdg = std::getenv("XDG_CACHE_HOME");
    if (xdg && xdg[0]) return std::filesystem::path(xdg) / "kinal" / "lsp-klib";
    const char *home = std::getenv("HOME");
    if (home && home[0]) return std::filesystem::path(home) / ".cache" / "kinal" / "lsp-klib";
#endif
    return {};
}

// Convert a file:// URI under the klib cache to a virtual kinal-stdlib:/klib/... URI.
// Returns empty string if the URI is not under the klib cache.
std::string file_uri_to_klib_virtual_uri(const std::string &file_uri) {
    if (file_uri.rfind("file://", 0) != 0) return {};
    if (g_klib_cache_base.empty()) return {};

    std::string fpath = uri_to_path(file_uri);
    if (fpath.empty()) return {};

    // Normalize both paths for comparison (case-insensitive on Windows).
    std::filesystem::path fp = std::filesystem::path(fpath).lexically_normal();
    std::filesystem::path bp = g_klib_cache_base.lexically_normal();

    // Check if fpath is under g_klib_cache_base.
    auto fp_it = fp.begin();
    auto bp_it = bp.begin();
    for (; bp_it != bp.end() && fp_it != fp.end(); ++bp_it, ++fp_it) {
#if defined(_WIN32)
        std::string a = bp_it->string(), b = fp_it->string();
        std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        std::transform(b.begin(), b.end(), b.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (a != b) return {};
#else
        if (*bp_it != *fp_it) return {};
#endif
    }
    if (bp_it != bp.end()) return {};

    // Remaining segments: <category>/<pkg>/<ver>/<source_root>/<relative...>
    // e.g.: stdpkg / IO.Web / 1.0.0 / src / IO / Web / Server.kn
    std::vector<std::string> segs;
    for (; fp_it != fp.end(); ++fp_it) segs.push_back(fp_it->string());
    // Need at least: category, pkg, ver, source_root, and one relative segment.
    if (segs.size() < 5) return {};

    const std::string &cat = segs[0];       // stdpkg
    const std::string &pkg = segs[1];       // IO.Web
    const std::string &ver = segs[2];       // 1.0.0
    // segs[3] is source_root (e.g., "src") — skip it.
    // segs[4..] is the relative path.
    std::string rel;
    for (size_t i = 4; i < segs.size(); ++i) {
        if (!rel.empty()) rel.push_back('/');
        rel += segs[i];
    }
    if (rel.empty()) return {};

    return std::string("kinal-stdlib:/klib/") + cat + "/" + pkg + "/" + ver + "/" + rel;
}

// Convert a kinal-stdlib:/klib/... URI back to a cache file path.
std::string klib_virtual_uri_to_file_path(const std::string &virt_uri) {
    constexpr std::string_view klib_prefix = "kinal-stdlib:/klib/";
    if (virt_uri.rfind(klib_prefix, 0) != 0) return {};
    if (g_klib_cache_base.empty()) return {};
    std::string_view rest = std::string_view(virt_uri).substr(klib_prefix.size());
    // rest = <category>/<pkg>/<ver>/<relative>
    // Split: category
    auto slash1 = rest.find('/');
    if (slash1 == std::string_view::npos) return {};
    std::string cat(rest.substr(0, slash1));
    rest.remove_prefix(slash1 + 1);
    // pkg
    auto slash2 = rest.find('/');
    if (slash2 == std::string_view::npos) return {};
    std::string pkg(rest.substr(0, slash2));
    rest.remove_prefix(slash2 + 1);
    // ver
    auto slash3 = rest.find('/');
    if (slash3 == std::string_view::npos) return {};
    std::string ver(rest.substr(0, slash3));
    rest.remove_prefix(slash3 + 1);
    // relative
    std::string rel(rest);

    // Reconstruct: <cache_base>/<category>/<pkg>/<ver>/src/<rel>
    std::filesystem::path result = g_klib_cache_base / cat / pkg / ver / "src" / rel;
    return result.string();
}

static size_t utf8_seq_len(unsigned char c) {
    if ((c & 0x80u) == 0u) return 1;
    if ((c & 0xE0u) == 0xC0u) return 2;
    if ((c & 0xF0u) == 0xE0u) return 3;
    if ((c & 0xF8u) == 0xF0u) return 4;
    return 1;
}

static int utf16_units_for_utf8_span(std::string_view s) {
    int units = 0;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];
        size_t step = utf8_seq_len(c);
        if (i + step > s.size()) step = 1;
        units += (step == 4) ? 2 : 1;
        i += step;
    }
    return units;
}

static std::pair<int, int> utf16_span_from_utf8_line_span(const std::string &text,
                                                          const std::vector<size_t> &line_starts,
                                                          int line0,
                                                          int byte_col0,
                                                          int byte_len) {
    if (line0 < 0 || line0 >= (int)line_starts.size())
        return { std::max(0, byte_col0), std::max(1, byte_len) };

    size_t line_start = line_starts[(size_t)line0];
    size_t line_end = text.size();
    if ((size_t)(line0 + 1) < line_starts.size())
        line_end = line_starts[(size_t)(line0 + 1)] - 1;

    size_t start_off = line_start + (size_t)std::max(0, byte_col0);
    if (start_off > line_end) start_off = line_end;
    size_t end_off = start_off + (size_t)std::max(1, byte_len);
    if (end_off > line_end) end_off = line_end;
    if (end_off < start_off) end_off = start_off;

    std::string_view line_view(text.data() + line_start, line_end - line_start);
    size_t rel_start = start_off - line_start;
    size_t rel_end = end_off - line_start;
    if (rel_start > line_view.size()) rel_start = line_view.size();
    if (rel_end > line_view.size()) rel_end = line_view.size();
    if (rel_end < rel_start) rel_end = rel_start;

    int utf16_col = utf16_units_for_utf8_span(line_view.substr(0, rel_start));
    int utf16_end = utf16_units_for_utf8_span(line_view.substr(0, rel_end));
    return { utf16_col, std::max(1, utf16_end - utf16_col) };
}

std::string first_declared_unit_name(const UnitDocIndex &idx);

int qualified_segment_count(std::string_view q) {
    if (q.empty()) return 0;
    int count = 1;
    for (char c : q) {
        if (c == '.') ++count;
    }
    return count;
}

std::optional<Json> read_json_file(const std::filesystem::path &p) {
    auto txt = read_file_text(p);
    if (!txt) return std::nullopt;
    Json j;
    JsonParser parser(*txt);
    if (!parser.parse(&j)) return std::nullopt;
    return j;
}

bool path_is_within(const std::filesystem::path &base, const std::filesystem::path &candidate) {
    auto base_norm = base.lexically_normal();
    auto candidate_norm = candidate.lexically_normal();
    auto base_it = base_norm.begin();
    auto candidate_it = candidate_norm.begin();
    for (; base_it != base_norm.end() && candidate_it != candidate_norm.end(); ++base_it, ++candidate_it) {
        if (*base_it != *candidate_it) return false;
    }
    return base_it == base_norm.end();
}

std::optional<std::filesystem::path> safe_extract_path(const std::filesystem::path &out_dir, std::string_view rel) {
    if (rel.empty()) return std::nullopt;
    std::filesystem::path rel_path(rel);
    if (rel_path.is_absolute() || rel_path.has_root_name() || rel_path.has_root_directory()) return std::nullopt;
    rel_path = rel_path.lexically_normal();
    if (rel_path.empty()) return std::nullopt;
    for (const auto &part : rel_path) {
        if (part == "..") return std::nullopt;
    }
    std::filesystem::path out_path = (out_dir / rel_path).lexically_normal();
    if (!path_is_within(out_dir, out_path)) return std::nullopt;
    return out_path;
}

bool write_blob(std::ofstream &out, const std::vector<unsigned char> &data, size_t off, size_t len) {
    if (len == 0) return true;
    if (off > data.size() || len > data.size() - off) return false;
    if (len > (size_t)std::numeric_limits<std::streamsize>::max()) return false;
    out.write((const char *)(data.data() + off), (std::streamsize)len);
    return (bool)out;
}

bool klib_extract_to_dir_lsp(const std::filesystem::path &klib_path, const std::filesystem::path &out_dir) {
    std::error_code ec;
    if (!std::filesystem::exists(klib_path, ec) || ec) return false;

    std::ifstream in(klib_path, std::ios::binary);
    if (!in) return false;
    std::vector<unsigned char> data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (data.size() < 20) return false;
    if (!(data[0] == 'K' && data[1] == 'N' && data[2] == 'K' && data[3] == 'L' &&
          data[4] == 'I' && data[5] == 'B' && data[6] == '1')) {
        return false;
    }

    size_t off = 8;
    uint32_t version = 0;
    uint32_t compiler_len = 0;
    uint32_t manifest_len = 0;
    uint32_t source_count = 0;
    if (!read_u32_le_bytes(data, off, version) ||
        !read_u32_le_bytes(data, off, compiler_len) ||
        !read_u32_le_bytes(data, off, manifest_len) ||
        !read_u32_le_bytes(data, off, source_count)) {
        return false;
    }
    if (version != 1u) return false;
    if (off + compiler_len + manifest_len > data.size()) return false;

    std::filesystem::create_directories(out_dir, ec);
    if (ec) return false;

    std::filesystem::path manifest_path = out_dir / "package.knpkg.json";
    off += compiler_len;
    {
        std::ofstream mf(manifest_path, std::ios::binary);
        if (!mf) return false;
        if (!write_blob(mf, data, off, (size_t)manifest_len)) return false;
        off += manifest_len;
    }

    for (uint32_t i = 0; i < source_count; ++i) {
        uint32_t rel_len = 0;
        uint64_t src_size = 0;
        size_t src_size_sz = 0;
        if (!read_u32_le_bytes(data, off, rel_len) ||
            !read_u64_le_bytes(data, off, src_size)) {
            return false;
        }
        if (src_size > (uint64_t)std::numeric_limits<size_t>::max()) return false;
        src_size_sz = (size_t)src_size;
        if (off > data.size() || rel_len > data.size() - off || src_size_sz > data.size() - off - rel_len) return false;
        std::string rel((const char *)(data.data() + off), (size_t)rel_len);
        off += rel_len;
        std::replace(rel.begin(), rel.end(), '/', (char)std::filesystem::path::preferred_separator);
        auto out_path = safe_extract_path(out_dir, rel);
        if (!out_path) return false;
        std::filesystem::create_directories(out_path->parent_path(), ec);
        if (ec) return false;
        std::ofstream sf(*out_path, std::ios::binary);
        if (!sf) return false;
        if (!write_blob(sf, data, off, src_size_sz)) return false;
        off += src_size_sz;
    }

    return true;
}

void collect_package_source_roots(const std::filesystem::path &package_root,
                                  const std::filesystem::path &cache_root,
                                  std::vector<std::filesystem::path> &out,
                                  std::unordered_set<std::string> &seen) {
    std::error_code ec;
    if (package_root.empty()) return;
    if (!std::filesystem::exists(package_root, ec) || ec) return;
    if (!std::filesystem::is_directory(package_root, ec) || ec) return;

    auto add = [&](std::filesystem::path p, bool is_klib_cache = false) {
        if (p.empty()) return;
        p = p.lexically_normal();
        std::error_code add_ec;
        if (!std::filesystem::exists(p, add_ec) || add_ec) return;
        if (!std::filesystem::is_directory(p, add_ec) || add_ec) return;
        std::string key = normalize_fs_path(p.string());
        if (key.empty()) return;
        if (!seen.insert(key).second) return;
        out.push_back(std::move(p));
        if (is_klib_cache) g_klib_cache_source_roots.insert(key);
    };

    for (std::filesystem::recursive_directory_iterator it(package_root, ec), end; !ec && it != end; it.increment(ec)) {
        if (ec) break;
        if (!it->is_regular_file()) continue;
        const std::filesystem::path manifest_path = it->path();
        const std::string filename = manifest_path.filename().string();
        if (filename != "package.knpkg.json" && filename != "package.knpkg") continue;

        auto manifest = read_json_file(manifest_path);
        if (!manifest || manifest->kind != Json::Kind::Object) continue;
        std::string kind = manifest->find("kind") ? manifest->find("kind")->str() : "";
        if (!kind.empty() && kind != "package") continue;

        const std::filesystem::path package_dir = manifest_path.parent_path();
        const std::string pkg_name = manifest->find("name") ? manifest->find("name")->str(package_dir.filename().string()) : package_dir.filename().string();
        const std::string pkg_version = manifest->find("version") ? manifest->find("version")->str("0.0.0") : "0.0.0";
        const std::string source_root = manifest->find("source_root") ? manifest->find("source_root")->str("src") : "src";
        const std::string klib_rel = manifest->find("klib") ? manifest->find("klib")->str() : "";

        if (!klib_rel.empty()) {
            std::filesystem::path klib_path = (package_dir / klib_rel).lexically_normal();
            std::error_code path_ec;
            if (std::filesystem::exists(klib_path, path_ec) && !path_ec) {
                std::filesystem::path cache_dir = cache_root / sanitize_cache_segment(pkg_name) / sanitize_cache_segment(pkg_version);
                std::filesystem::path cached_manifest = cache_dir / "package.knpkg.json";
                bool need_extract = true;
                std::error_code time_ec;
                if (std::filesystem::exists(cached_manifest, time_ec) && !time_ec) {
                    auto cache_time = std::filesystem::last_write_time(cached_manifest, time_ec);
                    auto klib_time = std::filesystem::last_write_time(klib_path, time_ec);
                    if (!time_ec && cache_time >= klib_time) need_extract = false;
                }
                if (need_extract) {
                    std::filesystem::remove_all(cache_dir, ec);
                    ec.clear();
                    if (klib_extract_to_dir_lsp(klib_path, cache_dir)) {
                        add(cache_dir / source_root, true);
                        continue;
                    }
                } else {
                    add(cache_dir / source_root, true);
                    continue;
                }
            }
        }

        add(package_dir / source_root);
    }
}

std::optional<std::string> resolve_package_entry_uri_from_source_root(std::string_view unit,
                                                                      const std::filesystem::path &root) {
    if (unit.empty() || root.empty()) return std::nullopt;

    const std::filesystem::path package_dir = root.parent_path();
    static constexpr const char *kManifestNames[] = {"package.knpkg.json", "package.knpkg"};

    for (const char *manifest_name : kManifestNames) {
        std::filesystem::path manifest_path = package_dir / manifest_name;
        auto manifest = read_json_file(manifest_path);
        if (!manifest || manifest->kind != Json::Kind::Object) continue;

        std::string package_name = manifest->find("name") ? manifest->find("name")->str() : "";
        if (package_name != unit) continue;

        const Json *entry = manifest->find("entry");
        if (!entry || entry->kind != Json::Kind::String || entry->s.empty()) continue;

        std::filesystem::path entry_path = (package_dir / entry->s).lexically_normal();
        if (!path_is_within(package_dir, entry_path)) continue;

        std::error_code ec;
        if (!std::filesystem::exists(entry_path, ec) || ec) continue;
        if (!std::filesystem::is_regular_file(entry_path, ec) || ec) continue;
        return path_to_uri(entry_path.string());
    }

    return std::nullopt;
}

std::vector<std::filesystem::path> stdlib_source_roots() {
    std::vector<std::filesystem::path> out;
    std::unordered_set<std::string> seen;

    auto add = [&](std::filesystem::path p) {
        if (p.empty()) return;
        p = p.lexically_normal();
        std::error_code ec;
        if (!std::filesystem::exists(p, ec) || ec) return;
        if (!std::filesystem::is_directory(p, ec) || ec) return;
        std::string key = normalize_fs_path(p.string());
        if (key.empty()) return;
        if (!seen.insert(key).second) return;
        out.push_back(std::move(p));
    };

    std::error_code ec;
    std::filesystem::path cwd = std::filesystem::current_path(ec);
    std::filesystem::path cache_root = lsp_system_cache_dir();
    bool have_cwd = !ec;

    if (cache_root.empty()) {
        // Fallback to project-local cache if system cache is unavailable.
        if (have_cwd) cache_root = cwd / ".kinal-cache" / "lsp-klib";
        else if (!g_exe_dir.empty()) cache_root = std::filesystem::path(g_exe_dir) / ".kinal-cache" / "lsp-klib";
    }
    g_klib_cache_base = cache_root;

    if (have_cwd) {
        collect_package_source_roots(cwd / "stdpkg", cache_root / "stdpkg", out, seen);
        collect_package_source_roots(cwd / "kpkg", cache_root / "kpkg", out, seen);
        add(cwd / "stdlib" / "src");
    }

    if (!g_exe_dir.empty()) {
        std::filesystem::path exe_dir(g_exe_dir);
        collect_package_source_roots(exe_dir / "stdpkg", cache_root / "stdpkg", out, seen);
        collect_package_source_roots(exe_dir / "kpkg", cache_root / "kpkg", out, seen);
        add(exe_dir / "stdlib" / "src");
        if (exe_dir.has_parent_path()) {
            collect_package_source_roots(exe_dir.parent_path() / "stdpkg", cache_root / "stdpkg-parent", out, seen);
            collect_package_source_roots(exe_dir.parent_path() / "kpkg", cache_root / "kpkg-parent", out, seen);
            add(exe_dir.parent_path() / "stdlib" / "src");
        }
    }

    return out;
}

std::vector<std::filesystem::path> guess_project_roots_for_source(const std::string &path, const UnitDocIndex &idx) {
    std::vector<std::filesystem::path> out;
    std::unordered_set<std::string> seen;

    auto add = [&](std::filesystem::path p) {
        if (p.empty()) return;
        p = p.lexically_normal();
        std::string key = normalize_fs_path(p.string());
        if (key.empty()) return;
        if (!seen.insert(key).second) return;
        out.push_back(std::move(p));
    };

    for (const std::string &root : g_workspace_roots) add(root);
    for (const std::filesystem::path &root : stdlib_source_roots()) add(root);

    std::filesystem::path file_path(path);
    add(file_path.parent_path());

    std::string unit = first_declared_unit_name(idx);
    if (!unit.empty()) {
        std::filesystem::path root = file_path.parent_path();
        int steps = std::max(0, qualified_segment_count(unit) - 1);
        for (int i = 0; i < steps && !root.empty(); ++i) root = root.parent_path();
        add(root);
    }

    return out;
}

std::optional<std::string> resolve_unit_uri_from_roots(std::string_view unit, const std::vector<std::filesystem::path> &roots) {
    if (unit.empty()) return std::nullopt;

    std::filesystem::path rel;
    std::string seg;
    for (char c : unit) {
        if (c == '.') {
            if (!seg.empty()) {
                rel /= seg;
                seg.clear();
            }
            continue;
        }
        seg.push_back(c);
    }
    if (!seg.empty()) rel /= seg;
    if (rel.empty()) return std::nullopt;

    static constexpr const char *kExts[] = {".kn", ".kinal", ".fx"};
    std::error_code ec;
    for (const std::filesystem::path &root : roots) {
        if (root.empty()) continue;
        for (const char *ext : kExts) {
            std::filesystem::path candidate = root / rel;
            candidate += ext;
            if (!std::filesystem::exists(candidate, ec) || ec) {
                ec.clear();
                continue;
            }
            if (!std::filesystem::is_regular_file(candidate, ec) || ec) {
                ec.clear();
                continue;
            }
            return path_to_uri(candidate.string());
        }

        if (auto entry_uri = resolve_package_entry_uri_from_source_root(unit, root)) {
            return entry_uri;
        }
    }
    return std::nullopt;
}

std::vector<std::string> discover_declaring_unit_uris_from_roots(std::string_view unit,
                                                                 const std::vector<std::filesystem::path> &roots) {
    std::vector<std::string> out;
    if (unit.empty()) return out;

    std::unordered_set<std::string> seen;
    bool touched = false;
    std::error_code ec;

    auto matches_declaring_unit = [&](const UnitDocIndex &idx) {
        for (const UnitIndexItem &it : idx.items) {
            if (it.decl && it.unit == unit) return true;
        }
        return false;
    };

    for (const std::filesystem::path &root : roots) {
        if (root.empty()) continue;
        if (!std::filesystem::exists(root, ec) || ec) {
            ec.clear();
            continue;
        }

        std::vector<std::string> root_hits;
        std::filesystem::recursive_directory_iterator it(root, std::filesystem::directory_options::skip_permission_denied, ec);
        std::filesystem::recursive_directory_iterator end;
        for (; it != end && !ec; it.increment(ec)) {
            const std::filesystem::directory_entry &de = *it;
            const std::filesystem::path &pp = de.path();

            if (de.is_directory(ec)) {
                if (should_skip_index_dir(pp.filename().string())) it.disable_recursion_pending();
                continue;
            }
            if (!de.is_regular_file(ec)) continue;
            if (!is_indexable_source_path(pp)) continue;

            std::string dep_uri = canonical_file_uri(path_to_uri(pp.string()));
            if (dep_uri.empty() || !seen.insert(dep_uri).second) continue;

            UnitDocIndex idx;
            auto known = g_unit_docs.find(dep_uri);
            if (known != g_unit_docs.end()) {
                idx = known->second;
            } else {
                auto txt = read_file_text(pp);
                if (!txt) continue;
                idx = index_units_from_tokens(lex_all(*txt));
                g_unit_docs[dep_uri] = idx;
                g_decl_doc_cache.erase(dep_uri);
                touched = true;
            }

            if (!matches_declaring_unit(idx)) continue;
            root_hits.push_back(dep_uri);
        }
        ec.clear();

        if (!root_hits.empty()) {
            out.insert(out.end(), root_hits.begin(), root_hits.end());
            break;
        }
    }

    if (touched) rebuild_known_units();
    return out;
}

struct ProjectSource {
    std::string uri;
    std::string path;
    std::string text;
    UnitDocIndex idx;
    bool current = false;
};

void merge_program_lists(const MetaList &src_metas, const FuncList &src_funcs, const ImportList &src_imports,
                         const ClassList &src_classes, const InterfaceList &src_interfaces,
                         const StructList &src_structs, const EnumList &src_enums, const StmtList &src_globals,
                         MetaList *dst_metas, FuncList *dst_funcs, ImportList *dst_imports, ClassList *dst_classes,
                         InterfaceList *dst_interfaces, StructList *dst_structs, EnumList *dst_enums, StmtList *dst_globals) {
    for (int i = 0; i < src_metas.count; ++i) metalist_push(dst_metas, src_metas.items[i]);
    for (int i = 0; i < src_funcs.count; ++i) funclist_push(dst_funcs, src_funcs.items[i]);
    for (int i = 0; i < src_imports.count; ++i) importlist_push(dst_imports, src_imports.items[i]);
    for (int i = 0; i < src_classes.count; ++i) classlist_push(dst_classes, src_classes.items[i]);
    for (int i = 0; i < src_interfaces.count; ++i) interfacelist_push(dst_interfaces, src_interfaces.items[i]);
    for (int i = 0; i < src_structs.count; ++i) structlist_push(dst_structs, src_structs.items[i]);
    for (int i = 0; i < src_enums.count; ++i) enumlist_push(dst_enums, src_enums.items[i]);
    for (int i = 0; i < src_globals.count; ++i) stmtlist_push(dst_globals, src_globals.items[i]);
}

void index_workspace(void) {
    g_unit_docs.clear();
    g_known_units.clear();
    g_decl_doc_cache.clear();

    struct WsFile {
        std::string uri;
        std::vector<Tok> toks;
    };
    std::vector<WsFile> files;
    std::unordered_set<std::string> seen_uris;

    auto add_root_files = [&](const std::filesystem::path &rp) {
        std::error_code ec;
        if (rp.empty()) return;
        if (!std::filesystem::exists(rp, ec) || ec) return;

        std::filesystem::recursive_directory_iterator it(rp, std::filesystem::directory_options::skip_permission_denied, ec);
        std::filesystem::recursive_directory_iterator end;
        for (; it != end && !ec; it.increment(ec)) {
            const std::filesystem::directory_entry &de = *it;
            const auto &pp = de.path();
            if (de.is_directory(ec)) {
                std::string dn = pp.filename().string();
                if (should_skip_index_dir(dn)) it.disable_recursion_pending();
                continue;
            }
            if (!de.is_regular_file(ec)) continue;
            if (!is_indexable_source_path(pp)) continue;

            auto txt = read_file_text(pp);
            if (!txt) continue;
            WsFile f;
            f.uri = path_to_uri(pp.string());
            if (!seen_uris.insert(canonical_file_uri(f.uri)).second) continue;
            f.toks = lex_all(*txt);
            files.push_back(std::move(f));
        }
    };

    for (const std::string &root : g_workspace_roots) {
        if (root.empty()) continue;
        add_root_files(std::filesystem::path(root));
    }
    for (const std::filesystem::path &root : stdlib_source_roots()) add_root_files(root);

    // Pass 1: Unit decls (seed g_known_units so Get can disambiguate module vs module.member).
    for (const WsFile &f : files) {
        UnitDocIndex idx;
        idx.items.clear();
        int brace_depth = 0;
        for (int i = 0; i < (int)f.toks.size(); ++i) {
            TokenType tt = f.toks[(size_t)i].type;
            if (tt == TOK_LBRACE) brace_depth++;
            if (tt == TOK_RBRACE) brace_depth = std::max(0, brace_depth - 1);
            if (brace_depth != 0) continue;
            if (tt != TOK_UNIT) continue;
            int j = i + 1;
            std::string unit;
            Range span;
            if (!parse_qualified_name_span(f.toks, j, unit, span)) continue;
            idx.items.push_back(UnitIndexItem{unit, span, true});
            g_known_units.insert(unit);
        }
        g_unit_docs[f.uri] = std::move(idx);
    }

    // Pass 2: Get refs (now unit_decl_exists works for workspace units).
    for (const WsFile &f : files) {
        UnitDocIndex &idx = g_unit_docs[f.uri];
        int brace_depth = 0;
        for (int i = 0; i < (int)f.toks.size(); ++i) {
            TokenType tt = f.toks[(size_t)i].type;
            if (tt == TOK_LBRACE) brace_depth++;
            if (tt == TOK_RBRACE) brace_depth = std::max(0, brace_depth - 1);
            if (brace_depth != 0) continue;
            if (tt != TOK_GET) continue;

            int j = i + 1;
            std::string first;
            Range first_span;
            if (!parse_qualified_name_span(f.toks, j, first, first_span)) continue;
            bool first_has_list = (j < (int)f.toks.size() && f.toks[(size_t)j].type == TOK_LBRACE);

            if (j < (int)f.toks.size() && f.toks[(size_t)j].type == TOK_BY) {
                ++j;
                std::string target;
                Range target_span;
                if (!parse_qualified_name_span(f.toks, j, target, target_span)) continue;
                bool target_has_list = (j < (int)f.toks.size() && f.toks[(size_t)j].type == TOK_LBRACE);
                std::string module = module_from_get_qualified(target, target_has_list);
                idx.items.push_back(UnitIndexItem{module, target_span, false});
                continue;
            }

            std::string module = module_from_get_qualified(first, first_has_list);
            idx.items.push_back(UnitIndexItem{module, first_span, false});
        }
    }
}

void set_workspace_roots_from_initialize(const Json *params) {
    g_workspace_roots.clear();
    if (!params || params->kind != Json::Kind::Object) return;

    auto add_root = [&](const std::string &u) {
        if (u.empty()) return;
        std::string p;
        if (u.rfind("file://", 0) == 0)
            p = uri_to_path(u);
        else
            p = u;
        if (p.empty()) return;
        for (const std::string &e : g_workspace_roots) {
            if (e == p) return;
        }
        g_workspace_roots.push_back(p);
    };

    if (const Json *wf = params->find("workspaceFolders"); wf && wf->kind == Json::Kind::Array) {
        for (const Json &x : wf->a) {
            if (x.kind != Json::Kind::Object) continue;
            const Json *uv = x.find("uri");
            if (!uv || uv->kind != Json::Kind::String) continue;
            add_root(uv->s);
        }
    }

    if (g_workspace_roots.empty()) {
        if (const Json *rv = params->find("rootUri"); rv && rv->kind == Json::Kind::String) add_root(rv->s);
        if (const Json *rp = params->find("rootPath"); rp && rp->kind == Json::Kind::String) add_root(rp->s);
    }
}

bool is_decl_modifier(TokenType t) {
    switch (t) {
    case TOK_PUBLIC:
    case TOK_PRIVATE:
    case TOK_PROTECTED:
    case TOK_INTERNAL:
    case TOK_STATIC:
    case TOK_ASYNC:
    case TOK_EXTERN:
    case TOK_SAFE:
    case TOK_TRUSTED:
    case TOK_UNSAFE:
    case TOK_VIRTUAL:
    case TOK_OVERRIDE:
    case TOK_ABSTRACT:
    case TOK_SEALED:
        return true;
    default:
        return false;
    }
}

bool kind_is_type(SymKind k) {
    return k == SymKind::Class || k == SymKind::Struct || k == SymKind::Interface || k == SymKind::Enum;
}

bool kind_is_callable(SymKind k) {
    return k == SymKind::Function || k == SymKind::Method;
}

int find_matching(const std::vector<Tok> &toks, int open_idx, TokenType open_t, TokenType close_t) {
    if (open_idx < 0 || open_idx >= (int)toks.size()) return -1;
    if (toks[(size_t)open_idx].type != open_t) return -1;
    int depth = 0;
    for (int i = open_idx; i < (int)toks.size(); ++i) {
        TokenType t = toks[(size_t)i].type;
        if (t == open_t) depth++;
        if (t == close_t) {
            depth--;
            if (depth == 0) return i;
        }
    }
    return -1;
}

struct ParsedFunctionDecl {
    std::string display_name;
    int name_idx = -1;
    int lpar_idx = -1;
    int rpar_idx = -1;
    int end_idx = -1;
    bool has_body = false;
    bool is_static = false;
    bool is_constructor = false;
};

struct ParsedPropertyDecl {
    int name_idx = -1;
    int lbrace_idx = -1;
    int rbrace_idx = -1;
    bool is_static = false;
};

bool parse_function_decl(const std::vector<Tok> &toks, int fn_idx, ParsedFunctionDecl &out) {
    if (fn_idx < 0 || fn_idx >= (int)toks.size()) return false;
    if (toks[(size_t)fn_idx].type != TOK_FUNCTION) return false;

    int name_idx = -1;
    int lpar_idx = -1;
    bool is_static = false;
    for (int i = fn_idx + 1; i < (int)toks.size(); ++i) {
        TokenType t = toks[(size_t)i].type;
        if (t == TOK_STATIC) is_static = true;
        if (t == TOK_LPAREN) {
            lpar_idx = i;
            break;
        }
        if (t == TOK_ID) name_idx = i;
        if (t == TOK_SEMI || t == TOK_LBRACE || t == TOK_RBRACE) break;
    }
    if (name_idx < 0 || lpar_idx < 0) return false;

    int rpar_idx = find_matching(toks, lpar_idx, TOK_LPAREN, TOK_RPAREN);
    if (rpar_idx < 0) return false;

    bool has_body = false;
    int end_idx = rpar_idx;
    for (int i = rpar_idx + 1; i < (int)toks.size(); ++i) {
        TokenType t = toks[(size_t)i].type;
        if (t == TOK_LBRACE) {
            has_body = true;
            end_idx = i;
            break;
        }
        if (t == TOK_SEMI) {
            end_idx = i;
            break;
        }
    }

    out.name_idx = name_idx;
    out.lpar_idx = lpar_idx;
    out.rpar_idx = rpar_idx;
    out.end_idx = end_idx;
    out.has_body = has_body;
    out.is_static = is_static;
    out.is_constructor = false;
    out.display_name = toks[(size_t)name_idx].text;
    return true;
}

bool parse_constructor_decl(const std::vector<Tok> &toks, int ctor_idx, ParsedFunctionDecl &out) {
    if (ctor_idx < 0 || ctor_idx >= (int)toks.size()) return false;
    if (toks[(size_t)ctor_idx].type != TOK_CONSTRUCTOR) return false;

    int lpar_idx = -1;
    for (int i = ctor_idx + 1; i < (int)toks.size(); ++i) {
        TokenType t = toks[(size_t)i].type;
        if (t == TOK_LPAREN) {
            lpar_idx = i;
            break;
        }
        if (t == TOK_SEMI || t == TOK_LBRACE || t == TOK_RBRACE) break;
    }
    if (lpar_idx < 0) return false;

    int rpar_idx = find_matching(toks, lpar_idx, TOK_LPAREN, TOK_RPAREN);
    if (rpar_idx < 0) return false;

    bool has_body = false;
    int end_idx = rpar_idx;
    for (int i = rpar_idx + 1; i < (int)toks.size(); ++i) {
        TokenType t = toks[(size_t)i].type;
        if (t == TOK_LBRACE) {
            has_body = true;
            end_idx = i;
            break;
        }
        if (t == TOK_SEMI) {
            end_idx = i;
            break;
        }
    }

    out.name_idx = ctor_idx;
    out.lpar_idx = lpar_idx;
    out.rpar_idx = rpar_idx;
    out.end_idx = end_idx;
    out.has_body = has_body;
    out.is_static = false;
    out.is_constructor = true;
    out.display_name = "Constructor";
    return true;
}

bool parse_callable_decl(const std::vector<Tok> &toks, int idx, ParsedFunctionDecl &out) {
    if (idx < 0 || idx >= (int)toks.size()) return false;
    if (toks[(size_t)idx].type == TOK_FUNCTION) return parse_function_decl(toks, idx, out);
    if (toks[(size_t)idx].type == TOK_CONSTRUCTOR) return parse_constructor_decl(toks, idx, out);
    return false;
}

bool parse_property_decl(const std::vector<Tok> &toks, int prop_idx, ParsedPropertyDecl &out) {
    if (prop_idx < 0 || prop_idx >= (int)toks.size()) return false;
    if (toks[(size_t)prop_idx].type != TOK_PROPERTY) return false;

    int name_idx = -1;
    int lbrace_idx = -1;
    bool is_static = false;
    for (int i = prop_idx + 1; i < (int)toks.size(); ++i) {
        TokenType t = toks[(size_t)i].type;
        if (t == TOK_STATIC) is_static = true;
        if (t == TOK_LBRACE) {
            lbrace_idx = i;
            break;
        }
        if (t == TOK_ID) name_idx = i;
        if (t == TOK_SEMI || t == TOK_RBRACE) break;
    }
    if (name_idx < 0 || lbrace_idx < 0) return false;

    int rbrace_idx = find_matching(toks, lbrace_idx, TOK_LBRACE, TOK_RBRACE);
    if (rbrace_idx < 0) return false;

    out.name_idx = name_idx;
    out.lbrace_idx = lbrace_idx;
    out.rbrace_idx = rbrace_idx;
    out.is_static = is_static;
    return true;
}

bool has_static_decl_modifier_before(const std::vector<Tok> &toks, int idx) {
    for (int i = idx - 1; i >= 0; --i) {
        if (!is_decl_modifier(toks[(size_t)i].type)) break;
        if (toks[(size_t)i].type == TOK_STATIC) return true;
    }
    return false;
}

int find_decl_name_in_segment(const std::vector<Tok> &toks, int start, int end_exclusive) {
    if (start < 0) start = 0;
    if (end_exclusive > (int)toks.size()) end_exclusive = (int)toks.size();
    if (start >= end_exclusive) return -1;

    int decl_end = end_exclusive;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    int angle_depth = 0;
    for (int i = start; i < end_exclusive; ++i) {
        TokenType t = toks[(size_t)i].type;
        if (t == TOK_ASSIGN && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 && angle_depth == 0) {
            decl_end = i;
            break;
        }
        if (t == TOK_LPAREN) paren_depth++;
        else if (t == TOK_RPAREN) paren_depth = std::max(0, paren_depth - 1);
        else if (t == TOK_LBRACKET) bracket_depth++;
        else if (t == TOK_RBRACKET) bracket_depth = std::max(0, bracket_depth - 1);
        else if (t == TOK_LBRACE) brace_depth++;
        else if (t == TOK_RBRACE) brace_depth = std::max(0, brace_depth - 1);
        else if (t == TOK_LT) angle_depth++;
        else if (t == TOK_GT) angle_depth = std::max(0, angle_depth - 1);
    }

    for (int i = decl_end - 1; i >= start; --i) {
        if (toks[(size_t)i].type != TOK_ID) continue;
        if (i > start && toks[(size_t)(i - 1)].type == TOK_DOT) continue;
        return i;
    }
    return -1;
}

std::string decl_type_text_from_segment(const std::vector<Tok> &toks, int start, int name_idx, int end_exclusive) {
    std::string ty = join_tok_text(toks, start, name_idx - 1);
    int i = name_idx + 1;
    while (i + 1 < end_exclusive && toks[(size_t)i].type == TOK_LBRACKET && toks[(size_t)(i + 1)].type == TOK_RBRACKET) {
        ty += "[]";
        i += 2;
    }
    return ty;
}

std::vector<int> collect_param_name_indices(const std::vector<Tok> &toks, int lpar_idx, int rpar_idx) {
    std::vector<int> out;
    if (lpar_idx < 0 || rpar_idx <= lpar_idx) return out;

    int seg_start = lpar_idx + 1;
    int paren_depth = 0;
    int bracket_depth = 0;
    int brace_depth = 0;
    int angle_depth = 0;
    for (int i = lpar_idx + 1; i < rpar_idx; ++i) {
        TokenType t = toks[(size_t)i].type;
        if (t == TOK_COMMA && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 && angle_depth == 0) {
            int name_idx = find_decl_name_in_segment(toks, seg_start, i);
            if (name_idx >= 0) out.push_back(name_idx);
            seg_start = i + 1;
            continue;
        }
        if (t == TOK_LPAREN) paren_depth++;
        else if (t == TOK_RPAREN) paren_depth = std::max(0, paren_depth - 1);
        else if (t == TOK_LBRACKET) bracket_depth++;
        else if (t == TOK_RBRACKET) bracket_depth = std::max(0, bracket_depth - 1);
        else if (t == TOK_LBRACE) brace_depth++;
        else if (t == TOK_RBRACE) brace_depth = std::max(0, brace_depth - 1);
        else if (t == TOK_LT) angle_depth++;
        else if (t == TOK_GT) angle_depth = std::max(0, angle_depth - 1);
    }
    int name_idx = find_decl_name_in_segment(toks, seg_start, rpar_idx);
    if (name_idx >= 0) out.push_back(name_idx);
    return out;
}

enum class ScopeKind {
    Global,
    Block,
    Function,
    Class,
    Struct,
    Interface,
    Enum
};

struct ScopeFrame {
    ScopeKind kind = ScopeKind::Block;
    int owner_sym_idx = -1;
    std::unordered_map<std::string, int> names;
    int64_t next_enum_value = 0;
    std::string enum_underlying;
    bool is_static_owner = false;
};

struct PendingScope {
    ScopeKind kind = ScopeKind::Block;
    int owner_sym_idx = -1;
    std::vector<int> seeded_sym_idxs;
    bool is_static_owner = false;
};

bool is_member_owner_scope(ScopeKind k) {
    return k == ScopeKind::Class || k == ScopeKind::Struct || k == ScopeKind::Interface;
}

std::string owner_name_from_scopes(const std::vector<ScopeFrame> &scopes, const Analysis &an) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        if (!is_member_owner_scope(scopes[(size_t)i].kind)) continue;
        int owner = scopes[(size_t)i].owner_sym_idx;
        if (owner >= 0 && owner < (int)an.syms.size()) return an.syms[(size_t)owner].name;
    }
    return {};
}

int add_sym(Analysis &an, int &next_id, SymKind kind, const Tok &tok, std::string key, bool is_static = false, std::string container = {}) {
    Symbol s;
    s.id = next_id++;
    s.kind = kind;
    s.name = tok.text;
    s.container = std::move(container);
    s.key = std::move(key);
    s.decl = mk_range(tok.line, tok.col, tok.len);
    s.decl_tok_idx = tok.idx;
    s.is_static = is_static;
    int sym_idx = (int)an.syms.size();
    an.syms.push_back(std::move(s));

    Occ occ;
    occ.sym_id = an.syms.back().id;
    occ.key = an.syms.back().key;
    occ.range = an.syms.back().decl;
    occ.decl = true;
    occ.tok_idx = tok.idx;
    an.occs.push_back(std::move(occ));
    return sym_idx;
}

int pick_preferred_symbol(const std::vector<int> &cands, const Analysis &an, bool prefer_type, bool prefer_callable) {
    if (cands.empty()) return -1;
    if (prefer_type) {
        for (int idx : cands) {
            if (idx >= 0 && idx < (int)an.syms.size() && kind_is_type(an.syms[(size_t)idx].kind)) return idx;
        }
    }
    if (prefer_callable) {
        for (int idx : cands) {
            if (idx >= 0 && idx < (int)an.syms.size() && kind_is_callable(an.syms[(size_t)idx].kind)) return idx;
        }
    }
    return cands.back();
}

int pick_current_owner_member_symbol(const Analysis &an,
                                     const std::vector<ScopeFrame> &scopes,
                                     const std::unordered_map<std::string, std::vector<int>> &members_by_name,
                                     const std::string &name,
                                     bool prefer_type,
                                     bool prefer_callable) {
    auto mit = members_by_name.find(name);
    if (mit == members_by_name.end()) return -1;

    std::string owner = owner_name_from_scopes(scopes, an);
    if (owner.empty()) return -1;

    bool static_only = false;
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        if (scopes[(size_t)i].kind != ScopeKind::Function) continue;
        int sym_idx = scopes[(size_t)i].owner_sym_idx;
        if (sym_idx >= 0 && sym_idx < (int)an.syms.size()) {
            const Symbol &s = an.syms[(size_t)sym_idx];
            if (s.kind == SymKind::Method && s.is_static)
                static_only = true;
        }
        break;
    }

    std::vector<int> filtered;
    for (int idx : mit->second) {
        if (idx < 0 || idx >= (int)an.syms.size()) continue;
        const Symbol &s = an.syms[(size_t)idx];
        if (s.container != owner) continue;
        if (static_only && !s.is_static) continue;
        filtered.push_back(idx);
    }
    return pick_preferred_symbol(filtered, an, prefer_type, prefer_callable);
}

int resolve_symbol(const Analysis &an,
                   const std::vector<ScopeFrame> &scopes,
                   const std::unordered_map<std::string, std::vector<int>> &globals_by_name,
                   const std::unordered_map<std::string, std::vector<int>> &members_by_name,
                   const std::string &name,
                   bool member_ctx,
                   bool prefer_type,
                   bool prefer_callable) {
    for (int i = (int)scopes.size() - 1; i >= 0; --i) {
        auto it = scopes[(size_t)i].names.find(name);
        if (it != scopes[(size_t)i].names.end()) return it->second;
    }

    if (!member_ctx) {
        int owner_member = pick_current_owner_member_symbol(an, scopes, members_by_name, name, prefer_type, prefer_callable);
        if (owner_member >= 0) return owner_member;
    }

    if (member_ctx) {
        auto mit = members_by_name.find(name);
        if (mit != members_by_name.end()) return pick_preferred_symbol(mit->second, an, prefer_type, prefer_callable);
    }

    auto git = globals_by_name.find(name);
    if (git != globals_by_name.end()) return pick_preferred_symbol(git->second, an, prefer_type, prefer_callable);

    auto mit = members_by_name.find(name);
    if (mit != members_by_name.end()) return pick_preferred_symbol(mit->second, an, prefer_type, prefer_callable);

    return -1;
}

Analysis build_symbols(const std::string &path, const std::string &uri, const std::string &text) {
    (void)path;
    Analysis an;
    an.toks = lex_all(text);
    std::string local_prefix = uri.empty() ? "<memory>" : uri;
    parse_imports_from_tokens(an);

    int next_id = 1;
    std::unordered_map<std::string, std::vector<int>> globals_by_name;
    std::unordered_map<std::string, std::vector<int>> members_by_name;
    std::vector<int> decl_sym_idx_by_tok(an.toks.size(), -1);

    auto mark_decl = [&](int tok_idx, int sym_idx) {
        if (tok_idx < 0 || tok_idx >= (int)decl_sym_idx_by_tok.size()) return;
        decl_sym_idx_by_tok[(size_t)tok_idx] = sym_idx;
    };

    auto add_global_decl = [&](SymKind k, int tok_pos, bool is_static = false, const std::string &container = std::string()) -> int {
        if (tok_pos < 0 || tok_pos >= (int)an.toks.size()) return -1;
        const Tok &id = an.toks[(size_t)tok_pos];
        if (decl_sym_idx_by_tok[(size_t)id.idx] >= 0) return decl_sym_idx_by_tok[(size_t)id.idx];
        std::string key = std::string("g|") + id.text;
        if (!container.empty()) key = std::string("g|") + container + "." + id.text;
        int sym_idx = add_sym(an, next_id, k, id, key, is_static, container);
        mark_decl(id.idx, sym_idx);
        if (k == SymKind::Field || k == SymKind::Property || k == SymKind::Method || k == SymKind::EnumMember) {
            members_by_name[id.text].push_back(sym_idx);
        } else {
            globals_by_name[id.text].push_back(sym_idx);
        }
        return sym_idx;
    };

    // Pass 1: predeclare top-level types/functions for forward references.
    int top_level_depth = 0;
    for (int i = 0; i < (int)an.toks.size(); ++i) {
        const Tok &t = an.toks[(size_t)i];
        if (t.type == TOK_RBRACE) top_level_depth = std::max(0, top_level_depth - 1);

        if (top_level_depth == 0) {
            if (t.type == TOK_CLASS || t.type == TOK_STRUCT || t.type == TOK_INTERFACE || t.type == TOK_ENUM) {
                if (i + 1 < (int)an.toks.size() && an.toks[(size_t)(i + 1)].type == TOK_ID) {
                    SymKind k = SymKind::Class;
                    bool is_static_owner = (t.type == TOK_CLASS) && has_static_decl_modifier_before(an.toks, i);
                    if (t.type == TOK_STRUCT) k = SymKind::Struct;
                    else if (t.type == TOK_INTERFACE) k = SymKind::Interface;
                    else if (t.type == TOK_ENUM) k = SymKind::Enum;
                    add_global_decl(k, i + 1, is_static_owner);
                }
            } else if (t.type == TOK_FUNCTION) {
                ParsedFunctionDecl fn;
                if (parse_function_decl(an.toks, i, fn)) {
                    add_global_decl(SymKind::Function, fn.name_idx, fn.is_static);
                }
            }
        }

        if (t.type == TOK_LBRACE) top_level_depth++;
    }

    std::vector<ScopeFrame> scopes;
    scopes.push_back(ScopeFrame{ScopeKind::Global, -1, {}, 0, {}, false});
    std::vector<PendingScope> pending_scopes;

    for (int i = 0; i < (int)an.toks.size(); ++i) {
        const Tok &t = an.toks[(size_t)i];

        if (t.type == TOK_LBRACE) {
            ScopeFrame sc;
            if (!pending_scopes.empty()) {
                PendingScope p = std::move(pending_scopes.back());
                pending_scopes.pop_back();
                sc.kind = p.kind;
                sc.owner_sym_idx = p.owner_sym_idx;
                sc.is_static_owner = p.is_static_owner;
                if (sc.kind == ScopeKind::Enum && sc.owner_sym_idx >= 0 && sc.owner_sym_idx < (int)an.syms.size()) {
                    sc.enum_underlying = an.syms[(size_t)sc.owner_sym_idx].type_text;
                    sc.next_enum_value = 0;
                }
                for (int sym_idx : p.seeded_sym_idxs) {
                    if (sym_idx < 0 || sym_idx >= (int)an.syms.size()) continue;
                    sc.names[an.syms[(size_t)sym_idx].name] = sym_idx;
                }
            } else {
                sc.kind = ScopeKind::Block;
                sc.owner_sym_idx = -1;
            }
            scopes.push_back(std::move(sc));
            continue;
        }
        if (t.type == TOK_RBRACE) {
            if (scopes.size() > 1) scopes.pop_back();
            continue;
        }

        if (t.type == TOK_CLASS || t.type == TOK_STRUCT || t.type == TOK_INTERFACE || t.type == TOK_ENUM) {
            if (i + 1 < (int)an.toks.size() && an.toks[(size_t)(i + 1)].type == TOK_ID) {
                const Tok &id = an.toks[(size_t)(i + 1)];
                SymKind sk = SymKind::Class;
                ScopeKind scope_kind = ScopeKind::Class;
                if (t.type == TOK_STRUCT) {
                    sk = SymKind::Struct;
                    scope_kind = ScopeKind::Struct;
                } else if (t.type == TOK_INTERFACE) {
                    sk = SymKind::Interface;
                    scope_kind = ScopeKind::Interface;
                } else if (t.type == TOK_ENUM) {
                    sk = SymKind::Enum;
                    scope_kind = ScopeKind::Enum;
                }
                bool is_static_owner = (t.type == TOK_CLASS) && has_static_decl_modifier_before(an.toks, i);

                int sym_idx = decl_sym_idx_by_tok[(size_t)id.idx];
                if (sym_idx < 0) {
                    std::string owner = owner_name_from_scopes(scopes, an);
                    std::string key = owner.empty() ? std::string("g|") + id.text : std::string("g|") + owner + "." + id.text;
                    sym_idx = add_sym(an, next_id, sk, id, key, is_static_owner, owner);
                    mark_decl(id.idx, sym_idx);
                    globals_by_name[id.text].push_back(sym_idx);
                }
                an.syms[(size_t)sym_idx].is_static = is_static_owner;
                if (sk == SymKind::Enum) {
                    std::string underlying = "int";
                    int j = i + 2;
                    if (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_BY) {
                        ++j;
                        int ts = j;
                        while (j < (int)an.toks.size() && an.toks[(size_t)j].type != TOK_LBRACE) ++j;
                        if (j > ts) underlying = join_tok_text(an.toks, ts, j - 1);
                    }
                    an.syms[(size_t)sym_idx].type_text = underlying;
                }
                pending_scopes.push_back(PendingScope{scope_kind, sym_idx, {}, is_static_owner});
            }
            continue;
        }

        if (t.type == TOK_FUNCTION || t.type == TOK_CONSTRUCTOR) {
            ParsedFunctionDecl fn;
            if (!parse_callable_decl(an.toks, i, fn)) continue;
            const Tok &nm = an.toks[(size_t)fn.name_idx];

            std::string owner = owner_name_from_scopes(scopes, an);
            bool in_member_owner = !owner.empty() && is_member_owner_scope(scopes.back().kind);
            SymKind kind = in_member_owner ? SymKind::Method : SymKind::Function;
            if (in_member_owner && scopes.back().is_static_owner) fn.is_static = true;

            int sym_idx = decl_sym_idx_by_tok[(size_t)nm.idx];
            if (sym_idx < 0) {
                std::string key;
                if (kind == SymKind::Method) key = std::string("m|") + owner + "." + nm.text;
                else key = std::string("g|") + nm.text;
                sym_idx = add_sym(an, next_id, kind, nm, key, fn.is_static, owner);
                mark_decl(nm.idx, sym_idx);
                if (kind == SymKind::Method) members_by_name[nm.text].push_back(sym_idx);
                else globals_by_name[nm.text].push_back(sym_idx);
            }

            an.syms[(size_t)sym_idx].container = owner;
            an.syms[(size_t)sym_idx].is_static = fn.is_static;
            an.syms[(size_t)sym_idx].name = fn.display_name.empty() ? nm.text : fn.display_name;
            an.syms[(size_t)sym_idx].ret_text = fn.is_constructor ? "void" : join_tok_text(an.toks, i + 1, fn.name_idx - 1);
            an.syms[(size_t)sym_idx].params.clear();

            std::vector<int> params;
            for (int pidx : collect_param_name_indices(an.toks, fn.lpar_idx, fn.rpar_idx)) {
                const Tok &pt = an.toks[(size_t)pidx];
                int ts = fn.lpar_idx + 1;
                int te = fn.rpar_idx;
                int paren_depth = 0;
                int bracket_depth = 0;
                int brace_depth = 0;
                int angle_depth = 0;
                for (int k = fn.lpar_idx + 1; k < fn.rpar_idx; ++k) {
                    TokenType kt = an.toks[(size_t)k].type;
                    if (k < pidx && kt == TOK_COMMA && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 && angle_depth == 0)
                        ts = k + 1;
                    if (k > pidx && kt == TOK_COMMA && paren_depth == 0 && bracket_depth == 0 && brace_depth == 0 && angle_depth == 0) {
                        te = k;
                        break;
                    }
                    if (kt == TOK_LPAREN) paren_depth++;
                    else if (kt == TOK_RPAREN) paren_depth = std::max(0, paren_depth - 1);
                    else if (kt == TOK_LBRACKET) bracket_depth++;
                    else if (kt == TOK_RBRACKET) bracket_depth = std::max(0, bracket_depth - 1);
                    else if (kt == TOK_LBRACE) brace_depth++;
                    else if (kt == TOK_RBRACE) brace_depth = std::max(0, brace_depth - 1);
                    else if (kt == TOK_LT) angle_depth++;
                    else if (kt == TOK_GT) angle_depth = std::max(0, angle_depth - 1);
                }
                std::string ptype = decl_type_text_from_segment(an.toks, ts, pidx, te);

                if (decl_sym_idx_by_tok[(size_t)pt.idx] >= 0) {
                    int p_sym_idx = decl_sym_idx_by_tok[(size_t)pt.idx];
                    if (p_sym_idx >= 0 && p_sym_idx < (int)an.syms.size()) {
                        if (an.syms[(size_t)p_sym_idx].type_text.empty())
                            an.syms[(size_t)p_sym_idx].type_text = ptype;
                    }
                    params.push_back(p_sym_idx);
                    an.syms[(size_t)sym_idx].params.emplace_back(ptype, pt.text);
                    continue;
                }
                std::string pkey = std::string("l|") + local_prefix + "|" + std::to_string(nm.idx) + "|p|" + pt.text + "|" + std::to_string(pt.idx);
                int p_sym_idx = add_sym(an, next_id, SymKind::Parameter, pt, pkey, false, nm.text);
                an.syms[(size_t)p_sym_idx].type_text = ptype;
                mark_decl(pt.idx, p_sym_idx);
                params.push_back(p_sym_idx);
                an.syms[(size_t)sym_idx].params.emplace_back(ptype, pt.text);
            }

            if (fn.has_body) pending_scopes.push_back(PendingScope{ScopeKind::Function, sym_idx, params});
            continue;
        }

        if (t.type == TOK_PROPERTY) {
            ParsedPropertyDecl prop;
            if (!parse_property_decl(an.toks, i, prop)) continue;

            std::string owner = owner_name_from_scopes(scopes, an);
            bool in_member_owner = !owner.empty() && is_member_owner_scope(scopes.back().kind);
            if (!in_member_owner) continue;

            const Tok &nm = an.toks[(size_t)prop.name_idx];
            bool is_static = prop.is_static;
            if (scopes.back().is_static_owner) is_static = true;

            int sym_idx = decl_sym_idx_by_tok[(size_t)nm.idx];
            if (sym_idx < 0) {
                std::string key = std::string("p|") + owner + "." + nm.text;
                sym_idx = add_sym(an, next_id, SymKind::Property, nm, key, is_static, owner);
                mark_decl(nm.idx, sym_idx);
                members_by_name[nm.text].push_back(sym_idx);
            }

            an.syms[(size_t)sym_idx].container = owner;
            an.syms[(size_t)sym_idx].is_static = is_static;
            an.syms[(size_t)sym_idx].type_text = join_tok_text(an.toks, i + 1, prop.name_idx - 1);
            continue;
        }

        if (scopes.back().kind == ScopeKind::Enum && t.type == TOK_ID) {
            TokenType prev = (i > 0) ? an.toks[(size_t)(i - 1)].type : TOK_COMMA;
            TokenType next = (i + 1 < (int)an.toks.size()) ? an.toks[(size_t)(i + 1)].type : TOK_RBRACE;
            bool enum_item = (prev == TOK_LBRACE || prev == TOK_COMMA || prev == TOK_ASSIGN || prev == TOK_NUMBER) &&
                             (next == TOK_COMMA || next == TOK_ASSIGN || next == TOK_RBRACE);
            if (enum_item && decl_sym_idx_by_tok[(size_t)t.idx] < 0) {
                std::string owner;
                if (scopes.back().owner_sym_idx >= 0 && scopes.back().owner_sym_idx < (int)an.syms.size())
                    owner = an.syms[(size_t)scopes.back().owner_sym_idx].name;
                std::string key = owner.empty() ? std::string("e|") + t.text : std::string("e|") + owner + "." + t.text;
                int sym_idx = add_sym(an, next_id, SymKind::EnumMember, t, key, false, owner);
                int64_t val = scopes.back().next_enum_value;
                if (next == TOK_ASSIGN && i + 2 < (int)an.toks.size() && an.toks[(size_t)(i + 2)].type == TOK_NUMBER)
                    val = parse_i64_text(an.toks[(size_t)(i + 2)].text, val);
                scopes.back().next_enum_value = val + 1;
                an.syms[(size_t)sym_idx].type_text = scopes.back().enum_underlying.empty() ? "int" : scopes.back().enum_underlying;
                an.syms[(size_t)sym_idx].has_const_value = true;
                an.syms[(size_t)sym_idx].const_value = val;
                mark_decl(t.idx, sym_idx);
                scopes.back().names[t.text] = sym_idx;
                members_by_name[t.text].push_back(sym_idx);
                continue;
            }
        }

        if (prev_allows_decl(an.toks, i)) {
            bool starts = t.type == TOK_CONST || t.type == TOK_VAR || is_type_kw(t.type) || t.type == TOK_ID || is_decl_modifier(t.type);
            if (starts) {
                int j = i;
                bool seen_var_or_const = false;
                bool seen_var_kw = false;
                bool seen_static = false;
                while (j < (int)an.toks.size() && is_decl_modifier(an.toks[(size_t)j].type)) {
                    if (an.toks[(size_t)j].type == TOK_STATIC) seen_static = true;
                    j++;
                }
                if (j < (int)an.toks.size() && (an.toks[(size_t)j].type == TOK_CONST || an.toks[(size_t)j].type == TOK_VAR)) {
                    seen_var_or_const = true;
                    seen_var_kw = an.toks[(size_t)j].type == TOK_VAR;
                    j++;
                }

                int name_idx = -1;
                int first_non_prefix = j;
                for (; j < (int)an.toks.size(); ++j) {
                    const Tok &x = an.toks[(size_t)j];
                    if (x.type == TOK_SEMI || x.type == TOK_LBRACE || x.type == TOK_RBRACE || x.type == TOK_RPAREN) break;
                    if (x.type == TOK_ID && j + 1 < (int)an.toks.size()) {
                        TokenType nx = an.toks[(size_t)(j + 1)].type;
                        bool legacy_array_name =
                            nx == TOK_LBRACKET &&
                            j + 2 < (int)an.toks.size() &&
                            an.toks[(size_t)(j + 2)].type == TOK_RBRACKET &&
                            (j + 3 >= (int)an.toks.size() ||
                             an.toks[(size_t)(j + 3)].type == TOK_ASSIGN ||
                             an.toks[(size_t)(j + 3)].type == TOK_SEMI ||
                             an.toks[(size_t)(j + 3)].type == TOK_COMMA ||
                             an.toks[(size_t)(j + 3)].type == TOK_RPAREN);
                        if (nx == TOK_ASSIGN || nx == TOK_SEMI || nx == TOK_COMMA || legacy_array_name) {
                            name_idx = j;
                            break;
                        }
                    }
                    if (!typeish(x.type)) break;
                }

                if (name_idx >= 0) {
                    bool has_decl_prefix = seen_var_or_const || (name_idx > first_non_prefix);
                    if (has_decl_prefix && decl_sym_idx_by_tok[(size_t)an.toks[(size_t)name_idx].idx] < 0) {
                        const Tok &nm = an.toks[(size_t)name_idx];
                        bool member_scope = is_member_owner_scope(scopes.back().kind);
                        std::string owner = owner_name_from_scopes(scopes, an);
                        if (member_scope && scopes.back().is_static_owner) seen_static = true;
                        SymKind kind = member_scope ? SymKind::Field : SymKind::Variable;
                        std::string key;
                        if (kind == SymKind::Field && !owner.empty()) {
                            key = std::string("f|") + owner + "." + nm.text;
                        } else if (kind == SymKind::Variable && scopes.size() == 1) {
                            key = std::string("g|") + nm.text;
                        } else {
                            key = std::string("l|") + local_prefix + "|" + std::to_string(scopes.size()) + "|" + nm.text + "|" + std::to_string(nm.idx);
                        }
                        int sym_idx = add_sym(an, next_id, kind, nm, key, seen_static, owner);
                        std::string ty = decl_type_text_from_segment(an.toks, first_non_prefix, name_idx, j);
                        if (ty.empty() && seen_var_kw) ty = "var";
                        an.syms[(size_t)sym_idx].type_text = ty;
                        mark_decl(nm.idx, sym_idx);
                        scopes.back().names[nm.text] = sym_idx;
                        if (kind == SymKind::Field) members_by_name[nm.text].push_back(sym_idx);
                        if (kind == SymKind::Variable && scopes.size() == 1) globals_by_name[nm.text].push_back(sym_idx);
                    }
                }
            }
        }

        if (t.type != TOK_ID) continue;
        if (decl_sym_idx_by_tok[(size_t)t.idx] >= 0) continue;

        TokenType prev = (i > 0) ? an.toks[(size_t)(i - 1)].type : TOK_EOF;
        TokenType next = (i + 1 < (int)an.toks.size()) ? an.toks[(size_t)(i + 1)].type : TOK_EOF;
        bool member_ctx = prev == TOK_DOT;
        bool prefer_callable = next == TOK_LPAREN;
        bool prefer_type = (prev == TOK_NEW || prev == TOK_CLASS || prev == TOK_STRUCT || prev == TOK_INTERFACE ||
                            prev == TOK_ENUM || prev == TOK_LBRACKET || prev == TOK_COLON || prev == TOK_COMMA) &&
                           !member_ctx;

        int sym_idx = resolve_symbol(an, scopes, globals_by_name, members_by_name, t.text, member_ctx, prefer_type, prefer_callable);
        if (sym_idx < 0 || sym_idx >= (int)an.syms.size()) continue;
        const Symbol &sym = an.syms[(size_t)sym_idx];
        an.occs.push_back(Occ{sym.id, sym.key, mk_range(t.line, t.col, t.len), false, t.idx});
    }

    // Add occurrences for Unit/Get module names so definition/references work on them.
    {
        UnitDocIndex ux = index_units_from_tokens(an.toks);
        for (const UnitIndexItem &it : ux.items) {
            Occ o;
            o.sym_id = 0;
            o.key = std::string(k_unit_key_prefix) + it.unit;
            o.range = it.range;
            o.decl = it.decl;
            o.tok_idx = -1;
            an.occs.push_back(std::move(o));
        }
    }

    return an;
}

static bool string_token_has_format_prefix(const std::vector<Tok> &toks, int string_idx);
static void emit_format_string_interpolation_tokens(const Analysis &an,
                                                    std::string_view current_unit,
                                                    const std::string &text,
                                                    const std::vector<size_t> &line_starts,
                                                    const Tok &string_tok,
                                                    std::vector<SemTokSpan> &out);

void build_semantic_and_completion(Analysis &an, const std::string &text) {
    std::unordered_map<int, std::pair<int, int>> tok_style;
    std::unordered_map<int, const Symbol *> sid_to_sym;
    std::vector<size_t> line_starts;
    line_starts.reserve(128);
    line_starts.push_back(0);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') line_starts.push_back(i + 1);
    }
    for (const Symbol &s : an.syms) sid_to_sym[s.id] = &s;
    const std::string current_unit = first_declared_unit_name(index_units_from_tokens(an.toks));

    for (const Occ &o : an.occs) {
        auto it = sid_to_sym.find(o.sym_id);
        if (it == sid_to_sym.end()) continue;
        int t = sem_type_for_kind(it->second->kind);
        int mods = 0;
        if (o.decl) mods |= 1 << 0;
        if (it->second->is_static) mods |= 1 << 1;
        tok_style[o.tok_idx] = {t, mods};
    }

    // Hard overrides for module/unit names (Unit/Get/std modules).
    std::unordered_map<int, std::pair<int, int>> force_style;
    auto force = [&](int tok_idx, int type, int mods) { force_style[tok_idx] = {type, mods}; };
    auto force_if_unbound = [&](int tok_idx, int type, int mods) {
        if (tok_style.find(tok_idx) != tok_style.end()) return;
        force(tok_idx, type, mods);
    };

    // Unit + Get statements: treat module/unit names as namespaces.
    {
        int brace_depth = 0;
        for (int i = 0; i < (int)an.toks.size(); ++i) {
            TokenType tt = an.toks[(size_t)i].type;
            if (tt == TOK_LBRACE) brace_depth++;
            if (tt == TOK_RBRACE) brace_depth = std::max(0, brace_depth - 1);
            if (brace_depth != 0) continue;

            if (tt == TOK_UNIT) {
                int j = i + 1;
                std::string q;
                Range span;
                int start = -1, end = -1;
                if (!parse_qualified_name_span(an.toks, j, q, span, &start, &end)) continue;
                for (int k = start; k <= end; ++k) {
                    const Tok &t = an.toks[(size_t)k];
                    if (t.type == TOK_ID) force(t.idx, 0, 1 << 0); // namespace + declaration
                }
                continue;
            }

            if (tt == TOK_GET) {
                int j = i + 1;
                std::string first;
                Range first_span;
                int start = -1, end = -1;
                if (!parse_qualified_name_span(an.toks, j, first, first_span, &start, &end)) continue;
                bool first_has_list = (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_LBRACE);

                auto mark_module_prefix = [&](int s, int e, const std::string &qname, bool has_list) {
                    std::string module = module_from_get_qualified(qname, has_list);
                    int mods = kn_std_module_has(module.c_str()) ? (1 << 2) : 0; // defaultLibrary
                    int need = 1;
                    for (char c : module)
                        if (c == '.') need++;
                    int seen = 0;
                    for (int k = s; k <= e; ++k) {
                        const Tok &t = an.toks[(size_t)k];
                        if (t.type != TOK_ID) continue;
                        force(t.idx, 0, mods);
                        seen++;
                        if (seen >= need) break;
                    }
                };
                auto mark_imported_symbol = [&](int s, int e, const std::string &qname, bool has_list) {
                    GetQualifiedInfo info = classify_get_qualified(qname, has_list);
                    if (!info.symbol_import) return;
                    auto top = project_top_level_decl(info.module, info.member);
                    if (!top) return;
                    int tok_idx = -1;
                    for (int k = s; k <= e; ++k) {
                        const Tok &t = an.toks[(size_t)k];
                        if (t.type == TOK_ID) tok_idx = t.idx;
                    }
                    if (tok_idx < 0) return;
                    int mods = top->is_static ? (1 << 1) : 0;
                    force(tok_idx, sem_type_for_kind(top->kind), mods);
                };

                mark_module_prefix(start, end, first, first_has_list);
                mark_imported_symbol(start, end, first, first_has_list);

                if (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_BY) {
                    ++j;
                    std::string target;
                    Range target_span;
                    int s2 = -1, e2 = -1;
                    if (parse_qualified_name_span(an.toks, j, target, target_span, &s2, &e2)) {
                        bool target_has_list = (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_LBRACE);
                        mark_module_prefix(s2, e2, target, target_has_list);
                        mark_imported_symbol(s2, e2, target, target_has_list);
                        mark_imported_symbol(start, end, target, target_has_list);
                    }
                }
            }
        }
    }

    // Stdlib module qualifier prefixes (e.g. IO.Console.PrintLine -> namespace namespace method).
    {
        for (int i = 0; i < (int)an.toks.size(); ++i) {
            if (!qual_segment_tok(an.toks[(size_t)i].type)) continue;
            int j = i;
            std::vector<int> seg_tok_idxs;
            std::vector<std::string> seg_texts;
            seg_tok_idxs.push_back(an.toks[(size_t)j].idx);
            seg_texts.push_back(an.toks[(size_t)j].text);
            j++;
            while (j + 1 < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_DOT && qual_segment_tok(an.toks[(size_t)(j + 1)].type)) {
                seg_tok_idxs.push_back(an.toks[(size_t)(j + 1)].idx);
                seg_texts.push_back(an.toks[(size_t)(j + 1)].text);
                j += 2;
            }

            std::string prefix;
            for (size_t k = 0; k < seg_texts.size(); ++k) {
                if (k) prefix.push_back('.');
                prefix += seg_texts[k];
                if (kn_std_module_has(prefix.c_str())) {
                    for (size_t m = 0; m <= k; ++m) {
                        force(seg_tok_idxs[m], 0, 1 << 2); // namespace + defaultLibrary
                    }
                }
            }
            i = j - 1;
        }

        // Aliased stdlib modules (e.g. `Get Console By IO.Console; Console.PrintLine(...)`)
        for (const auto &kv : an.import_alias_to_module) {
            const std::string &alias = kv.first;
            const std::string &module = kv.second;
            int mods = kn_std_module_has(module.c_str()) ? (1 << 2) : 0;
            for (const Tok &t : an.toks) {
                if (t.type == TOK_ID && t.text == alias) force(t.idx, 0, mods);
            }
        }

        // Builtin object namespaces and classes (e.g. IO.Type.Object.Function, Object.Block).
        for (int i = 0; i < (int)an.toks.size(); ++i) {
            if (!qual_segment_tok(an.toks[(size_t)i].type)) continue;
            int j = i;
            std::vector<int> seg_tok_idxs;
            std::vector<std::string> seg_texts;
            seg_tok_idxs.push_back(an.toks[(size_t)j].idx);
            seg_texts.push_back(an.toks[(size_t)j].text);
            j++;
            while (j + 1 < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_DOT &&
                   qual_segment_tok(an.toks[(size_t)(j + 1)].type)) {
                seg_tok_idxs.push_back(an.toks[(size_t)(j + 1)].idx);
                seg_texts.push_back(an.toks[(size_t)(j + 1)].text);
                j += 2;
            }

            std::string q;
            for (size_t k = 0; k < seg_texts.size(); ++k) {
                if (k) q.push_back('.');
                q += seg_texts[k];
            }

            if (auto builtin_q = canonical_builtin_object_qname(q)) {
                int ns_count = text_starts_with(*builtin_q, "IO.Type.Object.") ? 3 : 0;
                if (q.size() >= 7 && q.substr(0, 7) == "Object.") ns_count = 1;
                for (int k = 0; k < ns_count && k < (int)seg_tok_idxs.size(); ++k)
                    force(seg_tok_idxs[(size_t)k], 0, 1 << 2);
                if (ns_count < (int)seg_tok_idxs.size())
                    force(seg_tok_idxs[(size_t)ns_count], sem_type_for_kind(SymKind::Class), 1 << 2);
                i = j - 1;
                continue;
            }

            if (builtin_object_is_namespace(q)) {
                for (size_t k = 0; k < seg_tok_idxs.size(); ++k)
                    force(seg_tok_idxs[k], 0, 1 << 2);
                i = j - 1;
                continue;
            }
        }
    }

    // Workspace-qualified module/type/member chains (e.g. MiniLang.Token.Token, T.Token).
    {
        for (int i = 0; i < (int)an.toks.size(); ++i) {
            if (!qual_segment_tok(an.toks[(size_t)i].type)) continue;
            int j = i;
            std::vector<int> seg_tok_idxs;
            std::vector<std::string> seg_texts;
            seg_tok_idxs.push_back(an.toks[(size_t)j].idx);
            seg_texts.push_back(an.toks[(size_t)j].text);
            j++;
            while (j + 1 < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_DOT && qual_segment_tok(an.toks[(size_t)(j + 1)].type)) {
                seg_tok_idxs.push_back(an.toks[(size_t)(j + 1)].idx);
                seg_texts.push_back(an.toks[(size_t)(j + 1)].text);
                j += 2;
            }

            std::string module;
            int module_seg_count = 0;
            auto it_alias = an.import_alias_to_module.find(seg_texts[0]);
            if (it_alias != an.import_alias_to_module.end()) {
                module = it_alias->second;
                module_seg_count = 1;
            } else {
                std::string pfx;
                for (int k = 0; k < (int)seg_texts.size(); ++k) {
                    if (!pfx.empty()) pfx.push_back('.');
                    pfx += seg_texts[(size_t)k];
                    if (unit_decl_exists(pfx)) {
                        module = pfx;
                        module_seg_count = k + 1;
                    }
                }
            }

            if (!module.empty() && module_seg_count > 0) {
                for (int k = 0; k < module_seg_count; ++k) force(seg_tok_idxs[(size_t)k], 0, 0);
                if (module_seg_count < (int)seg_texts.size()) {
                    auto top = project_top_level_decl(module, seg_texts[(size_t)module_seg_count]);
                    if (top) {
                        int mods = top->is_static ? (1 << 1) : 0;
                        force(seg_tok_idxs[(size_t)module_seg_count], sem_type_for_kind(top->kind), mods);
                        if (module_seg_count + 1 < (int)seg_texts.size()) {
                            auto mem = project_member_decl(top->qname, seg_texts[(size_t)(module_seg_count + 1)]);
                            if (mem) {
                                int mm = mem->is_static ? (1 << 1) : 0;
                                force(seg_tok_idxs[(size_t)(module_seg_count + 1)], sem_type_for_kind(mem->kind), mm);
                            }
                        }
                    }
                }
            } else {
                auto top = project_visible_top_level_decl_in_analysis(an, current_unit, seg_texts[0]);
                if (top) {
                    int mods = top->is_static ? (1 << 1) : 0;
                    force_if_unbound(seg_tok_idxs[0], sem_type_for_kind(top->kind), mods);
                    if ((int)seg_texts.size() > 1) {
                        auto mem = project_member_decl(top->qname, seg_texts[1]);
                        if (mem) {
                            int mm = mem->is_static ? (1 << 1) : 0;
                            force_if_unbound(seg_tok_idxs[1], sem_type_for_kind(mem->kind), mm);
                        }
                    }
                }
            }

            i = j - 1;
        }
    }

    std::vector<SemTokSpan> st;
    constexpr int SEM_COMMENT = 16; // matches init_json() tokenTypes (comment is appended at the end)
    st.reserve(an.toks.size() + an.comment_ranges.size());

    for (const Range &r : an.comment_ranges) {
        if (r.sl != r.el) continue;
        auto span = utf16_span_from_utf8_line_span(text, line_starts, r.sl, r.sc, std::max(1, r.ec - r.sc));
        st.push_back(SemTokSpan{r.sl, span.first, span.second, SEM_COMMENT, 0});
    }

    auto in_unnecessary = [&](const Tok &t) -> bool {
        if (an.unnecessary_ranges.empty()) return false;
        Range tr = mk_range(t.line, t.col, t.len);
        for (const Range &r : an.unnecessary_ranges) {
            if (r.contains(tr.sl, tr.sc) || tr.contains(r.sl, r.sc)) return true;
        }
        return false;
    };

    for (size_t ti = 0; ti < an.toks.size(); ++ti) {
        const Tok &t = an.toks[ti];
        TokenType prev = (ti > 0) ? an.toks[ti - 1].type : TOK_EOF;
        TokenType next = (ti + 1 < an.toks.size()) ? an.toks[ti + 1].type : TOK_EOF;

        int type = -1;
        int mods = 0;
        auto it = tok_style.find(t.idx);
        if (it != tok_style.end()) {
            type = it->second.first;
            mods = it->second.second;
        } else if (t.type == TOK_ID) {
            if (prev != TOK_DOT &&
                (t.text == "Meta" || t.text == "On" || t.text == "Keep" || t.text == "Repeatable" ||
                 is_builtin_meta_attr(t.text) || is_string_prefix_name(t.text))) {
                type = 12;
            } else if (prev == TOK_AT) {
                type = 12;
            } else {
                bool prev_decl_kw = prev == TOK_CLASS || prev == TOK_STRUCT || prev == TOK_INTERFACE || prev == TOK_ENUM;
                bool prev_decl_kw_in_qual = prev_decl_kw && ti >= 2 && an.toks[ti - 2].type == TOK_DOT;
                if (prev == TOK_DOT && next == TOK_LPAREN) type = 8;
                else if (prev == TOK_DOT) type = 11;
                else if (next == TOK_LPAREN) type = 7;
                else if (((prev_decl_kw && !prev_decl_kw_in_qual) ||
                          prev == TOK_NEW || prev == TOK_LBRACKET || prev == TOK_COLON || prev == TOK_COMMA)) {
                    type = 1;
                } else {
                    type = 9;
                }
            }
        } else if (is_kw(t.type)) {
            type = 12;
        } else if (is_type_kw(t.type)) {
            type = 1;
        } else if (t.type == TOK_STRING || t.type == TOK_BAD_STRING || t.type == TOK_CHAR_LIT || t.type == TOK_BAD_CHAR) {
            // Split format strings into sub-tokens so interpolation content is not string-colored.
            if ((t.type == TOK_STRING || t.type == TOK_BAD_STRING) &&
                t.text.size() >= 2 && string_token_has_format_prefix(an.toks, (int)ti)) {
                emit_format_string_interpolation_tokens(an, current_unit, text, line_starts, t, st);
                continue;
            }
            type = 13;
        } else if (t.type == TOK_NUMBER) {
            type = 14;
        } else if (is_op(t.type)) {
            type = 15;
        }
        if (t.type == TOK_ID) {
            if (prev != TOK_DOT &&
                (t.text == "Meta" || t.text == "On" || t.text == "Keep" || t.text == "Repeatable" ||
                 is_builtin_meta_attr(t.text) || is_string_prefix_name(t.text))) {
                type = 12;
            } else if (prev == TOK_AT) {
                type = 12;
            }
        }
        auto fi = force_style.find(t.idx);
        if (fi != force_style.end()) {
            type = fi->second.first;
            mods |= fi->second.second;
        }
        if (type < 0) continue;
        if (in_unnecessary(t)) mods |= 1 << 4; // deprecated
        int line0 = std::max(0, t.line - 1);
        auto span = utf16_span_from_utf8_line_span(text, line_starts, line0, std::max(0, t.col - 1), std::max(1, t.len));
        st.push_back(SemTokSpan{line0, span.first, span.second, type, mods});
    }

    std::sort(st.begin(), st.end(), [](const SemTokSpan &a, const SemTokSpan &b) {
        if (a.line != b.line) return a.line < b.line;
        if (a.col != b.col) return a.col < b.col;
        if (a.len != b.len) return a.len < b.len;
        return a.type < b.type;
    });

    an.sem_data.clear();
    an.sem_data.reserve(st.size() * 5);
    int pl = 0;
    int pc = 0;
    bool first = true;
    for (const SemTokSpan &x : st) {
        int dl = first ? x.line : (x.line - pl);
        int dc = (first || dl != 0) ? x.col : (x.col - pc);
        an.sem_data.push_back((uint32_t)std::max(0, dl));
        an.sem_data.push_back((uint32_t)std::max(0, dc));
        an.sem_data.push_back((uint32_t)std::max(1, x.len));
        an.sem_data.push_back((uint32_t)x.type);
        an.sem_data.push_back((uint32_t)x.mods);
        pl = x.line;
        pc = x.col;
        first = false;
    }

    static const char *kws[] = {
        "Unit", "Get", "By", "Public", "Private", "Protected", "Internal",
        "Function", "Constructor", "Property", "Class", "Struct", "Enum", "Interface",
        "If", "Else", "While", "For", "Switch", "Case", "Return", "Break", "Continue",
        "Throw", "Try", "Catch", "Set", "Block", "Record", "Jump", "Is",
        "Safe", "Trusted", "Unsafe", "Static", "Async", "Await", "Extern", "Virtual", "Override", "Abstract", "Sealed",
        "This", "Base", "Var", "Const", "default", "null", "true", "false",
        "Meta", "On", "Keep", "Repeatable",
        "@function", "@extern_function", "@delegate_function",
        "@class", "@struct", "@enum", "@interface", "@method", "@field",
        "@source", "@compile", "@runtime"
    };
    static const char *type_kws[] = {
        "void", "bool", "byte", "char", "int", "float",
        "f16", "f32", "f64", "f128",
        "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
        "isize", "usize", "string", "any", "list", "dict", "set", "null"
    };
    static const char *meta_attrs[] = {
        "Docs", "Deprecated", "Since", "Example", "Experimental"
    };
    static const char *string_prefixes[] = {
        "r", "raw", "f", "format", "c", "chars"
    };
    std::set<std::pair<std::string, int>> seen;
    an.completions.clear();
    for (const char *kw : kws) {
        Completion c;
        c.label = kw;
        c.kind = 14;
        c.detail = "keyword";
        if (seen.insert({c.label, c.kind}).second) an.completions.push_back(std::move(c));
    }
    for (const char *kw : type_kws) {
        Completion c;
        c.label = kw;
        c.kind = 14;
        c.detail = "type keyword";
        if (seen.insert({c.label, c.kind}).second) an.completions.push_back(std::move(c));
    }
    for (const char *kw : meta_attrs) {
        Completion c;
        c.label = kw;
        c.kind = 7;
        c.detail = "builtin meta attribute";
        if (seen.insert({c.label, c.kind}).second) an.completions.push_back(std::move(c));
    }
    for (const char *kw : string_prefixes) {
        Completion c;
        c.label = kw;
        c.kind = 14;
        c.detail = "string prefix";
        if (seen.insert({c.label, c.kind}).second) an.completions.push_back(std::move(c));
    }
    for (const Symbol &s : an.syms) {
        if (is_hidden_internal_name(s.name)) continue;
        Completion c;
        c.label = s.name;
        c.kind = completion_kind_for(s.kind);
        switch (s.kind) {
        case SymKind::Class: c.detail = "class"; break;
        case SymKind::Struct: c.detail = "struct"; break;
        case SymKind::Interface: c.detail = "interface"; break;
        case SymKind::Enum: c.detail = "enum"; break;
        case SymKind::EnumMember: c.detail = "enum member"; break;
        case SymKind::Function: c.detail = "function"; break;
        case SymKind::Method: c.detail = "method"; break;
        case SymKind::Field: c.detail = "field"; break;
        case SymKind::Property: c.detail = "property"; break;
        case SymKind::Variable: c.detail = "variable"; break;
        case SymKind::Parameter: c.detail = "parameter"; break;
        }
        if (seen.insert({c.label, c.kind}).second) an.completions.push_back(std::move(c));
    }
}

struct CapDiag {
    KnDiagStage stage = KN_STAGE_PARSER;
    KnDiagSeverity sev = KN_DIAG_SEV_ERROR;
    int line = 1;
    int col = 1;
    int len = 1;
    std::string code;
    std::string title;
    std::string detail;
    std::string got;
    std::string path;
};

void diag_sink(const KnDiagEvent *ev, void *ud) {
    if (!ev || !ud) return;
    auto *v = static_cast<std::vector<CapDiag> *>(ud);
    CapDiag d;
    d.stage = ev->stage;
    d.sev = ev->severity;
    d.line = ev->line;
    d.col = ev->col;
    d.len = ev->len;
    d.code = ev->code ? ev->code : "";
    d.title = ev->title ? ev->title : "";
    d.detail = ev->detail ? ev->detail : "";
    d.got = ev->got ? ev->got : "";
    d.path = normalize_fs_path((ev->src && ev->src->path) ? ev->src->path : "");
    v->push_back(std::move(d));
}

std::string diag_msg(const CapDiag &d) {
    std::string m;
    if (!d.title.empty() && !d.detail.empty()) {
        if (d.detail.rfind(d.title, 0) == 0) m = d.detail;
        else m = d.title + ": " + d.detail;
    } else if (!d.detail.empty()) {
        m = d.detail;
    } else if (!d.title.empty()) {
        m = d.title;
    } else {
        m = "Diagnostic";
    }
    if (!d.got.empty()) {
        if (d.got == "<end-of-file>") m += " (found end of file)";
        else m += " (found token '" + d.got + "')";
    }
    return m;
}

std::string stage_name(KnDiagStage s) {
    const bool zh = (g_cfg.diag_lang == "zh");
    switch (s) {
    case KN_STAGE_PARSER: return zh ? "语法分析" : "Parser";
    case KN_STAGE_SEMA: return zh ? "语义分析" : "Sema";
    case KN_STAGE_CODEGEN: return zh ? "代码生成" : "Codegen";
    }
    return zh ? "诊断" : "Diag";
}

int find_token_for_range(const std::vector<Tok> &toks, const Range &r) {
    int best = -1;
    int best_score = std::numeric_limits<int>::max();
    for (int i = 0; i < (int)toks.size(); ++i) {
        Range tr = mk_range(toks[(size_t)i].line, toks[(size_t)i].col, toks[(size_t)i].len);
        if (!(tr.contains(r.sl, r.sc) || (tr.sl == r.sl && tr.sc >= r.sc))) continue;
        int score = (tr.sl - r.sl) * 4096 + (tr.sc - r.sc);
        if (score < 0) score = -score;
        if (best < 0 || score < best_score) {
            best = i;
            best_score = score;
        }
    }
    return best;
}

Range range_from_tokens(const Tok &a, const Tok &b) {
    Range r;
    r.sl = std::max(0, a.line - 1);
    r.sc = std::max(0, a.col - 1);
    r.el = std::max(0, b.line - 1);
    r.ec = std::max(0, b.col - 1 + std::max(1, b.len));
    return r;
}

Range expand_unreachable_range(const std::vector<Tok> &toks, const Range &base) {
    int s = find_token_for_range(toks, base);
    if (s < 0 || s >= (int)toks.size()) return base;

    const Tok &st = toks[(size_t)s];
    if (st.type == TOK_LBRACE) {
        int e = find_matching(toks, s, TOK_LBRACE, TOK_RBRACE);
        if (e > s) return range_from_tokens(st, toks[(size_t)e]);
    }

    if (st.type == TOK_IF || st.type == TOK_ELSE || st.type == TOK_WHILE || st.type == TOK_FOR ||
        st.type == TOK_SWITCH || st.type == TOK_CASE || st.type == TOK_TRY || st.type == TOK_CATCH) {
        for (int i = s; i < (int)toks.size(); ++i) {
            if (toks[(size_t)i].type == TOK_LBRACE) {
                int e = find_matching(toks, i, TOK_LBRACE, TOK_RBRACE);
                if (e > i) return range_from_tokens(st, toks[(size_t)e]);
                break;
            }
            if (toks[(size_t)i].type == TOK_SEMI) return range_from_tokens(st, toks[(size_t)i]);
            if (i > s && toks[(size_t)i].line > st.line + 2) break;
        }
    }

    int depth = 0;
    int e = s;
    for (int i = s; i < (int)toks.size(); ++i) {
        TokenType tt = toks[(size_t)i].type;
        if (tt == TOK_LBRACE) depth++;
        if (tt == TOK_RBRACE) {
            if (depth == 0) break;
            depth--;
        }
        e = i;
        if (depth == 0 && tt == TOK_SEMI) break;
        if (depth == 0 && i > s && toks[(size_t)i].line != st.line) break;
    }
    return range_from_tokens(st, toks[(size_t)e]);
}

struct Doc {
    std::string uri;
    std::string path;
    std::string text;
    int version = 0;
    Analysis an;
};

std::unordered_map<std::string, Doc> g_docs;
std::unordered_map<std::string, DeclDocIndex> g_decl_doc_cache;
ServerConfig g_cfg;
std::string g_exe_dir;
bool g_shutdown = false;

std::optional<DeclInfo> project_qualified_top_level_decl(std::string_view qualified) {
    if (qualified.empty()) return std::nullopt;
    std::string q(qualified);
    std::string module = prefix_before_last_segment(q);
    std::string member = last_segment(q);
    if (module.empty() || member.empty()) return std::nullopt;
    return project_top_level_decl(module, member);
}

std::optional<DeclInfo> project_imported_symbol_decl(const Analysis &an, std::string_view local) {
    auto it = an.import_symbol_to_qualified.find(std::string(local));
    if (it == an.import_symbol_to_qualified.end()) return std::nullopt;
    return project_qualified_top_level_decl(it->second);
}

std::optional<DeclInfo> project_visible_top_level_decl_in_analysis(const Analysis &an,
                                                                   std::string_view current_unit,
                                                                   std::string_view name) {
    if (name.empty()) return std::nullopt;
    if (!current_unit.empty()) {
        if (auto top = project_top_level_decl(std::string(current_unit), name)) return top;
    }
    if (auto top = project_imported_symbol_decl(an, name)) return top;
    std::optional<DeclInfo> unique;
    for (const std::string &mod : an.import_open_modules) {
        if (auto top = project_top_level_decl(mod, name)) {
            if (unique && unique->qname != top->qname) return std::nullopt;
            unique = top;
        }
    }
    return unique;
}

std::optional<DeclInfo> project_visible_top_level_decl(const Doc &d, std::string_view name) {
    return project_visible_top_level_decl_in_analysis(d.an, first_declared_unit_name(index_units_from_tokens(d.an.toks)), name);
}

std::optional<std::string> load_doc_text_for_uri(const std::string &uri) {
    auto dit = g_docs.find(uri);
    if (dit != g_docs.end()) return dit->second.text;
    std::string norm = canonical_file_uri(uri);
    if (norm != uri) {
        dit = g_docs.find(norm);
        if (dit != g_docs.end()) return dit->second.text;
    }
    if (uri.rfind("file://", 0) == 0) return read_file_text(uri_to_path(uri));
    // Handle kinal-stdlib:/klib/... virtual URIs by resolving to cache file.
    std::string cache_path = klib_virtual_uri_to_file_path(uri);
    if (!cache_path.empty()) return read_file_text(std::filesystem::path(cache_path));
    return std::nullopt;
}

bool keep_decl_symbol(const Symbol &s) {
    switch (s.kind) {
    case SymKind::Class:
    case SymKind::Struct:
    case SymKind::Interface:
    case SymKind::Enum:
    case SymKind::EnumMember:
    case SymKind::Function:
    case SymKind::Method:
    case SymKind::Field:
    case SymKind::Property:
        return true;
    case SymKind::Variable:
        return s.key.rfind("g|", 0) == 0;
    case SymKind::Parameter:
        return false;
    }
    return false;
}

std::string first_declared_unit_name(const UnitDocIndex &idx) {
    for (const UnitIndexItem &it : idx.items) {
        if (it.decl && !it.unit.empty()) return it.unit;
    }
    return {};
}

DeclDocIndex build_decl_doc_index(const std::string &path, const std::string &uri, const std::string &text) {
    Analysis an = build_symbols(path, uri, text);
    DeclDocIndex out;
    out.unit = first_declared_unit_name(index_units_from_tokens(an.toks));
    out.decls.reserve(an.syms.size());
    for (const Symbol &s : an.syms) {
        if (!keep_decl_symbol(s)) continue;
        DeclInfo d;
        d.uri = uri;
        d.unit = out.unit;
        d.name = s.name;
        d.container = s.container;
        d.type_text = s.type_text;
        d.ret_text = s.ret_text;
        d.params = s.params;
        d.kind = s.kind;
        d.decl = s.decl;
        d.is_static = s.is_static;
        d.has_const_value = s.has_const_value;
        d.const_value = s.const_value;

        if (!d.unit.empty()) {
            if (!d.container.empty()) {
                d.owner_qname = d.unit + "." + d.container;
                d.qname = d.owner_qname + "." + d.name;
            } else {
                d.qname = d.unit + "." + d.name;
            }
        } else if (!d.container.empty()) {
            d.owner_qname = d.container;
            d.qname = d.container + "." + d.name;
        } else {
            d.qname = d.name;
        }

        out.decls.push_back(std::move(d));
    }
    return out;
}

const DeclDocIndex *decl_doc_index_for_uri(const std::string &uri) {
    std::string key = canonical_file_uri(uri);
    static thread_local std::unordered_set<std::string> building;
    if (building.find(key) != building.end()) return nullptr;

    struct BuildGuard {
        std::unordered_set<std::string> *set = nullptr;
        std::string key;
        BuildGuard(std::unordered_set<std::string> *s, std::string k) : set(s), key(std::move(k)) {
            if (set) set->insert(key);
        }
        ~BuildGuard() {
            if (set) set->erase(key);
        }
    };

    auto dit = g_docs.find(uri);
    if (dit == g_docs.end() && key != uri) dit = g_docs.find(key);
    if (dit != g_docs.end()) {
        static thread_local DeclDocIndex live;
        BuildGuard guard(&building, key);
        live = build_decl_doc_index(dit->second.path, dit->second.uri, dit->second.text);
        return &live;
    }

    auto it = g_decl_doc_cache.find(key);
    if (it != g_decl_doc_cache.end()) return &it->second;

    auto text = load_doc_text_for_uri(key);
    if (!text) return nullptr;

    BuildGuard guard(&building, key);
    DeclDocIndex idx = build_decl_doc_index(uri_to_path(key), key, *text);
    auto ins = g_decl_doc_cache.emplace(key, std::move(idx));
    return &ins.first->second;
}

std::vector<std::string> project_child_unit_segments(std::string_view prefix) {
    std::set<std::string> segs;
    std::string p(prefix);
    if (!p.empty()) p.push_back('.');
    for (const auto &kv : g_unit_docs) {
        for (const UnitIndexItem &it : kv.second.items) {
            if (!it.decl || it.unit.empty()) continue;
            std::string_view unit(it.unit);
            if (!text_starts_with(unit, p)) continue;
            std::string_view rest = unit.substr(p.size());
            if (rest.empty()) continue;
            size_t dot = rest.find('.');
            segs.insert(std::string(dot == std::string_view::npos ? rest : rest.substr(0, dot)));
        }
    }
    return std::vector<std::string>(segs.begin(), segs.end());
}

std::vector<DeclInfo> project_top_level_decls_for_module(const std::string &module) {
    std::vector<DeclInfo> out;
    if (module.empty()) return out;
    std::set<std::string> seen;
    for (const auto &kv : g_unit_docs) {
        bool matches = false;
        for (const UnitIndexItem &it : kv.second.items) {
            if (it.decl && it.unit == module) {
                matches = true;
                break;
            }
        }
        if (!matches) continue;
        const DeclDocIndex *idx = decl_doc_index_for_uri(kv.first);
        if (!idx) continue;
        for (const DeclInfo &d : idx->decls) {
            if (d.unit != module || !d.container.empty()) continue;
            if (!seen.insert(d.qname).second) continue;
            out.push_back(d);
        }
    }
    return out;
}

std::optional<DeclInfo> project_top_level_decl(const std::string &module, std::string_view name) {
    for (const DeclInfo &d : project_top_level_decls_for_module(module)) {
        if (d.name == name) return d;
    }
    return std::nullopt;
}

std::vector<DeclInfo> project_member_decls_for_owner(const std::string &owner_qname) {
    std::vector<DeclInfo> out;
    if (owner_qname.empty()) return out;
    std::string module = prefix_before_last_segment(owner_qname);
    if (module.empty()) return out;
    std::set<std::string> seen;
    for (const auto &kv : g_unit_docs) {
        bool matches = false;
        for (const UnitIndexItem &it : kv.second.items) {
            if (it.decl && it.unit == module) {
                matches = true;
                break;
            }
        }
        if (!matches) continue;
        const DeclDocIndex *idx = decl_doc_index_for_uri(kv.first);
        if (!idx) continue;
        for (const DeclInfo &d : idx->decls) {
            if (d.owner_qname != owner_qname) continue;
            if (!seen.insert(d.qname).second) continue;
            out.push_back(d);
        }
    }
    return out;
}

std::optional<DeclInfo> project_member_decl(const std::string &owner_qname, std::string_view name) {
    for (const DeclInfo &d : project_member_decls_for_owner(owner_qname)) {
        if (d.name == name) return d;
    }
    return std::nullopt;
}

std::string decl_completion_detail(const DeclInfo &d) {
    std::ostringstream oss;
    switch (d.kind) {
    case SymKind::Function:
    case SymKind::Method:
        oss << (d.ret_text.empty() ? "unknown" : d.ret_text) << " " << d.name << "(";
        for (size_t i = 0; i < d.params.size(); ++i) {
            if (i) oss << ", ";
            oss << d.params[i].first << " " << d.params[i].second;
        }
        oss << ")";
        return oss.str();
    case SymKind::Property:
        return d.type_text.empty() ? "property" : ("property " + d.type_text);
    case SymKind::Field:
    case SymKind::Variable:
    case SymKind::Parameter:
        return d.type_text;
    case SymKind::Class: return "class";
    case SymKind::Struct: return "struct";
    case SymKind::Interface: return "interface";
    case SymKind::Enum:
        return d.type_text.empty() ? "enum" : ("enum by " + d.type_text);
    case SymKind::EnumMember:
    {
        std::ostringstream em;
        if (!d.type_text.empty()) em << d.type_text << " ";
        em << d.name;
        if (d.has_const_value) em << " = " << d.const_value;
        return em.str();
    }
    }
    return {};
}

std::vector<ProjectSource> collect_project_sources(const std::string &uri, const std::string &path, const std::string &text,
                                                   const std::vector<Tok> &current_toks) {
    std::vector<ProjectSource> out;

    ProjectSource cur;
    cur.uri = canonical_file_uri(uri);
    cur.path = path;
    cur.text = text;
    cur.idx = index_units_from_tokens(current_toks);
    cur.current = true;
    out.push_back(cur);

    std::unordered_map<std::string, std::vector<std::string>> unit_to_uris;
    auto add_unit_uri = [&](std::string_view unit, const std::string &doc_uri) {
        if (unit.empty() || doc_uri.empty()) return;
        auto &vec = unit_to_uris[std::string(unit)];
        if (std::find(vec.begin(), vec.end(), doc_uri) == vec.end())
            vec.push_back(doc_uri);
    };
    for (const auto &kv : g_unit_docs) {
        const std::string doc_uri = canonical_file_uri(kv.first);
        for (const UnitIndexItem &it : kv.second.items) {
            if (!it.decl || it.unit.empty()) continue;
            add_unit_uri(it.unit, doc_uri);
        }
    }
    for (const UnitIndexItem &it : cur.idx.items) {
        if (!it.decl || it.unit.empty()) continue;
        add_unit_uri(it.unit, cur.uri);
    }

    const std::vector<std::filesystem::path> fallback_roots = guess_project_roots_for_source(path, cur.idx);
    std::vector<std::filesystem::path> same_unit_roots;
    std::unordered_set<std::string> same_unit_root_seen;
    auto add_same_unit_root = [&](std::filesystem::path p) {
        if (p.empty()) return;
        p = p.lexically_normal();
        std::string key = normalize_fs_path(p.string());
        if (key.empty()) return;
        if (!same_unit_root_seen.insert(key).second) return;
        same_unit_roots.push_back(std::move(p));
    };
    std::filesystem::path file_path(path);
    add_same_unit_root(file_path.parent_path());
    std::string current_unit = first_declared_unit_name(cur.idx);
    if (!current_unit.empty()) {
        std::filesystem::path root = file_path.parent_path();
        int steps = std::max(0, qualified_segment_count(current_unit) - 1);
        for (int i = 0; i < steps && !root.empty(); ++i) root = root.parent_path();
        add_same_unit_root(root);
    }
    for (const std::filesystem::path &root : fallback_roots) add_same_unit_root(root);
    bool added_workspace_units = false;

    auto resolve_unit_uris = [&](std::string_view unit) -> std::vector<std::string> {
        auto uit = unit_to_uris.find(std::string(unit));
        if (uit != unit_to_uris.end()) return uit->second;

        std::vector<std::string> out_uris;

        if (auto fallback = resolve_unit_uri_from_roots(unit, fallback_roots)) {
            const std::string dep_uri = canonical_file_uri(*fallback);
            auto dep_text = load_doc_text_for_uri(dep_uri);
            if (dep_text) {
                UnitDocIndex idx = index_units_from_tokens(lex_all(*dep_text));
                g_unit_docs[dep_uri] = idx;
                g_decl_doc_cache.erase(dep_uri);
                for (const UnitIndexItem &item : idx.items) {
                    if (!item.decl || item.unit.empty()) continue;
                    add_unit_uri(item.unit, dep_uri);
                }
                added_workspace_units = true;
            }
            out_uris.push_back(dep_uri);
        }

        if (out_uris.empty()) {
            for (const std::string &dep_uri : discover_declaring_unit_uris_from_roots(unit, fallback_roots)) {
                add_unit_uri(unit, dep_uri);
                if (std::find(out_uris.begin(), out_uris.end(), dep_uri) == out_uris.end())
                    out_uris.push_back(dep_uri);
            }
            if (!out_uris.empty()) added_workspace_units = true;
        }

        return out_uris;
    };
    auto resolve_same_unit_uris = [&](std::string_view unit) -> std::vector<std::string> {
        std::vector<std::string> out_uris;
        auto uit = unit_to_uris.find(std::string(unit));
        if (uit != unit_to_uris.end()) out_uris = uit->second;

        bool only_current = out_uris.size() == 1 && canonical_file_uri(out_uris.front()) == cur.uri;
        if (out_uris.empty() || only_current) {
            for (const std::string &dep_uri : discover_declaring_unit_uris_from_roots(unit, same_unit_roots)) {
                add_unit_uri(unit, dep_uri);
                if (std::find(out_uris.begin(), out_uris.end(), dep_uri) == out_uris.end())
                    out_uris.push_back(dep_uri);
            }
        }
        return out_uris;
    };

    std::unordered_set<std::string> seen_paths;
    seen_paths.insert(normalize_fs_path(path));

    std::vector<std::string> queue;
    auto enqueue_uri = [&](const std::string &dep_uri) {
        if (dep_uri.empty()) return;
        if (dep_uri.rfind("file://", 0) != 0) return;
        std::string dep_norm = normalize_fs_path(uri_to_path(dep_uri));
        if (dep_norm.empty()) return;
        if (!seen_paths.insert(dep_norm).second) return;
        queue.push_back(dep_uri);
    };
    auto enqueue_same_unit_sources = [&](const UnitDocIndex &idx) {
        for (const UnitIndexItem &it : idx.items) {
            if (!it.decl || it.unit.empty()) continue;
            std::vector<std::string> dep_uris = resolve_same_unit_uris(it.unit);
            for (const std::string &dep_uri : dep_uris) enqueue_uri(dep_uri);
        }
    };
    auto enqueue_refs = [&](const UnitDocIndex &idx) {
        for (const UnitIndexItem &it : idx.items) {
            if (it.decl || it.unit.empty()) continue;
            std::vector<std::string> dep_uris = resolve_unit_uris(it.unit);
            for (const std::string &dep_uri : dep_uris) enqueue_uri(dep_uri);
        }
    };

    enqueue_same_unit_sources(cur.idx);
    enqueue_refs(cur.idx);

    for (size_t i = 0; i < queue.size(); ++i) {
        const std::string dep_uri = canonical_file_uri(queue[i]);
        auto dep_text = load_doc_text_for_uri(dep_uri);
        if (!dep_text) continue;

        ProjectSource dep;
        dep.uri = dep_uri;
        dep.path = uri_to_path(dep_uri);
        dep.text = *dep_text;

        auto idx_it = g_unit_docs.find(dep_uri);
        if (idx_it != g_unit_docs.end()) dep.idx = idx_it->second;
        else dep.idx = index_units_from_tokens(lex_all(dep.text));

        for (const UnitIndexItem &item : dep.idx.items) {
            if (!item.decl || item.unit.empty()) continue;
            add_unit_uri(item.unit, dep.uri);
        }

        out.push_back(dep);
        enqueue_same_unit_sources(dep.idx);
        enqueue_refs(dep.idx);
    }

    if (added_workspace_units) rebuild_known_units();

    return out;
}

std::string normalize_diag_lang(std::string v) {
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    if (v == "zh" || v == "zh-cn" || v == "zh-hans" || v == "zh-hans-cn") return "zh";
    if (v == "auto") return {};
    return "en";
}

void apply_server_config(const Json *settings) {
    if (!settings || settings->kind != Json::Kind::Object) return;
    // vscode-languageclient may send `{settings:{kinal:{...}}}`; accept both shapes.
    const Json *kinal = settings->find("kinal");
    if (kinal && kinal->kind == Json::Kind::Object) settings = kinal;

    if (const Json *v = settings->find("diagnosticsLanguage"); v && v->kind == Json::Kind::String) {
        std::string nv = normalize_diag_lang(v->s);
        if (!nv.empty()) g_cfg.diag_lang = nv;
    }
    if (const Json *v = settings->find("localeFile"); v && v->kind == Json::Kind::String) {
        g_cfg.locale_file = v->s;
    }

    // Default locale behavior (mirrors kinal.exe): for zh, prefer locales/zh-CN.json near the server exe.
    if (g_cfg.locale_file.empty() && g_cfg.diag_lang == "zh") {
        std::vector<std::filesystem::path> cands;
        if (!g_exe_dir.empty()) cands.push_back(std::filesystem::path(g_exe_dir) / "locales" / "zh-CN.json");
        cands.push_back(std::filesystem::path("locales") / "zh-CN.json");
        for (const auto &p : cands) {
            std::error_code ec;
            if (std::filesystem::exists(p, ec) && !ec) {
                g_cfg.locale_file = p.string();
                break;
            }
        }
    }
}

Analysis analyze_doc(const std::string &uri, const std::string &path, const std::string &text) {
    Analysis an = build_symbols(path, uri, text);
    an.comment_ranges = scan_comment_ranges(text);

    // Virtual stdlib/klib documents are read-only library source.
    // Only provide semantic tokens and completions; skip full sema to avoid noisy diagnostics.
    if (uri.rfind("kinal-stdlib:", 0) == 0) {
        build_semantic_and_completion(an, text);
        return an;
    }

    kn_temp_arena_begin();
    std::vector<CapDiag> cap;
    MetaList metas{};
    FuncList funcs{};
    ImportList imports{};
    ClassList classes{};
    InterfaceList interfaces{};
    StructList structs{};
    EnumList enums{};
    StmtList globals{};
    std::vector<ProjectSource> sources = collect_project_sources(uri, path, text, an.toks);
    std::vector<KnSource> parsed_sources;
    parsed_sources.reserve(sources.size());

    auto configure_diag_capture = [&](std::vector<CapDiag> *dst) {
        kn_diag_reset();
        kn_diag_set_color_mode(KN_COLOR_NEVER);
        kn_diag_set_language(g_cfg.diag_lang.c_str());
        if (!g_cfg.locale_file.empty())
            kn_diag_set_locale_file(g_cfg.locale_file.c_str());
        kn_diag_set_quiet(1);
        kn_diag_set_max_errors(1000000);
        kn_diag_set_warning_level(4);
        kn_diag_set_sink(diag_sink, dst);
    };

    configure_diag_capture(&cap);

    int current_parse_errors = 0;
    for (const ProjectSource &ps : sources) {
        KnSource src{};
        kn_source_init(&src, ps.path.c_str(), ps.text.c_str(), ps.text.size());
        parsed_sources.push_back(src);
        KnSource *src_ptr = &parsed_sources.back();

        FuncList file_funcs{};
        ImportList file_imports{};
        ClassList file_classes{};
        InterfaceList file_interfaces{};
        StructList file_structs{};
        EnumList file_enums{};
        StmtList file_globals{};
        MetaList file_metas{};

        size_t cap_before = cap.size();
        int errs_before = kn_diag_error_count();
        parse_program(src_ptr, &file_metas, &file_funcs, &file_imports, &file_classes, &file_interfaces, &file_structs, &file_enums, &file_globals);
        int file_parse_errors = kn_diag_error_count() - errs_before;

        if (ps.current) {
            current_parse_errors = file_parse_errors;
            merge_program_lists(file_metas, file_funcs, file_imports, file_classes, file_interfaces, file_structs, file_enums, file_globals,
                                &metas, &funcs, &imports, &classes, &interfaces, &structs, &enums, &globals);
            continue;
        }

        if (file_parse_errors > 0) {
            cap.resize(cap_before);
            continue;
        }

        merge_program_lists(file_metas, file_funcs, file_imports, file_classes, file_interfaces, file_structs, file_enums, file_globals,
                            &metas, &funcs, &imports, &classes, &interfaces, &structs, &enums, &globals);
    }
    kn_diag_set_sink(nullptr, nullptr);

    int require_main = 0;
    int sema_crashed = 0;
    if (current_parse_errors == 0) {
        // Mirror the CLI driver: parser errors mean the AST is incomplete, so
        // sema on top of it is both noisy and crash-prone for in-progress edits.
        configure_diag_capture(&cap);
        kn_inject_builtins(&classes);

        for (int i = 0; i < funcs.count; ++i) {
            const Func &f = funcs.items[i];
            if (!f.name) continue;
            if (std::string_view(f.name) != "Main") continue;
            if (f.type_param_count > 0) continue;
            if (f.is_generic_instance) continue;
            require_main = 1;
            break;
        }
        const KnSource *first_src = parsed_sources.empty() ? nullptr : &parsed_sources.front();
#if defined(_MSC_VER)
        __try
        {
            KnSemaOptions sema_opts{};
            sema_opts.require_entry = require_main;
            sema_opts.env_kind = KN_ENV_HOSTED;
            sema_opts.entry_name = "Main";
            (void)kn_sema_check(first_src, &metas, &funcs, &imports, &classes, &interfaces, &structs, &enums, &globals, &sema_opts);
        }
        __except (1)
        {
            sema_crashed = 1;
        }
#else
        KnSemaOptions sema_opts{};
        sema_opts.require_entry = require_main;
        sema_opts.env_kind = KN_ENV_HOSTED;
        sema_opts.entry_name = "Main";
        (void)kn_sema_check(first_src, &metas, &funcs, &imports, &classes, &interfaces, &structs, &enums, &globals, &sema_opts);
#endif
    }
    kn_diag_set_sink(nullptr, nullptr);

    if (sema_crashed)
    {
        // If sema crashed, any sema-stage diagnostics are incomplete/noisy.
        // Keep parser errors and only surface an internal error when parsing succeeded.
        std::vector<CapDiag> keep;
        keep.reserve(cap.size() + 1);
        for (const CapDiag &d : cap)
        {
            if (d.stage == KN_STAGE_PARSER)
                keep.push_back(d);
        }
        cap.swap(keep);

        if (current_parse_errors == 0)
        {
        CapDiag d;
        d.stage = KN_STAGE_SEMA;
        d.sev = KN_DIAG_SEV_ERROR;
        d.line = 1;
        d.col = 1;
        d.len = 1;
        d.code = "E-LSP-00001";
        d.title = "Internal Error";
        d.detail = "Semantic analysis crashed; some diagnostics may be missing.";
        cap.push_back(std::move(d));
        }
    }

    const std::string current_norm_path = normalize_fs_path(path);
    for (const CapDiag &d : cap) {
        if (!d.path.empty() && d.path != current_norm_path) continue;
        Diag di;
        di.code = d.code;
        di.message = diag_msg(d);
        di.source = stage_name(d.stage);
        di.severity = (d.sev == KN_DIAG_SEV_WARNING) ? 2 : 1;
        di.range = mk_range(d.line, d.col, d.len);
        std::string lt = d.title;
        std::transform(lt.begin(), lt.end(), lt.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        di.unnecessary = (di.severity == 2) && (lt.find("unused") != std::string::npos || lt.find("unreachable") != std::string::npos);
        if (di.unnecessary) {
            bool is_unreachable = (d.code == "W-LNT-00002") || (lt.find("unreachable") != std::string::npos);
            Range nr = is_unreachable ? expand_unreachable_range(an.toks, di.range) : di.range;
            an.unnecessary_ranges.push_back(nr);
        }
        an.diags.push_back(std::move(di));
    }

    build_semantic_and_completion(an, text);
    for (KnSource &src : parsed_sources) kn_source_free(&src);
    kn_temp_arena_end();
    return an;
}

void update_analysis(Doc &d) { d.an = analyze_doc(d.uri, d.path, d.text); }

std::string diag_params_json(const Doc &d) {
    std::ostringstream oss;
    oss << "{\"uri\":\"" << esc(d.uri) << "\",\"diagnostics\":[";
    for (size_t i = 0; i < d.an.diags.size(); ++i) {
        if (i) oss << ",";
        const Diag &x = d.an.diags[i];
        oss << "{";
        oss << "\"range\":" << range_json(x.range) << ",";
        oss << "\"severity\":" << x.severity << ",";
        oss << "\"code\":\"" << esc(x.code) << "\",";
        oss << "\"source\":\"" << esc(x.source) << "\",";
        oss << "\"message\":\"" << esc(x.message) << "\"";
        if (x.unnecessary) oss << ",\"tags\":[1]";
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

void publish_diags(const Doc &d) { notify("textDocument/publishDiagnostics", diag_params_json(d)); }

struct Hit {
    int sym_id = 0;
    std::string key;
    bool decl = false;
};

std::optional<Hit> hit_at(const Doc &d, int line, int ch) {
    const Occ *best = nullptr;
    int score = std::numeric_limits<int>::max();
    for (const Occ &o : d.an.occs) {
        if (!o.range.contains(line, ch)) continue;
        int s = o.range.score();
        if (!best || s < score || (s == score && o.decl)) {
            best = &o;
            score = s;
        }
    }
    if (!best) return std::nullopt;
    return Hit{best->sym_id, best->key, best->decl};
}

struct Loc {
    std::string uri;
    Range range;
};

// Forward declarations used by stdlib stubs.
bool starts_with(std::string_view s, std::string_view pfx);
std::string type_text(Type t);
bool std_module_is_method_like(std::string_view module);
int tok_at_pos(const std::vector<Tok> &toks, int line0, int ch0);
int tok_before_pos(const std::vector<Tok> &toks, int line0, int ch0);
std::vector<std::string> discover_declaring_unit_uris_from_roots(std::string_view unit,
                                                                 const std::vector<std::filesystem::path> &roots);

std::string stdlib_uri_for_module(std::string_view module) {
    std::string m(module);
    // Keep the URI stable and file-like so VSCode can infer the filename.
    return std::string("kinal-stdlib:/") + m + ".kn";
}

std::optional<std::string> packaged_unit_uri(std::string_view unit) {
    if (unit.empty()) return std::nullopt;
    auto roots = stdlib_source_roots();
    if (auto uri = resolve_unit_uri_from_roots(unit, roots)) return uri;
    auto decls = discover_declaring_unit_uris_from_roots(unit, roots);
    if (!decls.empty()) return decls.front();
    return std::nullopt;
}

std::optional<std::string> packaged_unit_text(std::string_view unit) {
    auto uri = packaged_unit_uri(unit);
    if (!uri) return std::nullopt;
    std::string path = uri_to_path(*uri);
    return read_file_text(std::filesystem::path(path));
}

bool std_prefix_has_children(std::string_view prefix) {
    std::string p(prefix);
    if (!p.empty()) p.push_back('.');
    int n = kn_std_module_count();
    for (int i = 0; i < n; ++i) {
        const KnStdModule *m = kn_std_module_at(i);
        if (!m || !m->name) continue;
        std::string_view mn(m->name);
        if (p.empty()) {
            if (!mn.empty()) return true;
        } else if (starts_with(mn, p)) {
            return true;
        }
    }
    return false;
}

std::vector<std::string> std_direct_children(std::string_view prefix) {
    std::set<std::string> segs;
    std::string p(prefix);
    if (!p.empty()) p.push_back('.');
    int n = kn_std_module_count();
    for (int i = 0; i < n; ++i) {
        const KnStdModule *m = kn_std_module_at(i);
        if (!m || !m->name) continue;
        std::string_view mn(m->name);
        if (!starts_with(mn, p)) continue;
        std::string_view rest = mn.substr(p.size());
        if (rest.empty()) continue;
        size_t dot = rest.find('.');
        segs.insert(std::string(dot == std::string_view::npos ? rest : rest.substr(0, dot)));
    }
    return std::vector<std::string>(segs.begin(), segs.end());
}

struct StdDocCache {
    std::string text;
    Range unit_name_range;
    std::unordered_map<std::string, Range> name_ranges; // functions, types, etc.
};

std::unordered_map<std::string, StdDocCache> g_std_doc_cache;

const StdDocCache &std_doc_cache(std::string_view module) {
    std::string key(module);
    if (key == "Object") key = "IO.Type.Object";
    auto it = g_std_doc_cache.find(key);
    if (it != g_std_doc_cache.end()) return it->second;

    StdDocCache c;
    c.text.clear();
    c.name_ranges.clear();
    c.unit_name_range = Range{0, 0, 0, 0};

    auto add_line = [&](std::string_view s) {
        c.text.append(s.data(), s.size());
        c.text.push_back('\n');
    };

    const bool is_mod = (!key.empty() && kn_std_module_has(key.c_str()));
    const bool has_children = (!key.empty() && std_prefix_has_children(key));
    const bool is_builtin_object_ns = (key == "IO.Type.Object");

    if (is_builtin_object_ns) {
        add_line("// Builtin object runtime types.");
        add_line("");
        std::string unit_line = "Unit " + key + ";";
        add_line(unit_line);
        c.unit_name_range = Range{2, 5, 2, 5 + (int)key.size()};
        add_line("");
        int line = 4;
        auto add_decl = [&](std::string_view name, std::string_view base) {
            std::string ln = "Class " + std::string(name);
            if (!base.empty()) ln += " By " + std::string(base);
            ln += " {}";
            add_line(ln);
            c.name_ranges[std::string(name)] = Range{line, 6, line, 6 + (int)name.size()};
            line++;
        };
        add_decl("Class", "");
        add_decl("Function", "IO.Type.Object.Class");
        add_decl("Block", "IO.Type.Object.Class");
    } else if (is_mod) {
        add_line("// Builtin module provided by the runtime.");
        add_line("");
        std::string unit_line = "Unit " + key + ";";
        add_line(unit_line);
        c.unit_name_range = Range{2, 5, 2, 5 + (int)key.size()};
        add_line("");

        // IO.Type is special: the runtime exposes primitive types/constructors that overlap with keywords
        // (e.g. `int`), so rendering them as `Extern Function int int(...)` is invalid source.
        if (key == "IO.Type") {
            int line = 4;
            add_line("// Primitive types:");
            line++;
            static const char *prim[] = {
                "void",  "bool",   "byte",  "char",  "int",   "float", "f16",   "f32",   "f64",
                "f128",  "i8",    "i16",   "i32",
                "i64",   "u8",     "u16",   "u32",   "u64",   "isize", "usize", "string","any",  "null"};
            for (const char *p : prim) {
                std::string ln = p;
                std::string stmt = ln + ";";
                add_line(stmt);
                c.name_ranges[ln] = Range{line, 0, line, (int)ln.size()};
                line++;
            }
            add_line("");
            add_line("// Cast syntax:");
            add_line("//   [int](\"123\")");
            add_line("//   [void*](&fn)");
            add_line("//   [T*](ptr)");
            add_line("//   [T**](ptr)");
        } else {
            const KnStdModule *mod = nullptr;
            int n = kn_std_module_count();
            for (int i = 0; i < n; ++i) {
                const KnStdModule *m = kn_std_module_at(i);
                if (m && m->name && key == m->name) { mod = m; break; }
            }
            if (mod) {
                const bool omit_recv = std_module_is_method_like(mod->name);
                int line = 4;
                for (int k = 0; k < mod->count; ++k) {
                    const KnStdFunc &f = mod->funcs[k];
                    if (!f.name) continue;
                    std::ostringstream oss;
                    oss << "Extern " << (f.safety == SAFETY_UNSAFE ? "Unsafe " : "Safe ");
                    oss << "Static Function " << type_text(f.ret_type) << " " << f.name << "(";
                    int start = (omit_recv && f.param_count > 0) ? 1 : 0;
                    int pi = 0;
                    for (int p = start; p < f.param_count; ++p) {
                        if (p != start) oss << ", ";
                        oss << type_text(f.params[p]) << " p" << pi++;
                    }
                    oss << ") By System;";
                    std::string ln = oss.str();
                    add_line(ln);
                    size_t col = ln.find(f.name);
                    if (col != std::string::npos) {
                        c.name_ranges[f.name] = Range{line, (int)col, line, (int)col + (int)kn_strlen(f.name)};
                    }
                    line++;
                }
            }
        }

        if (has_children) {
            add_line("");
            add_line("// Submodules:");
            for (const std::string &seg : std_direct_children(key)) {
                add_line(key + "." + seg + ";");
            }
        }
    } else if (has_children) {
        add_line("// Builtin namespace provided by the runtime.");
        add_line("");
        std::string unit_line = "Unit " + key + ";";
        add_line(unit_line);
        c.unit_name_range = Range{2, 5, 2, 5 + (int)key.size()};
        add_line("");
        add_line("// Modules:");
        for (const std::string &seg : std_direct_children(key)) {
            add_line(key + "." + seg + ";");
        }
    } else {
        add_line("// Unknown stdlib module: " + key);
    }

    auto ins = g_std_doc_cache.emplace(std::move(key), std::move(c));
    return ins.first->second;
}

std::optional<Loc> stdlib_loc_for(std::string_view module, std::string_view symbol) {
    if (module.empty()) return std::nullopt;
    std::string m(module);
    if (m == "Object") m = "IO.Type.Object";
    if (!kn_std_module_has(m.c_str()) && !std_prefix_has_children(m) && !builtin_object_namespace_has_children(m)) {
        auto uri = packaged_unit_uri(m);
        if (uri) {
            return Loc{*uri, Range{0, 0, 0, 1}};
        }
    }
    const StdDocCache &c = std_doc_cache(m);
    Range r = c.unit_name_range;
    if (!symbol.empty()) {
        auto it = c.name_ranges.find(std::string(symbol));
        if (it != c.name_ranges.end()) r = it->second;
    }
    return Loc{stdlib_uri_for_module(m), r};
}

std::vector<Loc> unit_defs_for_name(const std::string &unit) {
    std::vector<Loc> out;
    if (!unit.empty() && (kn_std_module_has(unit.c_str()) || std_prefix_has_children(unit) || builtin_object_namespace_has_children(unit))) {
        if (auto l = stdlib_loc_for(unit, "")) out.push_back(*l);
        return out;
    }
    for (const auto &kv : g_unit_docs) {
        const std::string &uri = kv.first;
        const UnitDocIndex &d = kv.second;
        for (const UnitIndexItem &it : d.items) {
            if (!it.decl) continue;
            if (it.unit != unit) continue;
            out.push_back(Loc{uri, it.range});
        }
    }
    if (out.empty()) {
        if (auto top = project_qualified_top_level_decl(unit))
            out.push_back(Loc{top->uri, top->decl});
    }
    if (out.empty()) {
        if (auto uri = packaged_unit_uri(unit))
            out.push_back(Loc{*uri, Range{0, 0, 0, 1}});
    }
    return out;
}

std::vector<Loc> unit_refs_for_name(const std::string &unit, bool include_decl) {
    std::vector<Loc> out;
    for (const auto &kv : g_unit_docs) {
        const std::string &uri = kv.first;
        const UnitDocIndex &d = kv.second;
        for (const UnitIndexItem &it : d.items) {
            if (it.unit != unit) continue;
            if (!include_decl && it.decl) continue;
            out.push_back(Loc{uri, it.range});
        }
    }
    return out;
}

std::string resolve_alias_prefix(const Analysis &an, const std::vector<std::string> &segs, int upto) {
    if (segs.empty() || upto < 0) return {};
    if (upto >= (int)segs.size()) upto = (int)segs.size() - 1;
    std::string out;
    int start = 0;
    auto it = an.import_alias_to_module.find(segs[0]);
    if (it != an.import_alias_to_module.end()) {
        out = it->second;
        start = 1;
    } else {
        out = segs[0];
        start = 1;
    }
    for (int i = start; i <= upto; ++i) {
        out.push_back('.');
        out += segs[(size_t)i];
    }
    return out;
}

std::optional<Loc> stdlib_definition_at(const Doc &d, int line, int ch) {
    int ti = tok_at_pos(d.an.toks, line, ch);
    if (ti < 0) ti = tok_before_pos(d.an.toks, line, ch);
    if (ti < 0 || ti >= (int)d.an.toks.size()) return std::nullopt;

    if (!qual_segment_tok(d.an.toks[(size_t)ti].type) && d.an.toks[(size_t)ti].type == TOK_DOT && ti > 0)
        ti--;
    if (ti < 0 || !qual_segment_tok(d.an.toks[(size_t)ti].type)) return std::nullopt;

    // Primitive types: jump to IO.Type docs.
    TokenType prev = (ti > 0) ? d.an.toks[(size_t)(ti - 1)].type : TOK_EOF;
    TokenType next = (ti + 1 < (int)d.an.toks.size()) ? d.an.toks[(size_t)(ti + 1)].type : TOK_EOF;
    if ((is_type_kw(d.an.toks[(size_t)ti].type) || d.an.toks[(size_t)ti].type == TOK_NULL) && prev != TOK_DOT && next != TOK_DOT) {
        return stdlib_loc_for("IO.Type", d.an.toks[(size_t)ti].text);
    }

    int l = ti;
    while (l >= 2 && d.an.toks[(size_t)(l - 1)].type == TOK_DOT && qual_segment_tok(d.an.toks[(size_t)(l - 2)].type))
        l -= 2;
    int r = ti;
    while (r + 2 < (int)d.an.toks.size() && d.an.toks[(size_t)(r + 1)].type == TOK_DOT && qual_segment_tok(d.an.toks[(size_t)(r + 2)].type))
        r += 2;

    std::vector<int> seg_tok_i;
    std::vector<std::string> segs;
    for (int i = l; i <= r; i += 2) {
        seg_tok_i.push_back(i);
        segs.push_back(d.an.toks[(size_t)i].text);
    }
    if (segs.empty()) return std::nullopt;

    int seg_idx = -1;
    for (int i = 0; i < (int)seg_tok_i.size(); ++i) {
        if (seg_tok_i[(size_t)i] == ti) {
            seg_idx = i;
            break;
        }
    }
    if (seg_idx < 0) return std::nullopt;

    const int last = (int)segs.size() - 1;
    const std::string clicked = segs[(size_t)seg_idx];
    std::string q;
    for (size_t i = 0; i < segs.size(); ++i) {
        if (i) q.push_back('.');
        q += segs[i];
    }

    if (auto builtin_q = canonical_builtin_object_qname(q)) {
        if (seg_idx == last) return stdlib_loc_for("IO.Type.Object", last_segment(*builtin_q));
        return stdlib_loc_for("IO.Type.Object", "");
    }
    if (builtin_object_is_namespace(q) || (seg_idx == 0 && clicked == "Object")) {
        return stdlib_loc_for("IO.Type.Object", "");
    }

    // 1) If clicking the last segment, try resolve as stdlib function.
    if (seg_idx == last) {
        // Unqualified: PrintLine(...)
        if (segs.size() == 1) {
            auto itq = d.an.import_symbol_to_qualified.find(clicked);
            if (itq != d.an.import_symbol_to_qualified.end()) {
                std::string q = itq->second;
                std::string mod = prefix_before_last_segment(q);
                std::string nm = last_segment(q);
                if (kn_std_find(mod.c_str(), nm.c_str())) {
                    return stdlib_loc_for(mod, nm);
                }
            }
            auto ita = d.an.import_alias_to_module.find(clicked);
            if (ita != d.an.import_alias_to_module.end()) {
                std::string mod = ita->second;
                if (kn_std_module_has(mod.c_str()) || std_prefix_has_children(mod)) {
                    return stdlib_loc_for(mod, "");
                }
            }
        } else {
            std::string mod = resolve_alias_prefix(d.an, segs, last - 1);
            if (!mod.empty() && kn_std_find(mod.c_str(), clicked.c_str())) {
                return stdlib_loc_for(mod, clicked);
            }
        }
    }

    // 2) Otherwise, treat as stdlib module/namespace prefix.
    std::string pfx = resolve_alias_prefix(d.an, segs, seg_idx);
    if (pfx.empty()) return std::nullopt;
    if (kn_std_module_has(pfx.c_str()) || std_prefix_has_children(pfx)) {
        return stdlib_loc_for(pfx, "");
    }
    return std::nullopt;
}

std::vector<Loc> project_definition_at(const Doc &d, int line, int ch) {
    std::vector<Loc> out;

    int ti = tok_at_pos(d.an.toks, line, ch);
    if (ti < 0) ti = tok_before_pos(d.an.toks, line, ch);
    if (ti < 0 || ti >= (int)d.an.toks.size()) return out;

    if (!qual_segment_tok(d.an.toks[(size_t)ti].type) && d.an.toks[(size_t)ti].type == TOK_DOT && ti > 0)
        ti--;
    if (ti < 0 || !qual_segment_tok(d.an.toks[(size_t)ti].type)) return out;

    int l = ti;
    while (l >= 2 && d.an.toks[(size_t)(l - 1)].type == TOK_DOT && qual_segment_tok(d.an.toks[(size_t)(l - 2)].type))
        l -= 2;
    int r = ti;
    while (r + 2 < (int)d.an.toks.size() && d.an.toks[(size_t)(r + 1)].type == TOK_DOT && qual_segment_tok(d.an.toks[(size_t)(r + 2)].type))
        r += 2;

    std::vector<int> seg_tok_i;
    std::vector<std::string> segs;
    for (int i = l; i <= r; i += 2) {
        seg_tok_i.push_back(i);
        segs.push_back(d.an.toks[(size_t)i].text);
    }
    if (segs.empty()) return out;

    int seg_idx = -1;
    for (int i = 0; i < (int)seg_tok_i.size(); ++i) {
        if (seg_tok_i[(size_t)i] == ti) {
            seg_idx = i;
            break;
        }
    }
    if (seg_idx < 0) return out;

    auto it_alias = d.an.import_alias_to_module.find(segs[0]);
    if (it_alias != d.an.import_alias_to_module.end()) {
        if (seg_idx == 0) return unit_defs_for_name(it_alias->second);
        if (auto top = project_top_level_decl(it_alias->second, segs[(size_t)1])) {
            if (seg_idx == 1) {
                out.push_back(Loc{top->uri, top->decl});
                return out;
            }
            if (seg_idx == 2) {
                if (auto mem = project_member_decl(top->qname, segs[(size_t)2])) {
                    out.push_back(Loc{mem->uri, mem->decl});
                    return out;
                }
            }
        }
        return out;
    }

    std::string module;
    int module_seg_count = 0;
    std::string pfx;
    for (int i = 0; i < (int)segs.size(); ++i) {
        if (!pfx.empty()) pfx.push_back('.');
        pfx += segs[(size_t)i];
        if (unit_decl_exists(pfx)) {
            module = pfx;
            module_seg_count = i + 1;
        }
    }

    if (module.empty()) {
        if (auto top = project_visible_top_level_decl(d, segs[0])) {
            if (seg_idx == 0) {
                out.push_back(Loc{top->uri, top->decl});
                return out;
            }
            if (seg_idx == 1) {
                if (auto mem = project_member_decl(top->qname, segs[(size_t)seg_idx])) {
                    out.push_back(Loc{mem->uri, mem->decl});
                    return out;
                }
            }
        }
        if (auto mem = project_member_decl_via_receiver(d, seg_tok_i, segs, seg_idx)) {
            out.push_back(Loc{mem->uri, mem->decl});
            return out;
        }

        // If clicking a namespace prefix, jump to the longest unit that extends it.
        for (int k = (int)segs.size(); k >= seg_idx + 1; --k) {
            std::string cand;
            for (int i = 0; i < k; ++i) {
                if (i) cand.push_back('.');
                cand += segs[(size_t)i];
            }
            if (unit_decl_exists(cand)) return unit_defs_for_name(cand);
        }
        return out;
    }

    if (seg_idx < module_seg_count) return unit_defs_for_name(module);

    if (seg_idx == module_seg_count) {
        if (auto top = project_top_level_decl(module, segs[(size_t)seg_idx])) {
            out.push_back(Loc{top->uri, top->decl});
        }
        return out;
    }

    if (seg_idx == module_seg_count + 1) {
        auto top = project_top_level_decl(module, segs[(size_t)module_seg_count]);
        if (!top) return out;
        if (auto mem = project_member_decl(top->qname, segs[(size_t)seg_idx])) {
            out.push_back(Loc{mem->uri, mem->decl});
        }
    }
    return out;
}

std::optional<std::string> project_hover_text_at(const Doc &d, int line, int ch) {
    int ti = tok_at_pos(d.an.toks, line, ch);
    if (ti < 0) ti = tok_before_pos(d.an.toks, line, ch);
    if (ti < 0 || ti >= (int)d.an.toks.size()) return std::nullopt;

    if (!qual_segment_tok(d.an.toks[(size_t)ti].type) && d.an.toks[(size_t)ti].type == TOK_DOT && ti > 0)
        ti--;
    if (ti < 0 || !qual_segment_tok(d.an.toks[(size_t)ti].type)) return std::nullopt;

    int l = ti;
    while (l >= 2 && d.an.toks[(size_t)(l - 1)].type == TOK_DOT && qual_segment_tok(d.an.toks[(size_t)(l - 2)].type))
        l -= 2;
    int r = ti;
    while (r + 2 < (int)d.an.toks.size() && d.an.toks[(size_t)(r + 1)].type == TOK_DOT && qual_segment_tok(d.an.toks[(size_t)(r + 2)].type))
        r += 2;

    std::vector<int> seg_tok_i;
    std::vector<std::string> segs;
    for (int i = l; i <= r; i += 2) {
        seg_tok_i.push_back(i);
        segs.push_back(d.an.toks[(size_t)i].text);
    }
    if (segs.empty()) return std::nullopt;

    int seg_idx = -1;
    for (int i = 0; i < (int)seg_tok_i.size(); ++i) {
        if (seg_tok_i[(size_t)i] == ti) {
            seg_idx = i;
            break;
        }
    }
    if (seg_idx < 0) return std::nullopt;

    std::string q;
    for (size_t i = 0; i < segs.size(); ++i) {
        if (i) q.push_back('.');
        q += segs[i];
    }
    if (auto builtin_q = canonical_builtin_object_qname(q)) {
        if (seg_idx == (int)segs.size() - 1) return builtin_object_decl_sig(*builtin_q);
        return std::string("Unit IO.Type.Object");
    }
    if (builtin_object_is_namespace(q) || (seg_idx == 0 && segs[(size_t)seg_idx] == "Object"))
        return std::string("Unit IO.Type.Object");

    auto it_alias = d.an.import_alias_to_module.find(segs[0]);
    if (it_alias != d.an.import_alias_to_module.end()) {
        if (seg_idx == 0) return std::nullopt;
        if (auto top = project_top_level_decl(it_alias->second, segs[(size_t)1])) {
            if (seg_idx == 1) return decl_sig(*top);
            if (seg_idx == 2) {
                if (auto mem = project_member_decl(top->qname, segs[(size_t)2]))
                    return decl_sig(*mem);
            }
        }
        return std::nullopt;
    }

    std::string module;
    int module_seg_count = 0;
    std::string pfx;
    for (int i = 0; i < (int)segs.size(); ++i) {
        if (!pfx.empty()) pfx.push_back('.');
        pfx += segs[(size_t)i];
        if (unit_decl_exists(pfx)) {
            module = pfx;
            module_seg_count = i + 1;
        }
    }

    if (module.empty()) {
        if (auto top = project_visible_top_level_decl(d, segs[0])) {
            if (seg_idx == 0) return decl_sig(*top);
            if (seg_idx == 1) {
                if (auto mem = project_member_decl(top->qname, segs[(size_t)seg_idx]))
                    return decl_sig(*mem);
            }
        }
        if (auto mem = project_member_decl_via_receiver(d, seg_tok_i, segs, seg_idx))
            return decl_sig(*mem);
        return std::nullopt;
    }
    if (seg_idx < module_seg_count) return std::nullopt;
    if (seg_idx == module_seg_count) {
        if (auto top = project_top_level_decl(module, segs[(size_t)seg_idx])) return decl_sig(*top);
        return std::nullopt;
    }
    if (seg_idx == module_seg_count + 1) {
        auto top = project_top_level_decl(module, segs[(size_t)module_seg_count]);
        if (!top) return std::nullopt;
        if (auto mem = project_member_decl(top->qname, segs[(size_t)seg_idx])) return decl_sig(*mem);
    }
    return std::nullopt;
}

std::vector<Loc> defs_for(const std::string &key) {
    std::vector<Loc> out;
    for (const auto &kv : g_docs) {
        const Doc &d = kv.second;
        for (const Occ &o : d.an.occs) {
            if (o.key == key && o.decl) out.push_back(Loc{d.uri, o.range});
        }
    }
    return out;
}

std::vector<Loc> refs_for(const std::string &key, bool include_decl) {
    std::vector<Loc> out;
    for (const auto &kv : g_docs) {
        const Doc &d = kv.second;
        for (const Occ &o : d.an.occs) {
            if (o.key != key) continue;
            if (!include_decl && o.decl) continue;
            out.push_back(Loc{d.uri, o.range});
        }
    }
    return out;
}

std::string locs_json(const std::vector<Loc> &locs) {
    std::ostringstream oss;
    oss << "[";
    for (size_t i = 0; i < locs.size(); ++i) {
        if (i) oss << ",";
        std::string uri = locs[i].uri;
        std::string virt = file_uri_to_klib_virtual_uri(uri);
        if (!virt.empty()) uri = virt;
        oss << "{\"uri\":\"" << esc(uri) << "\",\"range\":" << range_json(locs[i].range) << "}";
    }
    oss << "]";
    return oss.str();
}

int lsp_symbol_kind(SymKind k) {
    switch (k) {
    case SymKind::Class: return 5;
    case SymKind::Struct: return 23;
    case SymKind::Interface: return 11;
    case SymKind::Enum: return 10;
    case SymKind::EnumMember: return 22;
    case SymKind::Function: return 12;
    case SymKind::Method: return 6;
    case SymKind::Property: return 7;
    case SymKind::Field: return 8;
    case SymKind::Variable: return 13;
    case SymKind::Parameter: return 13;
    }
    return 13;
}

std::string document_symbols_json(const Doc &d) {
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const Symbol &s : d.an.syms) {
        if (is_hidden_internal_name(s.name)) continue;
        if (!first) oss << ",";
        first = false;
        oss << "{";
        oss << "\"name\":\"" << esc(s.name) << "\",";
        oss << "\"kind\":" << lsp_symbol_kind(s.kind) << ",";
        oss << "\"location\":{\"uri\":\"" << esc(d.uri) << "\",\"range\":" << range_json(s.decl) << "}";
        if (!s.container.empty()) oss << ",\"containerName\":\"" << esc(s.container) << "\"";
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

std::string completion_list_json(const std::vector<Completion> &items) {
    std::ostringstream oss;
    oss << "{\"isIncomplete\":false,\"items\":[";
    bool first = true;
    for (size_t i = 0; i < items.size(); ++i) {
        const Completion &c = items[i];
        if (is_hidden_internal_name(c.label)) continue;
        if (!first) oss << ",";
        first = false;
        oss << "{";
        oss << "\"label\":\"" << esc(c.label) << "\",";
        oss << "\"kind\":" << c.kind;
        if (!c.detail.empty()) oss << ",\"detail\":\"" << esc(c.detail) << "\"";
        if (!c.insert_text.empty()) {
            oss << ",\"insertText\":\"" << esc(c.insert_text) << "\"";
            if (c.insert_text_format != 0) oss << ",\"insertTextFormat\":" << c.insert_text_format;
        }
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

bool starts_with(std::string_view s, std::string_view pfx) {
    return pfx.empty() || (s.size() >= pfx.size() && s.substr(0, pfx.size()) == pfx);
}

bool is_hidden_internal_name(std::string_view s) {
    return s.size() >= 2 && s[0] == '_' && s[1] == '_';
}

Range tok_range(const Tok &t) { return mk_range(t.line, t.col, t.len); }

int tok_at_pos(const std::vector<Tok> &toks, int line0, int ch0) {
    for (int i = 0; i < (int)toks.size(); ++i) {
        if (tok_range(toks[(size_t)i]).contains(line0, ch0)) return i;
    }
    return -1;
}

int tok_before_pos(const std::vector<Tok> &toks, int line0, int ch0) {
    int best = -1;
    for (int i = 0; i < (int)toks.size(); ++i) {
        Range r = tok_range(toks[(size_t)i]);
        if (r.el < line0 || (r.el == line0 && r.ec <= ch0)) {
            best = i;
            continue;
        }
        if (r.sl > line0 || (r.sl == line0 && r.sc > ch0)) break;
    }
    return best;
}

std::string tok_prefix_at(const Tok &t, int line0, int ch0) {
    Range r = tok_range(t);
    if (r.sl != line0) return {};
    int rel = ch0 - r.sc;
    if (rel <= 0) return {};
    if (rel > (int)t.text.size()) rel = (int)t.text.size();
    return t.text.substr(0, (size_t)rel);
}

std::string qualifier_before_dot(const std::vector<Tok> &toks, int dot_i) {
    if (dot_i <= 0 || dot_i >= (int)toks.size()) return {};
    std::vector<std::string> segs;
    int i = dot_i - 1;
    if (!qual_segment_tok(toks[(size_t)i].type)) return {};
    segs.push_back(toks[(size_t)i].text);
    i--;
    while (i >= 1 && toks[(size_t)i].type == TOK_DOT && qual_segment_tok(toks[(size_t)(i - 1)].type)) {
        segs.push_back(toks[(size_t)(i - 1)].text);
        i -= 2;
    }
    std::reverse(segs.begin(), segs.end());
    std::string out;
    for (size_t k = 0; k < segs.size(); ++k) {
        if (k) out.push_back('.');
        out += segs[k];
    }
    return out;
}

const Symbol *sym_by_id(const Analysis &an, int id) {
    for (const Symbol &s : an.syms) {
        if (s.id == id) return &s;
    }
    return nullptr;
}

const Symbol *sym_for_token(const Analysis &an, int tok_idx) {
    const Occ *best = nullptr;
    for (const Occ &o : an.occs) {
        if (o.tok_idx != tok_idx) continue;
        if (!best || (o.decl && !best->decl)) best = &o;
    }
    if (!best) return nullptr;
    return sym_by_id(an, best->sym_id);
}

std::string resolve_owner_qname_in_doc(const Doc &d, std::string owner) {
    if (owner.empty()) return {};

    if (owner.find('.') != std::string::npos) {
        size_t dot = owner.find('.');
        std::string head = owner.substr(0, dot);
        auto it = d.an.import_alias_to_module.find(head);
        if (it != d.an.import_alias_to_module.end())
            return it->second + owner.substr(dot);
        return owner;
    }

    std::string cur_unit = first_declared_unit_name(index_units_from_tokens(d.an.toks));
    if (!cur_unit.empty()) {
        if (auto top = project_top_level_decl(cur_unit, owner))
            return top->qname;
    }

    auto it_symbol = d.an.import_symbol_to_qualified.find(owner);
    if (it_symbol != d.an.import_symbol_to_qualified.end())
        return it_symbol->second;

    for (const auto &kv : d.an.import_alias_to_module) {
        if (auto top = project_top_level_decl(kv.second, owner))
            return top->qname;
    }

    for (const std::string &mod : d.an.import_open_modules) {
        if (auto top = project_top_level_decl(mod, owner))
            return top->qname;
    }

    std::optional<DeclInfo> unique;
    for (const auto &kv : g_unit_docs) {
        const DeclDocIndex *idx = decl_doc_index_for_uri(kv.first);
        if (!idx) continue;
        for (const DeclInfo &dcl : idx->decls) {
            if (!dcl.container.empty()) continue;
            if (dcl.name != owner) continue;
            if (unique && unique->qname != dcl.qname) return {};
            unique = dcl;
        }
    }
    return unique ? unique->qname : std::string{};
}

std::vector<Loc> project_defs_for_hit(const Doc &d, const Hit &h) {
    std::vector<Loc> out;
    const Symbol *s = sym_by_id(d.an, h.sym_id);
    if (!s || s->name.empty()) return out;

    if (!s->container.empty()) {
        std::string owner_qname = resolve_owner_qname_in_doc(d, s->container);
        if (!owner_qname.empty()) {
            if (auto mem = project_member_decl(owner_qname, s->name))
                out.push_back(Loc{mem->uri, mem->decl});
        }
        return out;
    }

    std::string cur_unit = first_declared_unit_name(index_units_from_tokens(d.an.toks));
    if (!cur_unit.empty()) {
        if (auto top = project_top_level_decl(cur_unit, s->name))
            out.push_back(Loc{top->uri, top->decl});
    }
    return out;
}

std::optional<DeclInfo> project_member_decl_via_receiver(const Doc &d,
                                                         const std::vector<int> &seg_tok_i,
                                                         const std::vector<std::string> &segs,
                                                         int seg_idx) {
    if (seg_idx <= 0 || seg_idx >= (int)segs.size() || seg_idx >= (int)seg_tok_i.size())
        return std::nullopt;

    int recv_tok_i = seg_tok_i[(size_t)(seg_idx - 1)];
    if (recv_tok_i < 0 || recv_tok_i >= (int)d.an.toks.size())
        return std::nullopt;

    const Symbol *recv_sym = sym_for_token(d.an, d.an.toks[(size_t)recv_tok_i].idx);
    std::string base;
    if (recv_sym) {
        if (kind_is_type(recv_sym->kind)) base = recv_sym->name;
        else if (!recv_sym->type_text.empty()) base = normalize_base_type(recv_sym->type_text);
    }
    if (base.empty()) return std::nullopt;

    std::string owner_qname = resolve_owner_qname_in_doc(d, base);
    if (owner_qname.empty()) owner_qname = base;
    return project_member_decl(owner_qname, segs[(size_t)seg_idx]);
}

std::string typekind_text(TypeKind k) {
    switch (k) {
    case TY_VOID: return "void";
    case TY_BOOL: return "bool";
    case TY_BYTE: return "byte";
    case TY_CHAR: return "char";
    case TY_INT: return "int";
    case TY_FLOAT: return "float";
    case TY_F16: return "f16";
    case TY_F32: return "f32";
    case TY_F64: return "f64";
    case TY_F128: return "f128";
    case TY_I8: return "i8";
    case TY_I16: return "i16";
    case TY_I32: return "i32";
    case TY_I64: return "i64";
    case TY_U8: return "u8";
    case TY_U16: return "u16";
    case TY_U32: return "u32";
    case TY_U64: return "u64";
    case TY_ISIZE: return "isize";
    case TY_USIZE: return "usize";
    case TY_STRING: return "string";
    case TY_ANY: return "any";
    case TY_NULL: return "null";
    case TY_PTR: return "ptr";
    case TY_ARRAY: return "array";
    case TY_CLASS: return "class";
    case TY_STRUCT: return "struct";
    case TY_ENUM: return "enum";
    case TY_UNKNOWN: return "unknown";
    }
    return "unknown";
}

std::string type_text(Type t) {
    if (t.kind == TY_PTR) {
        std::string b = typekind_text(t.elem);
        if (t.ptr_depth < 1) t.ptr_depth = 1;
        for (int i = 0; i < t.ptr_depth; ++i) b.push_back('*');
        return b;
    }
    if (t.kind == TY_ARRAY) {
        std::string b = typekind_text(t.elem);
        b += "[]";
        return b;
    }
    if (t.kind == TY_CLASS || t.kind == TY_STRUCT || t.kind == TY_ENUM) {
        if (t.name && t.name[0]) return std::string(t.name);
        return typekind_text(t.kind);
    }
    return typekind_text(t.kind);
}

bool std_module_is_method_like(std::string_view module) {
    return (module != "IO.Type" && module.rfind("IO.Type.", 0) == 0) ||
           module.rfind("IO.Collection.", 0) == 0;
}

std::string stdfunc_sig(const KnStdFunc &f, std::string_view module, bool omit_receiver) {
    std::ostringstream oss;
    oss << type_text(f.ret_type) << " " << f.name << "(";
    int start = (omit_receiver && f.param_count > 0) ? 1 : 0;
    for (int i = start; i < f.param_count; ++i) {
        if (i != start) oss << ", ";
        oss << type_text(f.params[i]);
    }
    oss << ")";
    if (!module.empty()) oss << "  [" << module << "]";
    return oss.str();
}

std::string stdlib_stub_text(std::string_view module) {
    std::string mod(module);
    if (mod.empty()) return {};
    return std_doc_cache(mod).text;
}

void add_unique(std::vector<Completion> &out, std::set<std::pair<std::string, int>> &seen, Completion c) {
    if (!seen.insert({c.label, c.kind}).second) return;
    out.push_back(std::move(c));
}

void add_std_funcs(std::vector<Completion> &out,
                   std::set<std::pair<std::string, int>> &seen,
                   std::string_view module,
                   std::string_view label_prefix) {
    int n = kn_std_module_count();
    for (int i = 0; i < n; ++i) {
        const KnStdModule *m = kn_std_module_at(i);
        if (!m || !m->name) continue;
        if (module != m->name) continue;
        const bool omit_recv = std_module_is_method_like(m->name);
        for (int k = 0; k < m->count; ++k) {
            const KnStdFunc &f = m->funcs[k];
            if (!f.name) continue;
            if (!starts_with(f.name, label_prefix)) continue;
            Completion c;
            c.label = f.name;
            c.kind = 3;
            c.detail = stdfunc_sig(f, m->name, omit_recv);
            add_unique(out, seen, std::move(c));
        }
        return;
    }
}

void add_std_child_modules(std::vector<Completion> &out,
                          std::set<std::pair<std::string, int>> &seen,
                          std::string_view prefix_module,
                          std::string_view label_prefix) {
    std::string pfx(prefix_module);
    if (!pfx.empty()) pfx.push_back('.');
    std::set<std::string> seg_seen;
    int n = kn_std_module_count();
    for (int i = 0; i < n; ++i) {
        const KnStdModule *m = kn_std_module_at(i);
        if (!m || !m->name) continue;
        std::string_view mn(m->name);
        if (!starts_with(mn, pfx)) continue;
        std::string_view rest = mn.substr(pfx.size());
        if (rest.empty()) continue;
        size_t dot = rest.find('.');
        std::string seg = std::string(dot == std::string_view::npos ? rest : rest.substr(0, dot));
        if (!starts_with(seg, label_prefix)) continue;
        if (!seg_seen.insert(seg).second) continue;
        Completion c;
        c.label = seg;
        c.kind = 9;
        c.detail = "module";
        add_unique(out, seen, std::move(c));
    }
}

void add_builtin_object_children(std::vector<Completion> &out,
                                 std::set<std::pair<std::string, int>> &seen,
                                 std::string_view prefix_module,
                                 std::string_view label_prefix) {
    for (const auto &child : builtin_object_children(prefix_module)) {
        if (!starts_with(child.first, label_prefix)) continue;
        Completion c;
        c.label = child.first;
        c.kind = child.second;
        c.detail = (child.second == 9) ? "namespace" : "builtin object type";
        add_unique(out, seen, std::move(c));
    }
}

// Add compile-time constant members for IO.Target, IO.Host, IO.Runtime namespaces.
void add_info_constant_members(std::vector<Completion> &out,
                               std::set<std::pair<std::string, int>> &seen,
                               std::string_view module,
                               std::string_view label_prefix) {
    struct ConstItem { const char *name; const char *detail; };
    static const ConstItem target_members[] = {
        {"Current",              "int \xe2\x80\x94 current OS id"},
        {"Windows",              "int \xe2\x80\x94 OS constant"},
        {"Linux",                "int \xe2\x80\x94 OS constant"},
        {"MacOS",                "int \xe2\x80\x94 OS constant"},
        {"Unknown",              "int \xe2\x80\x94 OS constant"},
        {"X86",                  "int \xe2\x80\x94 Arch constant"},
        {"X64",                  "int \xe2\x80\x94 Arch constant"},
        {"Arm64",                "int \xe2\x80\x94 Arch constant"},
        {"Hosted",               "int \xe2\x80\x94 Env constant"},
        {"Freestanding",         "int \xe2\x80\x94 Env constant"},
        {"PointerBits",          "int \xe2\x80\x94 pointer size in bits"},
        {"PathSeparator",        "string \xe2\x80\x94 path separator"},
        {"NewLine",              "string \xe2\x80\x94 line ending"},
        {"ExeSuffix",            "string \xe2\x80\x94 executable suffix"},
        {"ObjectSuffix",         "string \xe2\x80\x94 object file suffix"},
        {"DynamicLibrarySuffix", "string \xe2\x80\x94 dynamic library suffix"},
        {"StaticLibrarySuffix",  "string \xe2\x80\x94 static library suffix"},
    };
    static const ConstItem os_members[] = {
        {"Current", "int \xe2\x80\x94 current OS id"},
        {"Windows", "int \xe2\x80\x94 OS constant"},
        {"Linux",   "int \xe2\x80\x94 OS constant"},
        {"MacOS",   "int \xe2\x80\x94 OS constant"},
        {"Unknown", "int \xe2\x80\x94 OS constant"},
    };
    static const ConstItem arch_members[] = {
        {"Current", "int \xe2\x80\x94 current architecture id"},
        {"X86",     "int \xe2\x80\x94 Arch constant"},
        {"X64",     "int \xe2\x80\x94 Arch constant"},
        {"Arm64",   "int \xe2\x80\x94 Arch constant"},
        {"Unknown", "int \xe2\x80\x94 Arch constant"},
    };
    static const ConstItem env_members[] = {
        {"Current",      "int \xe2\x80\x94 current environment id"},
        {"Hosted",       "int \xe2\x80\x94 Env constant"},
        {"Freestanding", "int \xe2\x80\x94 Env constant"},
        {"Unknown",      "int \xe2\x80\x94 Env constant"},
    };
    static const ConstItem runtime_members[] = {
        {"Current",  "int \xe2\x80\x94 current runtime backend id"},
        {"Unknown",  "int \xe2\x80\x94 Runtime.Kind constant"},
        {"Native",   "int \xe2\x80\x94 Runtime.Kind constant"},
        {"VM",       "int \xe2\x80\x94 Runtime.Kind constant"},
        {"Name",     "string \xe2\x80\x94 runtime name"},
        {"IsNative", "bool \xe2\x80\x94 true when native backend"},
        {"IsVM",     "bool \xe2\x80\x94 true when VM backend"},
    };
    static const ConstItem runtime_kind_members[] = {
        {"Current", "int \xe2\x80\x94 current runtime backend id"},
        {"Unknown", "int \xe2\x80\x94 Runtime.Kind constant"},
        {"Native",  "int \xe2\x80\x94 Runtime.Kind constant"},
        {"VM",      "int \xe2\x80\x94 Runtime.Kind constant"},
    };
    static const ConstItem version_members[] = {
        {"Major",  "int \xe2\x80\x94 compiler major version"},
        {"Minor",  "int \xe2\x80\x94 compiler minor version"},
        {"Patch",  "int \xe2\x80\x94 compiler patch version"},
        {"String", "string \xe2\x80\x94 compiler version string"},
    };
    static const ConstItem version_vm_members[] = {
        {"Major",  "int \xe2\x80\x94 VM major version (0 when native)"},
        {"Minor",  "int \xe2\x80\x94 VM minor version (0 when native)"},
        {"Patch",  "int \xe2\x80\x94 VM patch version (0 when native)"},
        {"String", "string \xe2\x80\x94 VM version string (empty when native)"},
    };

    const ConstItem *items = nullptr;
    size_t count = 0;
    if (module == "IO.Target") { items = target_members; count = sizeof(target_members)/sizeof(target_members[0]); }
    else if (module == "IO.Host") { items = target_members; count = sizeof(target_members)/sizeof(target_members[0]); }
    else if (module == "IO.Target.OS" || module == "IO.Host.OS") { items = os_members; count = sizeof(os_members)/sizeof(os_members[0]); }
    else if (module == "IO.Target.Arch" || module == "IO.Host.Arch") { items = arch_members; count = sizeof(arch_members)/sizeof(arch_members[0]); }
    else if (module == "IO.Target.Env" || module == "IO.Host.Env") { items = env_members; count = sizeof(env_members)/sizeof(env_members[0]); }
    else if (module == "IO.Runtime") { items = runtime_members; count = sizeof(runtime_members)/sizeof(runtime_members[0]); }
    else if (module == "IO.Runtime.Kind") { items = runtime_kind_members; count = sizeof(runtime_kind_members)/sizeof(runtime_kind_members[0]); }
    else if (module == "IO.Version") { items = version_members; count = sizeof(version_members)/sizeof(version_members[0]); }
    else if (module == "IO.Version.VM") { items = version_vm_members; count = sizeof(version_vm_members)/sizeof(version_vm_members[0]); }
    if (!items) return;
    for (size_t i = 0; i < count; ++i) {
        if (!starts_with(items[i].name, label_prefix)) continue;
        Completion c;
        c.label = items[i].name;
        c.kind = 21; // Constant
        c.detail = items[i].detail;
        add_unique(out, seen, std::move(c));
    }
}

std::string std_module_for_receiver(std::string_view base) {
    if (base == "string" || base == "any") {
        return "IO.Type." + std::string(base);
    }
    if (base == "dict" || base == "IO.Collection.dict")
        return "IO.Collection.dict";
    if (base == "list" || base == "IO.Collection.list")
        return "IO.Collection.list";
    if (base == "set" || base == "IO.Collection.set")
        return "IO.Collection.set";
    return std::string(base);
}

bool is_block_type_name(std::string_view base) {
    if (base == "Object.Block" || base == "IO.Type.Object.Block") return true;
    if (base.size() >= 13 && base.substr(base.size() - 13) == "Object.Block") return true;
    return false;
}

void add_block_methods(std::vector<Completion> &out,
                       std::set<std::pair<std::string, int>> &seen,
                       std::string_view label_prefix) {
    static const char *ms[] = {"Run", "Jump", "RunUntil", "JumpAndRunUntil"};
    for (const char *m : ms) {
        if (!starts_with(m, label_prefix)) continue;
        Completion c;
        c.label = m;
        c.kind = 2;
        c.detail = "block method";
        add_unique(out, seen, std::move(c));
    }
}

static bool is_ident_char_simple(char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '_';
}

static std::optional<size_t> offset_for_line_col_simple(const std::string &text, int line0, int ch0) {
    if (line0 < 0 || ch0 < 0) return std::nullopt;
    int cur_line = 0;
    int cur_col = 0;
    for (size_t i = 0; i < text.size(); ++i) {
        if (cur_line == line0 && cur_col == ch0) return i;
        if (text[i] == '\n') {
            cur_line++;
            cur_col = 0;
            continue;
        }
        cur_col++;
    }
    if (cur_line == line0 && cur_col == ch0) return text.size();
    return std::nullopt;
}

static bool string_token_has_format_prefix(const std::vector<Tok> &toks, int string_idx) {
    int i = string_idx - 1;
    bool found = false;
    while (i >= 2) {
        if (toks[(size_t)i].type != TOK_RBRACKET) break;
        if (toks[(size_t)(i - 1)].type != TOK_ID) break;
        if (toks[(size_t)(i - 2)].type != TOK_LBRACKET) break;
        if (toks[(size_t)(i - 1)].text == "f" || toks[(size_t)(i - 1)].text == "format") found = true;
        i -= 3;
    }
    return found;
}

static bool is_semantic_operator_token(TokenType t) {
    return is_op(t) || t == TOK_DOT || t == TOK_LPAREN || t == TOK_RPAREN ||
           t == TOK_LBRACKET || t == TOK_RBRACKET || t == TOK_LBRACE ||
           t == TOK_RBRACE || t == TOK_COMMA || t == TOK_SEMI;
}

static void emit_format_string_interpolation_tokens(const Analysis &an,
                                                    std::string_view current_unit,
                                                    const std::string &text,
                                                    const std::vector<size_t> &line_starts,
                                                    const Tok &string_tok,
                                                    std::vector<SemTokSpan> &out) {
    const int line0 = std::max(0, string_tok.line - 1);
    const int base_col = std::max(0, string_tok.col - 1);
    int depth = 0;
    size_t seg_start = 0;
    size_t expr_start = std::string::npos;

    auto emit_span = [&](size_t from, size_t to, int sem_type, int mods = 0) {
        if (to <= from) return;
        auto sp = utf16_span_from_utf8_line_span(text, line_starts, line0,
                                                 base_col + (int)from, (int)(to - from));
        out.push_back(SemTokSpan{line0, sp.first, sp.second, sem_type, mods});
    };

    auto emit_expr = [&](size_t from, size_t to) {
        if (to <= from) return;
        std::string expr = string_tok.text.substr(from, to - from);
        std::vector<Tok> expr_toks = lex_all(expr);
        for (size_t i = 0; i < expr_toks.size(); ++i) {
            const Tok &tok = expr_toks[i];
            TokenType prev = (i > 0) ? expr_toks[i - 1].type : TOK_EOF;
            TokenType next = (i + 1 < expr_toks.size()) ? expr_toks[i + 1].type : TOK_EOF;

            int sem_type = -1;
            int mods = 0;

            if (tok.type == TOK_ID) {
                if (prev != TOK_DOT &&
                    (tok.text == "Meta" || tok.text == "On" || tok.text == "Keep" || tok.text == "Repeatable" ||
                     is_builtin_meta_attr(tok.text) || is_string_prefix_name(tok.text))) {
                    sem_type = 12;
                } else if (prev == TOK_AT) {
                    sem_type = 12;
                } else if (prev != TOK_DOT) {
                    auto alias_it = an.import_alias_to_module.find(tok.text);
                    if (alias_it != an.import_alias_to_module.end()) {
                        sem_type = 0;
                        if (kn_std_module_has(alias_it->second.c_str())) mods |= 1 << 2;
                    } else if (auto top = project_visible_top_level_decl_in_analysis(an, current_unit, tok.text)) {
                        sem_type = sem_type_for_kind(top->kind);
                        if (top->is_static) mods |= 1 << 1;
                    }
                }

                if (sem_type < 0) {
                    if (prev == TOK_DOT && next == TOK_LPAREN) sem_type = 8;
                    else if (prev == TOK_DOT) sem_type = 11;
                    else if (next == TOK_LPAREN) sem_type = 7;
                    else sem_type = 9;
                }
            } else if (is_kw(tok.type)) {
                sem_type = 12;
            } else if (is_type_kw(tok.type)) {
                sem_type = 1;
            } else if (tok.type == TOK_STRING || tok.type == TOK_BAD_STRING || tok.type == TOK_CHAR_LIT || tok.type == TOK_BAD_CHAR) {
                sem_type = 13;
            } else if (tok.type == TOK_NUMBER) {
                sem_type = 14;
            } else if (is_semantic_operator_token(tok.type)) {
                sem_type = 15;
            }

            if (sem_type < 0) continue;
            auto sp = utf16_span_from_utf8_line_span(text, line_starts, line0,
                                                     base_col + (int)from + std::max(0, tok.col - 1),
                                                     std::max(1, tok.len));
            out.push_back(SemTokSpan{line0, sp.first, sp.second, sem_type, mods});
        }
    };

    for (size_t ci = 1; ci + 1 < string_tok.text.size(); ++ci) {
        char cc = string_tok.text[ci];
        if (depth == 0) {
            if (cc == '\\') {
                ++ci;
                continue;
            }
            if (cc == '{' && ci + 1 < string_tok.text.size() && string_tok.text[ci + 1] == '{') {
                ++ci;
                continue;
            }
            if (cc == '}' && ci + 1 < string_tok.text.size() && string_tok.text[ci + 1] == '}') {
                ++ci;
                continue;
            }
            if (cc == '{') {
                emit_span(seg_start, ci, 13);
                emit_span(ci, ci + 1, 15);
                depth = 1;
                expr_start = ci + 1;
                seg_start = ci + 1;
            }
            continue;
        }

        if (cc == '{') {
            ++depth;
            continue;
        }
        if (cc == '}') {
            --depth;
            if (depth == 0) {
                emit_expr(expr_start, ci);
                emit_span(ci, ci + 1, 15);
                seg_start = ci + 1;
                expr_start = std::string::npos;
            }
        }
    }

    emit_span(seg_start, string_tok.text.size(), 13);
}

static std::optional<std::pair<std::string, std::string>> interpolation_prefix_at(const Doc &d, int line, int ch) {
    const Analysis &an = d.an;
    int ti = tok_at_pos(an.toks, line, ch);
    if (ti < 0) ti = tok_before_pos(an.toks, line, ch);
    if (ti < 0) return std::nullopt;
    const Tok &tok = an.toks[(size_t)ti];
    if (!(tok.type == TOK_STRING || tok.type == TOK_BAD_STRING)) return std::nullopt;
    if (!string_token_has_format_prefix(an.toks, ti)) return std::nullopt;

    auto abs = offset_for_line_col_simple(d.text, line, ch);
    auto tok_abs = offset_for_line_col_simple(d.text, tok.line - 1, tok.col - 1);
    if (!abs || !tok_abs) return std::nullopt;
    if (*abs < *tok_abs) return std::nullopt;
    size_t rel = *abs - *tok_abs;
    if (rel > tok.text.size()) rel = tok.text.size();

    int depth = 0;
    size_t seg_start = std::string::npos;
    for (size_t i = 1; i < rel && i < tok.text.size(); ++i) {
        char c = tok.text[i];
        char n1 = (i + 1 < tok.text.size()) ? tok.text[i + 1] : 0;
        if (c == '\\') {
            i++;
            continue;
        }
        if (c == '{' && n1 == '{') {
            i++;
            continue;
        }
        if (c == '}' && n1 == '}') {
            i++;
            continue;
        }
        if (c == '{') {
            depth++;
            if (depth == 1) seg_start = i + 1;
            continue;
        }
        if (c == '}') {
            if (depth > 0) depth--;
            if (depth == 0) seg_start = std::string::npos;
        }
    }
    if (depth <= 0 || seg_start == std::string::npos || seg_start > rel) return std::nullopt;

    std::string fragment = tok.text.substr(seg_start, rel - seg_start);
    size_t start = fragment.size();
    while (start > 0) {
        char c = fragment[start - 1];
        if (is_ident_char_simple(c) || c == '.') start--;
        else break;
    }
    std::string chain = fragment.substr(start);
    size_t dot = chain.rfind('.');
    if (dot == std::string::npos) return std::make_pair(std::string{}, chain);
    return std::make_pair(chain.substr(0, dot), chain.substr(dot + 1));
}

static void add_global_completion_items(const Analysis &an,
                                        std::vector<Completion> &out,
                                        std::set<std::pair<std::string, int>> &seen,
                                        std::string_view label_prefix) {
    for (const Completion &c0 : an.completions) {
        if (!starts_with(c0.label, label_prefix)) continue;
        add_unique(out, seen, c0);
    }

    for (const auto &kv : an.import_alias_to_module) {
        if (!starts_with(kv.first, label_prefix)) continue;
        Completion c;
        c.label = kv.first;
        c.kind = 9;
        c.detail = kv.second;
        add_unique(out, seen, std::move(c));
    }

    for (const auto &kv : an.import_symbol_to_qualified) {
        if (!starts_with(kv.first, label_prefix)) continue;
        Completion c;
        c.label = kv.first;
        if (auto top = project_qualified_top_level_decl(kv.second)) {
            c.kind = completion_kind_for(top->kind);
            c.detail = decl_completion_detail(*top);
        } else {
            c.kind = 3;
            c.detail = kv.second;
        }
        add_unique(out, seen, std::move(c));
    }

    std::string cur_unit = first_declared_unit_name(index_units_from_tokens(an.toks));
    for (const DeclInfo &dcl : project_top_level_decls_for_module(cur_unit)) {
        if (is_hidden_internal_name(dcl.name)) continue;
        if (!starts_with(dcl.name, label_prefix)) continue;
        Completion c;
        c.label = dcl.name;
        c.kind = completion_kind_for(dcl.kind);
        c.detail = decl_completion_detail(dcl);
        add_unique(out, seen, std::move(c));
    }

    if (starts_with("Object", label_prefix)) {
        Completion c;
        c.label = "Object";
        c.kind = 9;
        c.detail = "IO.Type.Object";
        add_unique(out, seen, std::move(c));
    }

    for (const std::string &seg : project_child_unit_segments(std::string_view{})) {
        if (!starts_with(seg, label_prefix)) continue;
        Completion c;
        c.label = seg;
        c.kind = 9;
        c.detail = "namespace";
        add_unique(out, seen, std::move(c));
    }
}

static int token_before_or_at_for_prefix(const Analysis &an, int line, int ch, std::string &prefix) {
    int at = tok_at_pos(an.toks, line, ch);
    if (at >= 0 && an.toks[(size_t)at].type == TOK_ID) {
        prefix = tok_prefix_at(an.toks[(size_t)at], line, ch);
        return at;
    }
    int before = tok_before_pos(an.toks, line, ch);
    if (before >= 0 && an.toks[(size_t)before].type == TOK_ID) {
        Range r = tok_range(an.toks[(size_t)before]);
        if (r.el == line && r.ec == ch) prefix = an.toks[(size_t)before].text;
    }
    return before;
}

static void add_type_completion_items(const Doc & /*d*/,
                                      const Analysis &an,
                                      std::vector<Completion> &out,
                                      std::set<std::pair<std::string, int>> &seen,
                                      std::string_view label_prefix) {
    static const char *type_kws[] = {
        "void", "bool", "byte", "char", "int", "float",
        "f16", "f32", "f64", "f128",
        "i8", "i16", "i32", "i64", "u8", "u16", "u32", "u64",
        "isize", "usize", "string", "any", "list", "dict", "set", "null"
    };
    for (const char *kw : type_kws) {
        if (!starts_with(kw, label_prefix)) continue;
        Completion c;
        c.label = kw;
        c.kind = 14;
        c.detail = "type keyword";
        add_unique(out, seen, std::move(c));
    }

    for (const Symbol &s : an.syms) {
        if (is_hidden_internal_name(s.name)) continue;
        if (!(s.kind == SymKind::Class || s.kind == SymKind::Struct || s.kind == SymKind::Interface || s.kind == SymKind::Enum)) continue;
        if (!starts_with(s.name, label_prefix)) continue;
        Completion c;
        c.label = s.name;
        c.kind = completion_kind_for(s.kind);
        switch (s.kind) {
        case SymKind::Class: c.detail = "class"; break;
        case SymKind::Struct: c.detail = "struct"; break;
        case SymKind::Interface: c.detail = "interface"; break;
        case SymKind::Enum: c.detail = "enum"; break;
        default: c.detail = "type"; break;
        }
        add_unique(out, seen, std::move(c));
    }

    std::string cur_unit = first_declared_unit_name(index_units_from_tokens(an.toks));
    for (const DeclInfo &dcl : project_top_level_decls_for_module(cur_unit)) {
        if (is_hidden_internal_name(dcl.name)) continue;
        if (!(dcl.kind == SymKind::Class || dcl.kind == SymKind::Struct || dcl.kind == SymKind::Interface || dcl.kind == SymKind::Enum)) continue;
        if (!starts_with(dcl.name, label_prefix)) continue;
        Completion c;
        c.label = dcl.name;
        c.kind = completion_kind_for(dcl.kind);
        c.detail = decl_completion_detail(dcl);
        add_unique(out, seen, std::move(c));
    }

    for (const auto &kv : an.import_symbol_to_qualified) {
        if (!starts_with(kv.first, label_prefix)) continue;
        if (auto top = project_qualified_top_level_decl(kv.second)) {
            if (!(top->kind == SymKind::Class || top->kind == SymKind::Struct || top->kind == SymKind::Interface || top->kind == SymKind::Enum)) continue;
            Completion c;
            c.label = kv.first;
            c.kind = completion_kind_for(top->kind);
            c.detail = decl_completion_detail(*top);
            add_unique(out, seen, std::move(c));
        }
    }
}

static void add_unit_name_completion_items(const Analysis &an,
                                           std::vector<Completion> &out,
                                           std::set<std::pair<std::string, int>> &seen,
                                           std::string_view label_prefix) {
    for (const std::string &seg : project_child_unit_segments(std::string_view{})) {
        if (!starts_with(seg, label_prefix)) continue;
        Completion c;
        c.label = seg;
        c.kind = 9;
        c.detail = "namespace";
        add_unique(out, seen, std::move(c));
    }
    for (const auto &kv : an.import_alias_to_module) {
        if (!starts_with(kv.first, label_prefix)) continue;
        Completion c;
        c.label = kv.first;
        c.kind = 9;
        c.detail = kv.second;
        add_unique(out, seen, std::move(c));
    }
}

static void add_string_prefix_completion_items(std::vector<Completion> &out,
                                               std::set<std::pair<std::string, int>> &seen,
                                               std::string_view label_prefix) {
    static const char *string_prefixes[] = {"r", "raw", "f", "format", "c", "chars"};
    for (const char *kw : string_prefixes) {
        if (!starts_with(kw, label_prefix)) continue;
        Completion c;
        c.label = kw;
        c.kind = 14;
        c.detail = "string prefix";
        add_unique(out, seen, std::move(c));
    }
}

static void add_identifier_snippet(std::vector<Completion> &out,
                                   std::set<std::pair<std::string, int>> &seen,
                                   std::string_view label,
                                   std::string_view detail,
                                   std::string_view snippet) {
    Completion c;
    c.label = std::string(label);
    c.kind = 6;
    c.detail = std::string(detail);
    c.insert_text = std::string(snippet);
    c.insert_text_format = 2;
    add_unique(out, seen, std::move(c));
}

static bool is_string_prefix_context(const Analysis &an, int line, int ch, std::string &label_prefix) {
    int idx = token_before_or_at_for_prefix(an, line, ch, label_prefix);
    if (idx < 0) return false;
    int i = idx;
    if (an.toks[(size_t)i].type != TOK_ID) return false;
    if (i <= 0 || an.toks[(size_t)(i - 1)].type != TOK_LBRACKET) return false;
    int j = i + 1;
    if (j < (int)an.toks.size() && an.toks[(size_t)j].type == TOK_RBRACKET) j++;
    while (j + 2 < (int)an.toks.size() &&
           an.toks[(size_t)j].type == TOK_LBRACKET &&
           an.toks[(size_t)(j + 1)].type == TOK_ID &&
           an.toks[(size_t)(j + 2)].type == TOK_RBRACKET) {
        j += 3;
    }
    return j < (int)an.toks.size() &&
           (an.toks[(size_t)j].type == TOK_STRING || an.toks[(size_t)j].type == TOK_BAD_STRING);
}

static bool is_unit_name_context(const Analysis &an, int line, int ch, std::string &label_prefix) {
    int idx = token_before_or_at_for_prefix(an, line, ch, label_prefix);
    if (idx < 0) return false;
    if (an.toks[(size_t)idx].type == TOK_ID && idx > 0 && an.toks[(size_t)(idx - 1)].type == TOK_UNIT) return true;
    if (an.toks[(size_t)idx].type == TOK_DOT && idx > 0) {
        int start = idx - 1;
        while (start >= 1 && an.toks[(size_t)start].type == TOK_ID && an.toks[(size_t)(start - 1)].type == TOK_DOT) start -= 2;
        return start >= 1 && an.toks[(size_t)(start - 1)].type == TOK_UNIT;
    }
    return idx >= 0 && an.toks[(size_t)idx].type == TOK_UNIT;
}

static bool is_decl_name_context(const Analysis &an, int line, int ch, std::string &label_prefix, std::string &snippet) {
    int idx = token_before_or_at_for_prefix(an, line, ch, label_prefix);
    if (idx < 0) return false;
    if (an.toks[(size_t)idx].type == TOK_CLASS || an.toks[(size_t)idx].type == TOK_STRUCT ||
        an.toks[(size_t)idx].type == TOK_INTERFACE || an.toks[(size_t)idx].type == TOK_ENUM ||
        (an.toks[(size_t)idx].type == TOK_ID && an.toks[(size_t)idx].text == "Meta")) {
        snippet = "${1:Name}";
        label_prefix.clear();
        return true;
    }
    if (an.toks[(size_t)idx].type == TOK_ID && idx > 0) {
        TokenType prev = an.toks[(size_t)(idx - 1)].type;
        if (prev == TOK_CLASS || prev == TOK_STRUCT || prev == TOK_INTERFACE || prev == TOK_ENUM ||
            (prev == TOK_ID && an.toks[(size_t)(idx - 1)].text == "Meta")) {
            snippet = "${1:Name}";
            return true;
        }
    }
    if (an.toks[(size_t)idx].type == TOK_TYPE_VOID || is_type_kw(an.toks[(size_t)idx].type) || an.toks[(size_t)idx].type == TOK_ID) {
        int p = idx - 1;
        while (p >= 0 && is_decl_modifier(an.toks[(size_t)p].type)) p--;
        if (p >= 0 && an.toks[(size_t)p].type == TOK_FUNCTION) {
            snippet = "${1:Name}(${2})";
            return true;
        }
    }
    return false;
}

static bool is_type_context(const Analysis &an, int line, int ch, std::string &label_prefix) {
    int idx = token_before_or_at_for_prefix(an, line, ch, label_prefix);
    if (idx < 0) return false;
    TokenType cur = an.toks[(size_t)idx].type;
    if (cur == TOK_FUNCTION || cur == TOK_CONSTRUCTOR || cur == TOK_COLON || cur == TOK_NEW) {
        label_prefix.clear();
        return cur != TOK_CONSTRUCTOR;
    }
    if ((cur == TOK_LPAREN || cur == TOK_COMMA) && idx > 0) {
        for (int i = idx - 1; i >= 0; --i) {
            TokenType t = an.toks[(size_t)i].type;
            if (t == TOK_FUNCTION || t == TOK_CONSTRUCTOR) {
                label_prefix.clear();
                return true;
            }
            if (t == TOK_SEMI || t == TOK_LBRACE || t == TOK_RBRACE) break;
        }
    }
    if (cur == TOK_ID && idx > 0) {
        TokenType prev = an.toks[(size_t)(idx - 1)].type;
        if (prev == TOK_FUNCTION || prev == TOK_COLON || prev == TOK_NEW) return true;
        if (prev == TOK_LPAREN || prev == TOK_COMMA) {
            for (int i = idx - 2; i >= 0; --i) {
                TokenType t = an.toks[(size_t)i].type;
                if (t == TOK_FUNCTION || t == TOK_CONSTRUCTOR) return true;
                if (t == TOK_SEMI || t == TOK_LBRACE || t == TOK_RBRACE) break;
            }
        }
    }
    return false;
}

static bool is_cast_type_context(const Analysis &an, int line, int ch, std::string &label_prefix) {
    int idx = token_before_or_at_for_prefix(an, line, ch, label_prefix);
    if (idx < 0) return false;
    int lb = idx;
    if (an.toks[(size_t)lb].type != TOK_LBRACKET) {
        if (lb > 0 && an.toks[(size_t)(lb - 1)].type == TOK_LBRACKET) lb--;
        else return false;
    }
    int rb = -1;
    int depth = 0;
    for (int i = lb; i < (int)an.toks.size(); ++i) {
        if (an.toks[(size_t)i].type == TOK_LBRACKET) depth++;
        else if (an.toks[(size_t)i].type == TOK_RBRACKET) {
            depth--;
            if (depth == 0) { rb = i; break; }
        }
    }
    if (rb < 0 || rb + 1 >= (int)an.toks.size() || an.toks[(size_t)(rb + 1)].type != TOK_LPAREN) return false;
    return idx >= lb && idx <= rb;
}

std::vector<Completion> completion_items_at(const Doc &d, int line, int ch) {
    const Analysis &an = d.an;
    std::vector<Completion> out;
    std::set<std::pair<std::string, int>> seen;

    if (auto interp = interpolation_prefix_at(d, line, ch)) {
        const std::string &qual = interp->first;
        const std::string &label_prefix = interp->second;
        if (!qual.empty()) {
            std::string module = qual;
            auto it_alias = an.import_alias_to_module.find(qual);
            if (it_alias != an.import_alias_to_module.end()) module = it_alias->second;

            if (kn_std_module_has(module.c_str())) {
                add_std_funcs(out, seen, module, label_prefix);
                if (!out.empty()) return out;
            }

            add_builtin_object_children(out, seen, module, label_prefix);
            if (!out.empty()) return out;
            add_std_child_modules(out, seen, module, label_prefix);
            add_info_constant_members(out, seen, module, label_prefix);
            if (!out.empty()) return out;

            for (const std::string &seg : project_child_unit_segments(module)) {
                if (!starts_with(seg, label_prefix)) continue;
                Completion c;
                c.label = seg;
                c.kind = 9;
                c.detail = "namespace";
                add_unique(out, seen, std::move(c));
            }

            for (const DeclInfo &dcl : project_top_level_decls_for_module(module)) {
                if (is_hidden_internal_name(dcl.name)) continue;
                if (!starts_with(dcl.name, label_prefix)) continue;
                Completion c;
                c.label = dcl.name;
                c.kind = completion_kind_for(dcl.kind);
                c.detail = decl_completion_detail(dcl);
                add_unique(out, seen, std::move(c));
            }
            if (!out.empty()) return out;

            std::string base = qual;
            for (const Symbol &s : an.syms) {
                if (s.name == qual) {
                    if (kind_is_type(s.kind)) base = s.name;
                    else if (!s.type_text.empty()) base = normalize_base_type(s.type_text);
                    break;
                }
            }
            if (base.find('.') == std::string::npos) {
                if (auto top = project_visible_top_level_decl(d, base)) base = top->qname;
            }

            std::string std_m = std_module_for_receiver(base);
            if (kn_std_module_has(std_m.c_str())) {
                add_std_funcs(out, seen, std_m, label_prefix);
                return out;
            }
            if (is_block_type_name(base)) {
                add_block_methods(out, seen, label_prefix);
                return out;
            }

            for (const DeclInfo &dcl : project_member_decls_for_owner(base)) {
                if (is_hidden_internal_name(dcl.name)) continue;
                if (!starts_with(dcl.name, label_prefix)) continue;
                Completion c;
                c.label = dcl.name;
                c.kind = completion_kind_for(dcl.kind);
                c.detail = decl_completion_detail(dcl);
                add_unique(out, seen, std::move(c));
            }
            if (!out.empty()) return out;
        }

        add_global_completion_items(an, out, seen, label_prefix);
        return out;
    }

    std::string ctx_prefix;
    if (is_string_prefix_context(an, line, ch, ctx_prefix)) {
        add_string_prefix_completion_items(out, seen, ctx_prefix);
        return out;
    }

    if (is_cast_type_context(an, line, ch, ctx_prefix)) {
        add_type_completion_items(d, an, out, seen, ctx_prefix);
        return out;
    }

    if (is_unit_name_context(an, line, ch, ctx_prefix)) {
        add_unit_name_completion_items(an, out, seen, ctx_prefix);
        return out;
    }

    std::string decl_snippet;
    if (is_decl_name_context(an, line, ch, ctx_prefix, decl_snippet)) {
        add_identifier_snippet(out, seen, "Name", "identifier", decl_snippet);
        return out;
    }

    if (is_type_context(an, line, ch, ctx_prefix)) {
        add_type_completion_items(d, an, out, seen, ctx_prefix);
        return out;
    }

    int at = tok_at_pos(an.toks, line, ch);
    int before = tok_before_pos(an.toks, line, ch);

    int dot_i = -1;
    std::string label_prefix;

    if (at >= 0 && an.toks[(size_t)at].type == TOK_ID && at > 0 && an.toks[(size_t)(at - 1)].type == TOK_DOT) {
        dot_i = at - 1;
        label_prefix = tok_prefix_at(an.toks[(size_t)at], line, ch);
    } else if (before >= 0 && an.toks[(size_t)before].type == TOK_DOT) {
        dot_i = before;
    } else if (before >= 0 && an.toks[(size_t)before].type == TOK_ID && before > 0 &&
               an.toks[(size_t)(before - 1)].type == TOK_DOT) {
        dot_i = before - 1;
        label_prefix = an.toks[(size_t)before].text;
    }

    if (dot_i >= 0) {
        std::string qual = qualifier_before_dot(an.toks, dot_i);
        if (qual.empty()) return out;

        std::string module = qual;
        auto it_alias = an.import_alias_to_module.find(qual);
        if (it_alias != an.import_alias_to_module.end()) module = it_alias->second;

        if (kn_std_module_has(module.c_str())) {
            add_std_funcs(out, seen, module, label_prefix);
            if (!out.empty()) return out;
        }

        // Module prefix completion (e.g., IO. -> Console/Time/Type)
        add_builtin_object_children(out, seen, module, label_prefix);
        if (!out.empty()) return out;
        add_std_child_modules(out, seen, module, label_prefix);
        add_info_constant_members(out, seen, module, label_prefix);
        if (!out.empty()) return out;

        for (const std::string &seg : project_child_unit_segments(module)) {
            if (!starts_with(seg, label_prefix)) continue;
            Completion c;
            c.label = seg;
            c.kind = 9;
            c.detail = "namespace";
            add_unique(out, seen, std::move(c));
        }

        for (const DeclInfo &d : project_top_level_decls_for_module(module)) {
            if (is_hidden_internal_name(d.name)) continue;
            if (!starts_with(d.name, label_prefix)) continue;
            Completion c;
            c.label = d.name;
            c.kind = completion_kind_for(d.kind);
            c.detail = decl_completion_detail(d);
            add_unique(out, seen, std::move(c));
        }
        if (!out.empty()) return out;

        // Receiver/member completion based on local symbol type.
        const Tok &recv_tok = an.toks[(size_t)std::max(0, dot_i - 1)];
        const Symbol *recv_sym = sym_for_token(an, recv_tok.idx);
        std::string base;
        if (recv_sym) {
            if (kind_is_type(recv_sym->kind)) base = recv_sym->name;
            else if (!recv_sym->type_text.empty()) base = normalize_base_type(recv_sym->type_text);
        }
        if (base.empty()) base = qual;
        if (base.find('.') == std::string::npos) {
            if (auto top = project_visible_top_level_decl(d, base)) base = top->qname;
        }

        std::string std_m = std_module_for_receiver(base);
        if (kn_std_module_has(std_m.c_str())) {
            add_std_funcs(out, seen, std_m, label_prefix);
            return out;
        }
        if (is_block_type_name(base)) {
            add_block_methods(out, seen, label_prefix);
            return out;
        }

        for (const DeclInfo &d : project_member_decls_for_owner(base)) {
            if (is_hidden_internal_name(d.name)) continue;
            if (!starts_with(d.name, label_prefix)) continue;
            Completion c;
            c.label = d.name;
            c.kind = completion_kind_for(d.kind);
            c.detail = decl_completion_detail(d);
            add_unique(out, seen, std::move(c));
        }
        if (!out.empty()) return out;

        std::string container = last_segment(base);
        for (const Symbol &s : an.syms) {
            if (is_hidden_internal_name(s.name)) continue;
            if (s.container != container) continue;
            if (!(s.kind == SymKind::Method || s.kind == SymKind::Property || s.kind == SymKind::Field || s.kind == SymKind::EnumMember)) continue;
            if (!starts_with(s.name, label_prefix)) continue;
            Completion c;
            c.label = s.name;
            c.kind = completion_kind_for(s.kind);
            if (s.kind == SymKind::Method) {
                std::ostringstream det;
                det << (s.ret_text.empty() ? "unknown" : s.ret_text) << " " << s.name << "(";
                for (size_t i = 0; i < s.params.size(); ++i) {
                    if (i) det << ", ";
                    det << s.params[i].first;
                }
                det << ")";
                c.detail = det.str();
            } else if (!s.type_text.empty()) {
                c.detail = s.type_text;
            }
            add_unique(out, seen, std::move(c));
        }
        return out;
    }

    // Global-ish completion.
    if (at >= 0 && an.toks[(size_t)at].type == TOK_ID) {
        label_prefix = tok_prefix_at(an.toks[(size_t)at], line, ch);
    } else if (before >= 0 && an.toks[(size_t)before].type == TOK_ID) {
        Range r = tok_range(an.toks[(size_t)before]);
        if (r.el == line && r.ec == ch) label_prefix = an.toks[(size_t)before].text;
    }

    add_global_completion_items(an, out, seen, label_prefix);

    return out;
}

std::string completion_json_at(const Doc &d, int line, int ch) {
    return completion_list_json(completion_items_at(d, line, ch));
}

std::string completion_json(const Doc &d) { return completion_list_json(d.an.completions); }

std::string sym_sig(const Symbol &s) {
    std::ostringstream oss;
    auto ty = [&](std::string_view t) { return t.empty() ? std::string("unknown") : std::string(t); };
    switch (s.kind) {
    case SymKind::Function:
        oss << ty(s.ret_text) << " " << s.name << "(";
        for (size_t i = 0; i < s.params.size(); ++i) {
            if (i) oss << ", ";
            oss << s.params[i].first << " " << s.params[i].second;
        }
        oss << ")";
        return oss.str();
    case SymKind::Method:
        oss << ty(s.ret_text) << " " << (s.container.empty() ? "" : (s.container + ".")) << s.name << "(";
        for (size_t i = 0; i < s.params.size(); ++i) {
            if (i) oss << ", ";
            oss << s.params[i].first << " " << s.params[i].second;
        }
        oss << ")";
        return oss.str();
    case SymKind::Property:
        oss << "Property " << ty(s.type_text) << " " << (s.container.empty() ? "" : (s.container + ".")) << s.name;
        return oss.str();
    case SymKind::Field:
    case SymKind::Variable:
    case SymKind::Parameter:
        oss << ty(s.type_text) << " " << (s.container.empty() ? "" : (s.container + ".")) << s.name;
        return oss.str();
    case SymKind::Class: return "Class " + s.name;
    case SymKind::Struct: return "Struct " + s.name;
    case SymKind::Interface: return "Interface " + s.name;
    case SymKind::Enum:
        return s.type_text.empty() ? ("Enum " + s.name) : ("Enum " + s.name + " By " + s.type_text);
    case SymKind::EnumMember:
        if (!s.container.empty()) {
            oss << "Enum " << s.container;
            if (!s.type_text.empty()) oss << " By " << s.type_text;
            oss << "\n" << s.name;
            if (s.has_const_value) oss << " = " << s.const_value;
            return oss.str();
        }
        oss << "EnumMember " << s.name;
        if (s.has_const_value) oss << " = " << s.const_value;
        return oss.str();
    }
    return s.name;
}

std::string decl_sig(const DeclInfo &d) {
    std::ostringstream oss;
    auto ty = [&](std::string_view t) { return t.empty() ? std::string("unknown") : std::string(t); };
    switch (d.kind) {
    case SymKind::Function:
    case SymKind::Method:
        oss << ty(d.ret_text) << " " << d.name << "(";
        for (size_t i = 0; i < d.params.size(); ++i) {
            if (i) oss << ", ";
            oss << d.params[i].first << " " << d.params[i].second;
        }
        oss << ")";
        return oss.str();
    case SymKind::Property:
        oss << "Property " << ty(d.type_text) << " " << d.name;
        return oss.str();
    case SymKind::Field:
    case SymKind::Variable:
    case SymKind::Parameter:
        oss << ty(d.type_text) << " " << d.name;
        return oss.str();
    case SymKind::Class: return "Class " + d.name;
    case SymKind::Struct: return "Struct " + d.name;
    case SymKind::Interface: return "Interface " + d.name;
    case SymKind::Enum:
        return d.type_text.empty() ? ("Enum " + d.name) : ("Enum " + d.name + " By " + d.type_text);
    case SymKind::EnumMember:
        oss << "Enum " << (d.container.empty() ? "?" : d.container);
        if (!d.type_text.empty()) oss << " By " << d.type_text;
        oss << "\n" << d.name;
        if (d.has_const_value) oss << " = " << d.const_value;
        return oss.str();
    }
    return d.name;
}

static std::string hover_markdown_codeblock(std::string_view code) {
    std::string md;
    md.reserve(code.size() + 16);
    md += "```kinal\n";
    md.append(code.data(), code.size());
    md += "\n```";
    std::ostringstream oss;
    oss << "{\"contents\":{\"kind\":\"markdown\",\"value\":\"" << esc(md) << "\"}}";
    return oss.str();
}

std::string hover_json_at(const Doc &d, int line, int ch) {
    // 1) Prefer resolved user symbols.
    if (auto h = hit_at(d, line, ch)) {
        if (const Symbol *s = sym_by_id(d.an, h->sym_id)) {
            return hover_markdown_codeblock(sym_sig(*s));
        }
    }

    // 2) Try stdlib symbol hover (qualified name or imported/opened symbols).
    int ti = tok_at_pos(d.an.toks, line, ch);
    if (ti < 0) ti = tok_before_pos(d.an.toks, line, ch);
    if (ti < 0 || ti >= (int)d.an.toks.size()) return "null";

    if (!qual_segment_tok(d.an.toks[(size_t)ti].type) && d.an.toks[(size_t)ti].type == TOK_DOT && ti > 0)
        ti--;
    if (ti < 0 || !qual_segment_tok(d.an.toks[(size_t)ti].type)) return "null";

    int l = ti;
    while (l >= 2 && d.an.toks[(size_t)(l - 1)].type == TOK_DOT && qual_segment_tok(d.an.toks[(size_t)(l - 2)].type))
        l -= 2;
    int r = ti;
    while (r + 2 < (int)d.an.toks.size() && d.an.toks[(size_t)(r + 1)].type == TOK_DOT && qual_segment_tok(d.an.toks[(size_t)(r + 2)].type))
        r += 2;

    std::string q = join_tok_text(d.an.toks, l, r);
    std::string module = prefix_before_last_segment(q);
    std::string name = last_segment(q);

    if (module.empty()) {
        auto it = d.an.import_symbol_to_qualified.find(name);
        if (it != d.an.import_symbol_to_qualified.end()) {
            module = prefix_before_last_segment(it->second);
            name = last_segment(it->second);
        }
    }

    const KnStdFunc *sf = (!module.empty()) ? kn_std_find(module.c_str(), name.c_str()) : nullptr;
    if (sf) {
        const bool omit_recv = std_module_is_method_like(module);
        return hover_markdown_codeblock(stdfunc_sig(*sf, module, omit_recv));
    }

    if (auto h = project_hover_text_at(d, line, ch)) return hover_markdown_codeblock(*h);
    return "null";
}

std::string sem_json(const Doc &d) {
    std::ostringstream oss;
    oss << "{\"data\":[";
    for (size_t i = 0; i < d.an.sem_data.size(); ++i) {
        if (i) oss << ",";
        oss << d.an.sem_data[i];
    }
    oss << "]}";
    return oss.str();
}

std::string init_json() {
    static const char *types[] = {
        "namespace", "type", "class", "struct", "interface", "enum", "enumMember",
        "function", "method", "variable", "parameter", "property", "keyword", "string", "number", "operator",
        "comment"
    };
    static const char *mods[] = {"declaration", "static", "defaultLibrary", "readonly", "deprecated"};

    std::ostringstream oss;
    oss << "{";
    oss << "\"capabilities\":{";
    oss << "\"textDocumentSync\":1,";
    oss << "\"completionProvider\":{\"resolveProvider\":false,\"triggerCharacters\":[\".\",\":\",\"[\"]},";
    oss << "\"codeActionProvider\":true,";
    oss << "\"hoverProvider\":true,";
    oss << "\"definitionProvider\":true,";
    oss << "\"referencesProvider\":true,";
    oss << "\"documentSymbolProvider\":true,";
    oss << "\"semanticTokensProvider\":{";
    oss << "\"legend\":{";
    oss << "\"tokenTypes\":[";
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); ++i) {
        if (i) oss << ",";
        oss << "\"" << types[i] << "\"";
    }
    oss << "],\"tokenModifiers\":[";
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); ++i) {
        if (i) oss << ",";
        oss << "\"" << mods[i] << "\"";
    }
    oss << "]},\"full\":true}";
    oss << "},";
    oss << "\"serverInfo\":{\"name\":\"kinal-lsp-server\",\"version\":\"" << KN_VERSION_STRING << "\"}";
    oss << "}";
    return oss.str();
}

std::vector<size_t> line_offsets(const std::string &text) {
    std::vector<size_t> off;
    off.push_back(0);
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\n') off.push_back(i + 1);
    }
    return off;
}

std::optional<size_t> pos_to_off(const std::string &text, int line, int ch) {
    if (line < 0 || ch < 0) return std::nullopt;
    auto offs = line_offsets(text);
    if (line >= (int)offs.size()) return std::nullopt;
    size_t st = offs[(size_t)line];
    size_t ed = text.size();
    if (line + 1 < (int)offs.size()) ed = offs[(size_t)(line + 1)];
    size_t off = st + (size_t)ch;
    if (off > ed) return std::nullopt;
    return off;
}

std::optional<Range> json_to_range(const Json *rv) {
    if (!rv || rv->kind != Json::Kind::Object) return std::nullopt;
    const Json *sv = rv->find("start");
    const Json *ev = rv->find("end");
    if (!sv || !ev || sv->kind != Json::Kind::Object || ev->kind != Json::Kind::Object)
        return std::nullopt;
    Range r;
    r.sl = sv->find("line") ? sv->find("line")->i32(0) : 0;
    r.sc = sv->find("character") ? sv->find("character")->i32(0) : 0;
    r.el = ev->find("line") ? ev->find("line")->i32(0) : 0;
    r.ec = ev->find("character") ? ev->find("character")->i32(0) : 0;
    return r;
}

std::optional<std::string> slice_text(const std::string &text, int line, int ch, int len) {
    auto so = pos_to_off(text, line, ch);
    if (!so) return std::nullopt;
    size_t end = *so + (size_t)std::max(0, len);
    if (end > text.size()) return std::nullopt;
    return text.substr(*so, end - *so);
}

std::optional<std::pair<Range, std::string>> legacy_array_fix_at(const Doc &d, const Range &hint) {
    const std::vector<Tok> &toks = d.an.toks;
    if (toks.empty()) return std::nullopt;

    int ti = tok_at_pos(toks, hint.sl, hint.sc);
    if (ti < 0) ti = tok_before_pos(toks, hint.sl, hint.sc);
    if (ti < 0) return std::nullopt;
    if (toks[(size_t)ti].type != TOK_LBRACKET) {
        if (ti + 1 < (int)toks.size() && toks[(size_t)(ti + 1)].type == TOK_LBRACKET)
            ti++;
        else if (ti - 1 >= 0 && toks[(size_t)(ti - 1)].type == TOK_LBRACKET)
            ti--;
        else
            return std::nullopt;
    }

    int lb = ti;
    int rb = -1;
    int depth = 0;
    for (int i = lb; i < (int)toks.size(); ++i) {
        if (toks[(size_t)i].type == TOK_LBRACKET) depth++;
        else if (toks[(size_t)i].type == TOK_RBRACKET) {
            depth--;
            if (depth == 0) {
                rb = i;
                break;
            }
        }
    }
    if (rb < 0 || lb == 0) return std::nullopt;

    int name_i = lb - 1;
    if (toks[(size_t)name_i].type != TOK_ID || name_i == 0) return std::nullopt;

    int type_end = name_i - 1;
    int type_start = type_end;
    while (type_start >= 0) {
        TokenType tt = toks[(size_t)type_start].type;
        if (qual_segment_tok(tt) || tt == TOK_DOT || tt == TOK_STAR) {
            type_start--;
            continue;
        }
        break;
    }
    type_start++;
    if (type_start > type_end) return std::nullopt;

    auto type_text = slice_text(
        d.text,
        toks[(size_t)type_start].line - 1,
        toks[(size_t)type_start].col - 1,
        (toks[(size_t)type_end].col - toks[(size_t)type_start].col) + toks[(size_t)type_end].len);
    auto name_text = slice_text(d.text, toks[(size_t)name_i].line - 1, toks[(size_t)name_i].col - 1, toks[(size_t)name_i].len);
    auto bracket_text = slice_text(
        d.text,
        toks[(size_t)lb].line - 1,
        toks[(size_t)lb].col - 1,
        (toks[(size_t)rb].col - toks[(size_t)lb].col) + toks[(size_t)rb].len);
    if (!type_text || !name_text || !bracket_text) return std::nullopt;

    Range edit_range = range_from_tokens(toks[(size_t)type_start], toks[(size_t)rb]);
    return std::make_pair(edit_range, *type_text + *bracket_text + " " + *name_text);
}

std::string code_actions_json(const Doc &d, const Json *params) {
    if (!params || params->kind != Json::Kind::Object) return "[]";
    const Json *ctx = params->find("context");
    if (!ctx || ctx->kind != Json::Kind::Object) return "[]";
    const Json *diags = ctx->find("diagnostics");
    if (!diags || diags->kind != Json::Kind::Array) return "[]";

    std::set<std::string> seen;
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const Json &diag : diags->a) {
        if (diag.kind != Json::Kind::Object) continue;
        std::string code = diag.find("code") ? diag.find("code")->str() : "";
        if (code != "W-SYN-00001") continue;
        auto range = json_to_range(diag.find("range"));
        if (!range) continue;
        auto fix = legacy_array_fix_at(d, *range);
        if (!fix) continue;

        std::ostringstream key;
        key << fix->first.sl << ":" << fix->first.sc << ":" << fix->first.el << ":" << fix->first.ec << ":" << fix->second;
        if (!seen.insert(key.str()).second) continue;

        if (!first) oss << ",";
        first = false;
        oss << "{";
        oss << "\"title\":\"Convert to Type[] name\",";
        oss << "\"kind\":\"quickfix\",";
        oss << "\"isPreferred\":true,";
        oss << "\"edit\":{\"changes\":{\"" << esc(d.uri) << "\":[{";
        oss << "\"range\":" << range_json(fix->first) << ",";
        oss << "\"newText\":\"" << esc(fix->second) << "\"";
        oss << "}]}}";
        oss << "}";
    }
    oss << "]";
    return oss.str();
}

void apply_change(std::string &text, const Json &chg) {
    const Json *tv = chg.find("text");
    if (!tv || tv->kind != Json::Kind::String) return;
    const Json *rv = chg.find("range");
    if (!rv || rv->kind != Json::Kind::Object) {
        text = tv->s;
        return;
    }
    const Json *sv = rv->find("start");
    const Json *ev = rv->find("end");
    if (!sv || !ev) return;
    int sl = sv->find("line") ? sv->find("line")->i32(0) : 0;
    int sc = sv->find("character") ? sv->find("character")->i32(0) : 0;
    int el = ev->find("line") ? ev->find("line")->i32(0) : 0;
    int ec = ev->find("character") ? ev->find("character")->i32(0) : 0;
    auto so = pos_to_off(text, sl, sc);
    auto eo = pos_to_off(text, el, ec);
    if (!so || !eo || *so > *eo || *eo > text.size()) return;
    text.replace(*so, *eo - *so, tv->s);
}

void clear_diags(std::string_view uri) {
    std::ostringstream oss;
    oss << "{\"uri\":\"" << esc(uri) << "\",\"diagnostics\":[]}";
    notify("textDocument/publishDiagnostics", oss.str());
}

} // namespace

int main() {
#if defined(_WIN32)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    {
        // Best-effort executable directory for default locale resolution.
        KnArgs args = kn_parse_cmdline();
        if (args.argc > 0 && args.argv && args.argv[0] && args.argv[0][0]) {
            std::filesystem::path p(args.argv[0]);
            if (p.has_parent_path()) g_exe_dir = p.parent_path().string();
        }
        if (args.argv) {
            for (int i = 0; i < args.argc; ++i) kn_free(args.argv[i]);
            kn_free(args.argv);
        }
        // Fallback: use platform API to resolve the real executable path when argv[0] has no directory.
        if (g_exe_dir.empty()) {
#if defined(_WIN32)
            wchar_t buf[1024] = {};
            unsigned long len = GetModuleFileNameW(nullptr, buf, 1023);
            if (len > 0 && len < 1023) {
                std::filesystem::path ep(buf);
                if (ep.has_parent_path()) g_exe_dir = ep.parent_path().string();
            }
#elif defined(__linux__)
            char buf[PATH_MAX + 1] = {};
            ssize_t len = readlink("/proc/self/exe", buf, PATH_MAX);
            if (len > 0) {
                buf[len] = '\0';
                std::filesystem::path ep(buf);
                if (ep.has_parent_path()) g_exe_dir = ep.parent_path().string();
            }
#elif defined(__APPLE__)
            char buf[PATH_MAX + 1] = {};
            uint32_t sz = sizeof(buf);
            if (_NSGetExecutablePath(buf, &sz) == 0) {
                std::filesystem::path ep = std::filesystem::path(buf).lexically_normal();
                if (ep.has_parent_path()) g_exe_dir = ep.parent_path().string();
            }
#endif
        }
    }
    while (true) {
        std::string raw;
        if (!read_message(raw)) break;

        Json root;
        JsonParser jp(raw);
        if (!jp.parse(&root) || root.kind != Json::Kind::Object) {
            write_message("{\"jsonrpc\":\"2.0\",\"id\":null,\"error\":{\"code\":-32700,\"message\":\"Invalid JSON\"}}");
            continue;
        }

        std::string id = find_id_raw(raw);
        const Json *mv = root.find("method");
        std::string method = mv ? mv->str() : "";
        const Json *params = root.find("params");

        if (method == "initialize") {
            if (params && params->kind == Json::Kind::Object) {
                const Json *opts = params->find("initializationOptions");
                if (opts) apply_server_config(opts);
                set_workspace_roots_from_initialize(params);
                index_workspace();
            }
            write_message(response(id, init_json()));
            continue;
        }

        if (method == "kinal/stdlibText") {
            std::string rs = "\"\"";
            if (params && params->kind == Json::Kind::Object) {
                const Json *mv = params->find("module");
                if (mv && mv->kind == Json::Kind::String) {
                    std::string text;
                    std::string mod = mv->s;
                    // Handle klib/... paths from virtual document URIs.
                    constexpr std::string_view klib_pfx = "klib/";
                    if (text_starts_with(mod, klib_pfx)) {
                        std::string virt_uri = std::string("kinal-stdlib:/") + mod;
                        std::string cache_path = klib_virtual_uri_to_file_path(virt_uri);
                        if (!cache_path.empty()) {
                            if (auto ft = read_file_text(std::filesystem::path(cache_path))) text = *ft;
                        }
                    } else {
                        if (auto pkg = packaged_unit_text(mod)) text = *pkg;
                        else text = stdlib_stub_text(mod);
                    }
                    rs = "\"" + esc(text) + "\"";
                }
            }
            write_message(response(id, rs));
            continue;
        }
        if (method == "shutdown") {
            g_shutdown = true;
            write_message(response(id, "null"));
            continue;
        }
        if (method == "exit") {
            break;
        }

        if (method == "workspace/didChangeConfiguration") {
            std::string prev_lang = g_cfg.diag_lang;
            std::string prev_locale = g_cfg.locale_file;
            if (params && params->kind == Json::Kind::Object) {
                const Json *s = params->find("settings");
                if (s) apply_server_config(s);
            }
            if (g_cfg.diag_lang != prev_lang || g_cfg.locale_file != prev_locale) {
                for (auto &kv : g_docs) {
                    update_analysis(kv.second);
                    update_unit_index_for_uri(kv.second.uri, kv.second.an.toks);
                    publish_diags(kv.second);
                }
            }
            continue;
        }

        if (method == "workspace/didChangeWatchedFiles") {
            bool touched = false;
            if (params && params->kind == Json::Kind::Object) {
                const Json *chs = params->find("changes");
                if (chs && chs->kind == Json::Kind::Array) {
                    for (const Json &c : chs->a) {
                        if (c.kind != Json::Kind::Object) continue;
                        std::string uri = c.find("uri") ? c.find("uri")->str() : "";
                        int ty = c.find("type") ? c.find("type")->i32(0) : 0; // 1=create, 2=change, 3=delete
                        if (uri.empty()) continue;
                        if (uri.rfind("file://", 0) == 0) uri = path_to_uri(uri_to_path(uri));
                        if (ty == 3) {
                            g_unit_docs.erase(uri);
                            g_decl_doc_cache.erase(uri);
                            touched = true;
                            continue;
                        }
                        std::filesystem::path p(uri_to_path(uri));
                        auto txt = read_file_text(p);
                        if (!txt) continue;
                        g_unit_docs[uri] = index_units_from_tokens(lex_all(*txt));
                        g_decl_doc_cache.erase(uri);
                        touched = true;
                    }
                }
            }
            if (touched) rebuild_known_units();
            continue;
        }

        if (method == "textDocument/didOpen") {
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                if (td && td->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    if (!uri.empty()) {
                        Doc d;
                        d.uri = uri;
                        d.path = uri_to_path(uri);
                        d.text = td->find("text") ? td->find("text")->str() : "";
                        d.version = td->find("version") ? td->find("version")->i32(0) : 0;
                        update_analysis(d);
                        update_unit_index_for_uri(uri, d.an.toks);
                        g_docs[uri] = std::move(d);
                        publish_diags(g_docs[uri]);
                    }
                }
            }
            continue;
        }

        if (method == "textDocument/didChange") {
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                const Json *cc = params->find("contentChanges");
                if (td && td->kind == Json::Kind::Object && cc && cc->kind == Json::Kind::Array) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end()) {
                        for (const Json &chg : cc->a) {
                            if (chg.kind == Json::Kind::Object) apply_change(it->second.text, chg);
                        }
                        it->second.version = td->find("version") ? td->find("version")->i32(it->second.version) : it->second.version;
                        update_analysis(it->second);
                        update_unit_index_for_uri(uri, it->second.an.toks);
                        publish_diags(it->second);
                    }
                }
            }
            continue;
        }

        if (method == "textDocument/didSave") {
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                if (td && td->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end()) {
                        const Json *tv = params->find("text");
                        if (tv && tv->kind == Json::Kind::String) it->second.text = tv->s;
                        update_analysis(it->second);
                        update_unit_index_for_uri(uri, it->second.an.toks);
                        publish_diags(it->second);
                    }
                }
            }
            continue;
        }

        if (method == "textDocument/didClose") {
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                if (td && td->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    if (!uri.empty()) {
                        g_docs.erase(uri);
                        clear_diags(uri);
                    }
                }
            }
            continue;
        }

        if (method == "textDocument/completion") {
            std::string rs = "{\"isIncomplete\":false,\"items\":[]}";
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                const Json *pv = params->find("position");
                if (td && td->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end() && pv && pv->kind == Json::Kind::Object) {
                        int line = pv->find("line") ? pv->find("line")->i32(0) : 0;
                        int ch = pv->find("character") ? pv->find("character")->i32(0) : 0;
                        rs = completion_json_at(it->second, line, ch);
                    } else if (it != g_docs.end()) {
                        rs = completion_json(it->second);
                    }
                }
            }
            write_message(response(id, rs));
            continue;
        }

        if (method == "textDocument/codeAction") {
            std::string rs = "[]";
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                if (td && td->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end()) rs = code_actions_json(it->second, params);
                }
            }
            write_message(response(id, rs));
            continue;
        }

        if (method == "textDocument/hover") {
            std::string rs = "null";
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                const Json *pv = params->find("position");
                if (td && pv && td->kind == Json::Kind::Object && pv->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    int line = pv->find("line") ? pv->find("line")->i32(0) : 0;
                    int ch = pv->find("character") ? pv->find("character")->i32(0) : 0;
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end()) rs = hover_json_at(it->second, line, ch);
                }
            }
            write_message(response(id, rs));
            continue;
        }

        if (method == "textDocument/semanticTokens/full") {
            std::string rs = "{\"data\":[]}";
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                if (td && td->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end()) rs = sem_json(it->second);
                }
            }
            write_message(response(id, rs));
            continue;
        }

        if (method == "textDocument/definition") {
            std::string rs = "[]";
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                const Json *pv = params->find("position");
                if (td && pv && td->kind == Json::Kind::Object && pv->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    int line = pv->find("line") ? pv->find("line")->i32(0) : 0;
                    int ch = pv->find("character") ? pv->find("character")->i32(0) : 0;
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end()) {
                        auto h = hit_at(it->second, line, ch);
                        if (h) {
                            if (is_unit_key(h->key)) rs = locs_json(unit_defs_for_name(unit_name_from_key(h->key)));
                            else {
                                std::vector<Loc> locs = defs_for(h->key);
                                if (locs.empty()) locs = project_defs_for_hit(it->second, *h);
                                if (locs.empty()) locs = project_definition_at(it->second, line, ch);
                                rs = locs_json(locs);
                            }
                        } else {
                            if (auto l = stdlib_definition_at(it->second, line, ch)) {
                                std::vector<Loc> tmp;
                                tmp.push_back(*l);
                                rs = locs_json(tmp);
                            } else {
                                rs = locs_json(project_definition_at(it->second, line, ch));
                            }
                        }
                    }
                }
            }
            write_message(response(id, rs));
            continue;
        }

        if (method == "textDocument/documentSymbol") {
            std::string rs = "[]";
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                if (td && td->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end()) rs = document_symbols_json(it->second);
                }
            }
            write_message(response(id, rs));
            continue;
        }

        if (method == "textDocument/references") {
            std::string rs = "[]";
            if (params && params->kind == Json::Kind::Object) {
                const Json *td = params->find("textDocument");
                const Json *pv = params->find("position");
                bool inc_decl = true;
                const Json *ctx = params->find("context");
                if (ctx && ctx->kind == Json::Kind::Object) {
                    const Json *iv = ctx->find("includeDeclaration");
                    inc_decl = iv ? iv->bval(true) : true;
                }
                if (td && pv && td->kind == Json::Kind::Object && pv->kind == Json::Kind::Object) {
                    std::string uri = td->find("uri") ? td->find("uri")->str() : "";
                    int line = pv->find("line") ? pv->find("line")->i32(0) : 0;
                    int ch = pv->find("character") ? pv->find("character")->i32(0) : 0;
                    auto it = g_docs.find(uri);
                    if (it != g_docs.end()) {
                        auto h = hit_at(it->second, line, ch);
                        if (h) {
                            if (is_unit_key(h->key)) rs = locs_json(unit_refs_for_name(unit_name_from_key(h->key), inc_decl));
                            else rs = locs_json(refs_for(h->key, inc_decl));
                        }
                    }
                }
            }
            write_message(response(id, rs));
            continue;
        }

        if (!id.empty()) write_message(response(id, "null"));
    }
    return g_shutdown ? 0 : 0;
}
