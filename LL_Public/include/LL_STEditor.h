#pragma once
#include <string>
#include <vector>
#include <cctype>
namespace LL {
struct STEditorParts {
    std::string declarations, body, ending;
    bool split = false;
    std::string Join() const {
        return declarations + body + ending;
    }
};
// Ignore keywords inside nested comments and quoted strings; preserve exact source bytes.
inline STEditorParts SplitST(const std::string &source) {
    struct Token {
        std::string text;
        std::size_t begin, end;
    };
    std::vector<Token> tokens;
    for (std::size_t i = 0; i < source.size();) {
        if (source.compare(i, 2, "(*") == 0) {
            int depth = 1;
            i += 2;
            while (i < source.size() && depth) {
                if (source.compare(i, 2, "(*") == 0) {
                    ++depth;
                    i += 2;
                } else if (source.compare(i, 2, "*)") == 0) {
                    --depth;
                    i += 2;
                } else
                    ++i;
            }
            continue;
        }
        if (source.compare(i, 2, "//") == 0) {
            while (i < source.size() && source[i] != '\n')
                ++i;
            continue;
        }
        if (source[i] == '\'' || source[i] == '"') {
            char quote = source[i++];
            while (i < source.size()) {
                if (source[i] == '$') {
                    i += 2;
                    continue;
                }
                if (source[i] == quote) {
                    ++i;
                    if (i < source.size() && source[i] == quote) {
                        ++i;
                        continue;
                    }
                    break;
                }
                ++i;
            }
            continue;
        }
        if (std::isalpha(static_cast<unsigned char>(source[i])) || source[i] == '_') {
            auto begin = i++;
            while (i < source.size() &&
                   (std::isalnum(static_cast<unsigned char>(source[i])) || source[i] == '_'))
                ++i;
            auto word = source.substr(begin, i - begin);
            for (auto &c : word)
                c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            tokens.push_back({word, begin, i});
        } else
            ++i;
    }
    if (tokens.empty())
        return {source, "", "", false};
    const auto kind = tokens.front().text;
    if (kind != "PROGRAM" && kind != "FUNCTION" && kind != "FUNCTION_BLOCK")
        return {source, "", "", false};
    const std::string close = "END_" + kind;
    std::size_t end = source.size(), boundary = std::string::npos;
    int heads = 0;
    for (const auto &t : tokens) {
        if (t.text == "PROGRAM" || t.text == "FUNCTION" || t.text == "FUNCTION_BLOCK")
            ++heads;
        if (t.text == close) {
            end = t.begin;
            break;
        }
        if (t.text == "END_VAR")
            boundary = t.end;
    }
    if (heads != 1 || end == source.size())
        return {source, "", "", false};
    if (boundary == std::string::npos) {
        boundary = source.find('\n', tokens.front().end);
        if (boundary == std::string::npos || boundary >= end)
            return {source, "", "", false};
        ++boundary;
    } else {
        while (boundary < end && (source[boundary] == '\r' || source[boundary] == '\n'))
            ++boundary;
    }
    return {source.substr(0, boundary), source.substr(boundary, end - boundary), source.substr(end), true};
}
} // namespace LL
