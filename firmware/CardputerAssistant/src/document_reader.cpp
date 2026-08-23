#include "document_reader.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace cardputer {
namespace {

std::string lowercaseAscii(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) {
        return static_cast<char>(std::tolower(character));
    });
    return value;
}

bool endsWith(const std::string& value, const std::string& suffix)
{
    return value.size() >= suffix.size() &&
        value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string trimAscii(const std::string& value)
{
    const std::size_t first = value.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return "";
    }
    const std::size_t last = value.find_last_not_of(" \t\r");
    return value.substr(first, last - first + 1);
}

std::string formatMarkdown(const std::string& content)
{
    std::string output;
    std::size_t start = 0;
    while (start <= content.size()) {
        const std::size_t end = content.find('\n', start);
        std::string line = content.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        const std::size_t first = line.find_first_not_of(' ');
        if (first != std::string::npos) {
            std::size_t marker = first;
            while (marker < line.size() && line[marker] == '#') {
                ++marker;
            }
            if (marker > first && marker < line.size() && line[marker] == ' ') {
                line.erase(first, marker - first + 1);
            } else if (line.compare(first, 2, "- ") == 0 ||
                       line.compare(first, 2, "* ") == 0) {
                line.replace(first, 2, "• ");
            } else if (line.compare(first, 2, "> ") == 0) {
                line.erase(first, 2);
            }
        }
        output += line;
        if (end == std::string::npos) {
            break;
        }
        output += '\n';
        start = end + 1;
    }
    return output;
}

std::vector<std::string> parseCsvLine(const std::string& line)
{
    std::vector<std::string> cells;
    std::string cell;
    bool quoted = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (character == '"') {
            if (quoted && index + 1 < line.size() && line[index + 1] == '"') {
                cell += '"';
                ++index;
            } else {
                quoted = !quoted;
            }
        } else if (character == ',' && !quoted) {
            cells.push_back(trimAscii(cell));
            cell.clear();
        } else if (character != '\r') {
            cell += character;
        }
    }
    cells.push_back(trimAscii(cell));
    return cells;
}

std::string formatCsv(const std::string& content)
{
    std::string output;
    std::size_t start = 0;
    while (start <= content.size()) {
        const std::size_t end = content.find('\n', start);
        const std::string line = content.substr(
            start, end == std::string::npos ? std::string::npos : end - start);
        const std::vector<std::string> cells = parseCsvLine(line);
        for (std::size_t index = 0; index < cells.size(); ++index) {
            if (index > 0) {
                output += " | ";
            }
            output += cells[index];
        }
        if (end == std::string::npos) {
            break;
        }
        output += '\n';
        start = end + 1;
    }
    return output;
}

std::string formatJson(const std::string& content)
{
    std::string output;
    std::size_t indent = 0;
    bool quoted = false;
    bool escaped = false;
    auto appendIndent = [&output, &indent]() { output.append(indent * 2, ' '); };
    for (const char character : content) {
        if (quoted) {
            output += character;
            if (escaped) {
                escaped = false;
            } else if (character == '\\') {
                escaped = true;
            } else if (character == '"') {
                quoted = false;
            }
            continue;
        }
        if (character == '"') {
            quoted = true;
            output += character;
        } else if (character == '{' || character == '[') {
            output += character;
            output += '\n';
            ++indent;
            appendIndent();
        } else if (character == '}' || character == ']') {
            if (!output.empty() && output.back() == ' ') {
                while (!output.empty() && output.back() == ' ') {
                    output.pop_back();
                }
            }
            if (!output.empty() && output.back() != '\n') {
                output += '\n';
            }
            indent = indent > 0 ? indent - 1 : 0;
            appendIndent();
            output += character;
        } else if (character == ',') {
            output += ",\n";
            appendIndent();
        } else if (character == ':') {
            output += ": ";
        } else if (!std::isspace(static_cast<unsigned char>(character))) {
            output += character;
        }
    }
    return output;
}

std::string stripHtml(const std::string& content)
{
    std::string output;
    bool inTag = false;
    for (std::size_t index = 0; index < content.size(); ++index) {
        const char character = content[index];
        if (character == '<') {
            inTag = true;
            if (!output.empty() && output.back() != ' ') {
                output += ' ';
            }
        } else if (character == '>') {
            inTag = false;
        } else if (!inTag) {
            output += character;
        }
    }
    const std::pair<const char*, const char*> entities[] = {
        {"&nbsp;", " "}, {"&amp;", "&"}, {"&lt;", "<"},
        {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"},
    };
    for (const auto& entity : entities) {
        std::size_t found = 0;
        while ((found = output.find(entity.first, found)) != std::string::npos) {
            output.replace(found, std::char_traits<char>::length(entity.first), entity.second);
            found += std::char_traits<char>::length(entity.second);
        }
    }
    return output;
}

std::string simplifyMarkdownSpeech(const std::string& content)
{
    std::string output = formatMarkdown(content);
    output.erase(std::remove(output.begin(), output.end(), '`'), output.end());
    output.erase(std::remove(output.begin(), output.end(), '*'), output.end());
    output.erase(std::remove(output.begin(), output.end(), '_'), output.end());
    std::size_t open = 0;
    while ((open = output.find('[', open)) != std::string::npos) {
        const std::size_t middle = output.find("](", open);
        const std::size_t close = middle == std::string::npos
            ? std::string::npos
            : output.find(')', middle + 2);
        if (middle == std::string::npos || close == std::string::npos) {
            break;
        }
        output.erase(middle, close - middle + 1);
        output.erase(open, 1);
    }
    return output;
}

}  // namespace

DocumentReaderMode detectDocumentReaderMode(const std::string& filename)
{
    const std::string lower = lowercaseAscii(filename);
    if (endsWith(lower, ".md") || endsWith(lower, ".markdown")) {
        return DocumentReaderMode::Markdown;
    }
    if (endsWith(lower, ".csv")) {
        return DocumentReaderMode::Csv;
    }
    if (endsWith(lower, ".json")) {
        return DocumentReaderMode::Json;
    }
    if (endsWith(lower, ".html") || endsWith(lower, ".htm")) {
        return DocumentReaderMode::HtmlSource;
    }
    return DocumentReaderMode::Text;
}

std::string documentReaderModeLabel(DocumentReaderMode mode)
{
    switch (mode) {
    case DocumentReaderMode::Markdown:
        return "MARKDOWN";
    case DocumentReaderMode::Csv:
        return "CSV TABLE";
    case DocumentReaderMode::Json:
        return "JSON";
    case DocumentReaderMode::HtmlSource:
        return "HTML SOURCE";
    case DocumentReaderMode::Text:
        return "TEXT";
    }
    return "TEXT";
}

std::string formatDocumentChunk(DocumentReaderMode mode, const std::string& content)
{
    switch (mode) {
    case DocumentReaderMode::Markdown:
        return formatMarkdown(content);
    case DocumentReaderMode::Csv:
        return formatCsv(content);
    case DocumentReaderMode::Json:
        return formatJson(content);
    case DocumentReaderMode::HtmlSource:
    case DocumentReaderMode::Text:
        return content;
    }
    return content;
}

std::string documentSpeechText(DocumentReaderMode mode, const std::string& content)
{
    switch (mode) {
    case DocumentReaderMode::Markdown:
        return simplifyMarkdownSpeech(content);
    case DocumentReaderMode::Csv:
        return formatCsv(content);
    case DocumentReaderMode::Json:
        return formatJson(content);
    case DocumentReaderMode::HtmlSource:
        return stripHtml(content);
    case DocumentReaderMode::Text:
        return content;
    }
    return content;
}

}  // namespace cardputer
