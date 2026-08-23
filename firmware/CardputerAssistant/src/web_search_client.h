#pragma once

#include "api_client.h"
#include "app_types.h"

#include <vector>

namespace cardputer {

struct WebSearchSource {
    String title;
    String url;
    std::string snippet;
};

struct WebSearchSourcesResult {
    bool success;
    String query;
    std::vector<WebSearchSource> sources;
    String error;
};

ToolExecutionResult executeWebSearchTool(const Settings& settings,
                                         const ToolCall& call,
                                         const CancelCallback& isCancelled);
ToolExecutionResult executeWebFetchTool(const Settings& settings,
                                        const ToolCall& call,
                                        const CancelCallback& isCancelled);
WebSearchSourcesResult loadLatestWebSearchSources();

}  // namespace cardputer
