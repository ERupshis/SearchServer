#include "test_example_functions.h"
#include "log_duration.h"

#include <iostream>

using namespace std;

void PrintDocument(const Document& document) {
    cout << "{ "s
        << "document_id = "s << document.id << ", "s
        << "relevance = "s << document.relevance << ", "s
        << "rating = "s << document.rating << " }"s << endl;
}

void PrintMatchDocumentResult(int document_id, const vector<string_view>& words, DocumentStatus status) {
    cout << "{ "s
        << "document_id = "s << document_id << ", "s
        << "status = "s << static_cast<int>(status) << ", "s
        << "words ="s;
    for (const string_view word : words) {
        cout << ' ' << word;
    }
    cout << "}"s << endl;
}

void AddDocument(SearchServer& search_server, int document_id, string_view document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const invalid_argument& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, string_view raw_query) {
    LOG_DURATION_STREAM("Время операции"s, cout);
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, string_view query) {
    LOG_DURATION_STREAM("Время операции"s, cout);
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;

        for (int document_id : search_server) {
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const invalid_argument& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}

void RemoveDuplicates(SearchServer& search_server) {
    std::set<std::set<std::string_view>> existing_docs;
    std::vector<int> found_duplicates;

    for (int document_id : search_server) {
        auto& freqs = search_server.GetWordFrequencies(document_id);
        std::set<std::string_view> words;

        std::transform(freqs.begin(), freqs.end(), std::inserter(words, words.begin()),
            [](auto p) {
                return p.first;
            });

        if (existing_docs.count(words) > 0) {
            cout << "Found duplicate document id " << document_id << endl;
            found_duplicates.push_back(document_id);
        }
        else {
            existing_docs.insert(words);
        }
    }

    for (int id : found_duplicates) {
        search_server.RemoveDocument(id);
    }
}