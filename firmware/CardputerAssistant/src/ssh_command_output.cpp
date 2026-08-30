#include "ssh_command_output.h"

#include "file_workspace.h"
#include "sd_storage.h"

#include <SD.h>
#include <esp_random.h>

#include <cstdio>
#include <limits>

namespace cardputer {
namespace {

constexpr std::size_t kMaximumFilenameAttempts = 8;

String commandLogName()
{
    char token[17] = {};
    std::snprintf(
        token, sizeof(token), "%08lx%08lx",
        static_cast<unsigned long>(esp_random()),
        static_cast<unsigned long>(esp_random()));
    return String("ssh-command-") + token + ".log";
}

}  // namespace

SshCommandOutputCapture::SshCommandOutputCapture(
    std::size_t maximumInlineBytes)
    : maximumInlineBytes_(maximumInlineBytes),
      inlineOutput_(),
      file_(),
      receivedBytes_(0),
      acceptedBytes_(0),
      verifiedOutputBytes_(0),
      logName_(),
      error_(),
      logCreated_(false),
      finalized_(false),
      complete_(false)
{
}

SshCommandOutputCapture::~SshCommandOutputCapture()
{
    if (file_) {
        file_.close();
    }
}

OperationResult SshCommandOutputCapture::fail(const String& error)
{
    if (error_.isEmpty()) {
        error_ = error;
    }
    complete_ = false;
    return {false, error_};
}

OperationResult SshCommandOutputCapture::writeToOpenLog(
    const std::uint8_t* data,
    std::size_t bytes)
{
    if (!file_) {
        return fail("SSH command output log is not open");
    }
    const std::size_t written = file_.write(data, bytes);
    if (written > std::numeric_limits<std::uint32_t>::max() - acceptedBytes_) {
        return fail("SSH command output log byte count exceeded the downloadable limit");
    }
    acceptedBytes_ += static_cast<std::uint32_t>(written);
    if (written != bytes) {
        return fail(
            String("SSH command output log accepted only ") +
            String(static_cast<unsigned long>(written)) + " of " +
            String(static_cast<unsigned long>(bytes)) + " bytes");
    }
    return {true, ""};
}

OperationResult SshCommandOutputCapture::createLog(
    std::size_t additionalBytes)
{
    if (logCreated_) {
        return {true, ""};
    }
    if (!error_.isEmpty()) {
        return {false, error_};
    }
    if (additionalBytes >
        std::numeric_limits<std::uint32_t>::max() - inlineOutput_.size()) {
        return fail("SSH command output exceeded the downloadable file limit");
    }
    const std::size_t requiredBytes = inlineOutput_.size() + additionalBytes;
    OperationResult result = requireSdWriteAccess(
        requiredBytes, kStorageOperationalFloorBytes);
    if (!result.success) {
        return fail(result.error);
    }

    String candidate;
    for (std::size_t attempt = 0; attempt < kMaximumFilenameAttempts; ++attempt) {
        candidate = commandLogName();
        if (!SD.exists(workspaceFilePath(candidate))) {
            break;
        }
        candidate = "";
    }
    if (candidate.isEmpty()) {
        return fail("Unable to allocate a collision-free SSH command output log");
    }

    result = createWorkspaceFile(candidate);
    if (!result.success) {
        return fail(result.error);
    }
    logName_ = candidate;
    logCreated_ = true;
    file_ = SD.open(workspaceFilePath(logName_), FILE_APPEND);
    if (!file_) {
        inlineOutput_.clear();
        return fail("Failed to open the created SSH command output log");
    }
    if (!inlineOutput_.empty()) {
        result = writeToOpenLog(
            reinterpret_cast<const std::uint8_t*>(inlineOutput_.data()),
            inlineOutput_.size());
        inlineOutput_.clear();
        if (!result.success) {
            return result;
        }
    }
    return {true, ""};
}

OperationResult SshCommandOutputCapture::appendToLog(
    const std::uint8_t* data,
    std::size_t bytes)
{
    OperationResult result = requireSdWriteAccess(
        bytes, kStorageOperationalFloorBytes);
    if (!result.success) {
        return fail(result.error);
    }
    return writeToOpenLog(data, bytes);
}

OperationResult SshCommandOutputCapture::append(
    const std::uint8_t* data,
    std::size_t bytes)
{
    if (finalized_) {
        return {false, "SSH command output capture is already finalized"};
    }
    if (!error_.isEmpty()) {
        return {false, error_};
    }
    if (data == nullptr || bytes == 0) {
        return {false, "SSH command output append requires non-empty data"};
    }
    if (bytes > std::numeric_limits<std::uint32_t>::max() - receivedBytes_) {
        return fail("SSH command output exceeded the downloadable file limit");
    }
    receivedBytes_ += static_cast<std::uint32_t>(bytes);

    if (!logCreated_ &&
        inlineOutput_.size() <= maximumInlineBytes_ &&
        bytes <= maximumInlineBytes_ - inlineOutput_.size()) {
        inlineOutput_.append(
            reinterpret_cast<const char*>(data), bytes);
        return {true, ""};
    }
    OperationResult result = createLog(bytes);
    if (!result.success) {
        return result;
    }
    return appendToLog(data, bytes);
}

OperationResult SshCommandOutputCapture::promoteToLog()
{
    if (finalized_) {
        return {false, "SSH command output capture is already finalized"};
    }
    if (!error_.isEmpty()) {
        return {false, error_};
    }
    if (logCreated_ || receivedBytes_ == 0) {
        return {true, ""};
    }
    return createLog(0);
}

OperationResult SshCommandOutputCapture::finalize()
{
    if (finalized_) {
        return error_.isEmpty()
            ? OperationResult{true, ""}
            : OperationResult{false, error_};
    }
    finalized_ = true;
    if (!logCreated_) {
        complete_ = error_.isEmpty();
        return complete_
            ? OperationResult{true, ""}
            : OperationResult{false, error_};
    }

    if (file_) {
        file_.flush();
        file_.close();
    }
    OperationResult readable = requireSdReadAccess();
    if (!readable.success) {
        return fail(readable.error);
    }
    File stored = SD.open(workspaceFilePath(logName_), FILE_READ);
    if (!stored) {
        return fail("Failed to reopen the SSH command output log for verification");
    }
    const std::size_t storedBytes = stored.size();
    stored.close();
    if (storedBytes > std::numeric_limits<std::uint32_t>::max() ||
        storedBytes != acceptedBytes_) {
        return fail("SSH command output log size did not match accepted writes");
    }
    if (!error_.isEmpty()) {
        return {false, error_};
    }
    verifiedOutputBytes_ = acceptedBytes_;
    complete_ = verifiedOutputBytes_ == receivedBytes_;
    return complete_
        ? OperationResult{true, ""}
        : fail("SSH command output log is incomplete");
}

bool SshCommandOutputCapture::hasOutput() const
{
    return receivedBytes_ != 0;
}

bool SshCommandOutputCapture::hasLog() const
{
    return logCreated_;
}

bool SshCommandOutputCapture::isComplete() const
{
    return complete_;
}

const std::string& SshCommandOutputCapture::inlineOutput() const
{
    return inlineOutput_;
}

std::uint32_t SshCommandOutputCapture::verifiedOutputBytes() const
{
    return complete_ && logCreated_ ? verifiedOutputBytes_ : 0;
}

String SshCommandOutputCapture::logName() const
{
    return logName_;
}

String SshCommandOutputCapture::downloadPath() const
{
    return logCreated_
        ? String("/api/file/download?name=") + logName_
        : String();
}

}  // namespace cardputer
