#include "remove_duplicates.h"

void RemoveDuplicates(SearchServer &search_server) {
    std::set<std::set<std::string_view>> docs;
    std::vector<int> duplicates;

    for (const int document_id: search_server) {
        auto word_freqs = search_server.GetWordFrequencies(document_id);
        std::set<std::string_view> words;
        transform(word_freqs.begin(), word_freqs.end(), inserter(words, words.begin()),
                  [](const std::pair<std::string_view, double> word) {
                      return word.first;
                  });

        if (docs.count(words) == 0) {
            docs.insert(words);
        } else {
            duplicates.push_back(document_id);
        }
    }

    for (const int id: duplicates) {
        std::cout << "Found duplicate document id" + std::to_string(id) << "\n";
        search_server.RemoveDocument(id);
    }
}