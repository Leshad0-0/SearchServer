#pragma once

#include <execution>
#include <deque>

#include "search_server.h"
#include "document.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries);

std::deque<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries);