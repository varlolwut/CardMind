#pragma once

#include "app_types.h"

#include <FS.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace cardputer {

class SshCommandOutputCapture {
public:
    explicit SshCommandOutputCapture(std::size_t maximumInlineBytes);
    ~SshCommandOutputCapture();

    OperationResult append(const std::uint8_t* data, std::size_t bytes);
    OperationResult promoteToLog();
    OperationResult finalize();

    bool hasOutput() const;
    bool hasLog() const;
    bool isComplete() const;
    const std::string& inlineOutput() const;
    std::uint32_t verifiedOutputBytes() const;
    String logName() const;
    String downloadPath() const;

private:
    OperationResult createLog(std::size_t additionalBytes);
    OperationResult appendToLog(const std::uint8_t* data, std::size_t bytes);
    OperationResult writeToOpenLog(const std::uint8_t* data, std::size_t bytes);
    OperationResult fail(const String& error);

    std::size_t maximumInlineBytes_;
    std::string inlineOutput_;
    File file_;
    std::uint32_t receivedBytes_;
    std::uint32_t acceptedBytes_;
    std::uint32_t verifiedOutputBytes_;
    String logName_;
    String error_;
    bool logCreated_;
    bool finalized_;
    bool complete_;
};

}  // namespace cardputer
