#pragma once

#include <iostream>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <algorithm>
#include <stdexcept>
#include <execution>

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;
static constexpr double INACCURACY_OF_COMPARISON = 1e-6;

class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);

    explicit SearchServer(const std::string& stop_words_text)
            : SearchServer(SplitIntoWords(stop_words_text)) {
    }

    explicit SearchServer(std::string_view stop_words_text)
            : SearchServer(SplitIntoWordsStringView(stop_words_text)) {
    }

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);

    using DocumentMatch = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    [[nodiscard]] DocumentMatch MatchDocument(std::string_view raw_query, int document_id) const;
    [[nodiscard]] DocumentMatch MatchDocument(const std::execution::sequenced_policy& policy, std::string_view raw_query, int document_id) const;
    [[nodiscard]] DocumentMatch MatchDocument(const std::execution::parallel_policy& policy, std::string_view raw_query, int document_id) const;

    template <typename DocumentPredicate, typename ExecutionPolicy>
    [[nodiscard]] std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    [[nodiscard]] std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const;
    [[nodiscard]] std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    [[nodiscard]] std::vector<Document> FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const;
    [[nodiscard]] std::vector<Document> FindTopDocuments(std::string_view raw_query) const;

    template <typename ExecutionPolicy>
    void RemoveDocument(ExecutionPolicy&& policy, int document_id);
    void RemoveDocument(int document_id);

    [[nodiscard]] std::set<int>::const_iterator begin() const noexcept;
    [[nodiscard]] std::set<int>::const_iterator end() const noexcept;

    [[nodiscard]] const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    [[nodiscard]] int GetDocumentCount() const;

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
        std::string text_;
    };

    std::set<int> document_ids_;
    std::map<int, DocumentData> documents_;
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    [[nodiscard]] QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };

    [[nodiscard]] Query ParseQuery(std::string_view text) const;

    [[nodiscard]] bool IsStopWord(std::string_view word) const;

    [[nodiscard]] static bool IsValidWord(std::string_view word);

    [[nodiscard]] static int ComputeAverageRating(const std::vector<int>& ratings);

    [[nodiscard]] double ComputeWordInverseDocumentFreq(std::string_view word ) const;

    [[nodiscard]] std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;

    template<typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindAllDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;

    template<typename DocumentPredicate>
    [[nodiscard]] std::vector<Document> FindAllDocuments(const std::execution::parallel_policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const;
};

template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate, typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    auto matched_documents = FindAllDocuments(policy, raw_query, document_predicate);

    std::sort(policy, matched_documents.begin(), matched_documents.end(), [](const Document &lhs, const Document &rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < INACCURACY_OF_COMPARISON) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });

    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }

    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(policy, raw_query, [&status](int document_id, DocumentStatus new_status, int rating) {
        return new_status == status;
    });
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(ExecutionPolicy&& policy, std::string_view raw_query) const {
    return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    std::map<int, double> document_to_relevance;

    for (auto word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const DocumentData& documents_data = documents_.at(document_id);
            if (document_predicate(document_id, documents_data.status, documents_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (auto word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }

        for (const auto& [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (auto [document_id, relevance] : document_to_relevance) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }

    return matched_documents;
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    return FindAllDocuments(raw_query, document_predicate);
}

template<typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(16);
    const auto query = ParseQuery(raw_query);

    std::for_each(policy, query.minus_words.begin(), query.minus_words.end(),
                  [this, &document_to_relevance](std::string_view word) {
                      if (word_to_document_freqs_.count(word)) {
                          for (const auto[document_id, _]: word_to_document_freqs_.at(word)) {
                              document_to_relevance.Erase(document_id);
                          }
                      }
                  });

    std::for_each(policy, query.plus_words.begin(), query.plus_words.end(),
                  [this, &document_predicate, &document_to_relevance](std::string_view word) {
                      if (word_to_document_freqs_.count(word)) {
                          const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                          for (const auto[document_id, term_freq]: word_to_document_freqs_.at(word)) {
                              const auto &document_data = documents_.at(document_id);
                              if (document_predicate(document_id, document_data.status, document_data.rating)) {
                                  document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                              }
                          }
                      }
                  });

    std::map<int, double> document_to_relevance_reduced = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    matched_documents.reserve(document_to_relevance_reduced.size());

    for (const auto[document_id, relevance]: document_to_relevance_reduced) {
        matched_documents.emplace_back(document_id, relevance, documents_.at(document_id).rating);
    }

    return matched_documents;
}


template <typename ExecutionPolicy>
void SearchServer::RemoveDocument(ExecutionPolicy&& policy, int document_id) {
    if (document_to_word_freqs_.count(document_id)) {
        const std::map<std::string_view, double> &word_freqs = document_to_word_freqs_.at(document_id);
        std::for_each(policy, word_freqs.begin(), word_freqs.end(), [this, document_id](const auto &word_freq) {
            auto position = word_to_document_freqs_.find(word_freq.first);
            position->second.erase(document_id);
        });

        documents_.erase(document_id);
        document_ids_.erase(document_id);
        document_to_word_freqs_.erase(document_id);
    }
}