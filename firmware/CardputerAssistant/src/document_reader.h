#pragma once

#include <string>

namespace cardputer {

enum class DocumentReaderMode {
    Text,
    Markdown,
    Csv,
    Json,
    HtmlSource,
};

DocumentReaderMode detectDocumentReaderMode(const std::string& filename);
std::string documentReaderModeLabel(DocumentReaderMode mode);
std::string formatDocumentChunk(DocumentReaderMode mode, const std::string& content);
std::string documentSpeechText(DocumentReaderMode mode, const std::string& content);

}  // namespace cardputer
