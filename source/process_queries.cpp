#include "process_queries.h"

std::vector<std::vector<Document>> ProcessQueries(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> answers(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(), answers.begin(), [&search_server](const std::string& query){
        return search_server.FindTopDocuments(query);
    });
    return answers;
}

std::deque<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    std::deque<Document> results;
    for (const auto& doc : ProcessQueries(search_server, queries)) {
        results.insert(results.end(), doc.begin(), doc.end());
    }
    return results;
}