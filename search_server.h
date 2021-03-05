#pragma once

#include "document.h"
#include "string_processing.h"
#include "concurrent_map.h"

#include <execution>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

const int MAX_RESULT_DOCUMENT_COUNT = 5;


class SearchServer {
public:
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(std::string_view stop_words_text);// S8 LESSON 9.3  NEW

    void AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings);// S8 LESSON 9.3 NEW

    void RemoveDocument(int document_id); //S5 NEW
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id); //S8 NEW
    void RemoveDocument(const std::execution::parallel_policy&, int document_id); //S8 NEW

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const;

    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query) const;

    int GetDocumentCount() const;

    //const std::map<std::string, double>& GetWordFrequencies(int document_id) const; // S5
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const; // S5

    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

    using MatchDocumentResult = std::tuple<std::vector<std::string_view>, DocumentStatus>;
    MatchDocumentResult MatchDocument(std::string_view raw_query, int document_id) const; // S8 9.3 NEW
    MatchDocumentResult MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const; // S8 9.3 NEW
    MatchDocumentResult MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const; // S8 9.3 NEW

private:
    struct DocumentData {
        int rating = {};
        DocumentStatus status = {};
    };

    std::map<std::string, std::pair<std::string, std::string_view>> words_in_docs_;
    const std::set<std::string, std::less<>> stop_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_; // S8 LESSON 9.3  NEW
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    std::map<int, std::map<std::string_view, double>> document_words_freqs_;// S8 LESSON 9.3  NEW        

    friend void RemoveDuplicates(SearchServer& search_server);

    bool IsStopWord(std::string_view word) const;
    static bool IsValidWord(std::string_view word);

    std::vector<std::string_view> SplitIntoWordsNoStop(std::string_view text) const;// S8 LESSON 9.3 NEW
    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data; // 9.3
        bool is_minus;
        bool is_stop;
    };
    QueryWord ParseQueryWord(std::string_view text) const; // S8 LESSON 9.3 NEW

    struct Query {
        std::set<std::string_view> plus_words; // S8 LESSON 9.3 NEW
        std::set<std::string_view> minus_words;// S8 LESSON 9.3 NEW
    };

    Query ParseQuery(const std::string_view& text) const; // S8 9.3 NEW

    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;
};
//////////////////////////////////// FUNCTIONS WITH TEMPLATES
template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words))  // Extract non-empty stop words
{
    if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
        throw std::invalid_argument("Some of stop words are invalid"s);
    }
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) const { //9.3 NEW    
    return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
}
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::sequenced_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const { //9.3 NEW
    Query query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);
    sort(std::execution::seq, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}
template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(const std::execution::parallel_policy&, std::string_view raw_query, DocumentPredicate document_predicate) const { //NEED TO FIX
    Query query = ParseQuery(raw_query);
    std::vector<Document> matched_documents = FindAllDocuments(std::execution::par, query, document_predicate);
    sort(std::execution::par, matched_documents.begin(), matched_documents.end(), [](const Document& lhs, const Document& rhs) {
        if (std::abs(lhs.relevance - rhs.relevance) < 1e-6) {
            return lhs.rating > rhs.rating;
        }
        else {
            return lhs.relevance > rhs.relevance;
        }
        });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(4);

    const auto plus_word_checker =
        [this, &document_predicate, &document_to_relevance](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id].ref_to_value += static_cast<double>(term_freq * inverse_document_freq);
            }
        }
    };
    std::for_each(std::execution::seq, query.plus_words.begin(), query.plus_words.end(), plus_word_checker);

    const auto minus_word_checker =
        [this, &document_predicate, &document_to_relevance](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.Erase(document_id);
        }
    };
    std::for_each(std::execution::seq, query.minus_words.begin(), query.minus_words.end(), minus_word_checker);

    std::map<int, double> m_doc_to_relevance = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : m_doc_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
    ConcurrentMap<int, double> document_to_relevance(4);

    const auto plus_word_checker =
        [this, &document_predicate, &document_to_relevance](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id].ref_to_value += static_cast<double>(term_freq * inverse_document_freq);
            }
        }
    };
    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), plus_word_checker);
    const auto minus_word_checker =
        [this, &document_predicate, &document_to_relevance](std::string_view word) {
        if (word_to_document_freqs_.count(word) == 0) {
            return;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.Erase(document_id);
        }
    };

    std::for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(), minus_word_checker);

    std::map<int, double> m_doc_to_relevance = document_to_relevance.BuildOrdinaryMap();
    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : m_doc_to_relevance) {
        matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
    }
    return matched_documents;
}

