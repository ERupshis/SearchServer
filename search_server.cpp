#include "search_server.h"
#include "log_duration.h"

#include <cmath>
#include <execution>

using namespace std;

SearchServer::SearchServer(string_view stop_words_text) // S8 LESSON 9.3   
    :SearchServer(SplitIntoWords(stop_words_text))
{
}

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))// S8 9.3 UPD
{
}


void SearchServer::AddDocument(int document_id, string_view document, DocumentStatus status, const vector<int>& ratings) { // S8 LESSON 9.3 NEW
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("document contains wrong id"s);
    }
    vector<string_view> words = SplitIntoWordsNoStop(document);
    const double inv_word_count = 1.0 / words.size();
    for (string_view& word : words) {
        string s_word{ word };
        if (words_in_docs_.count(s_word) == 0) {
            words_in_docs_[s_word].first = s_word;
            string_view sv_word{ words_in_docs_.at(s_word).first };
            words_in_docs_.at(s_word).second = sv_word;
        }
        word_to_document_freqs_[words_in_docs_.at(s_word).second][document_id] += inv_word_count;
        document_words_freqs_[document_id][words_in_docs_.at(s_word).second] += inv_word_count;
    }
    documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.insert(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    RemoveDocument(std::execution::seq, document_id); // S8 NEW  
}

void SearchServer::RemoveDocument(const execution::sequenced_policy&, int document_id) { // S8 NEW
    if (document_ids_.count(document_id) == 1) {
        for (auto [word, freq] : GetWordFrequencies(document_id)) {
            word_to_document_freqs_[word].erase(document_id);
            if (word_to_document_freqs_.count(word) == 1 && word_to_document_freqs_.at(word).size() == 0) {
                string s_word{ word };
                words_in_docs_.erase(s_word);
            }
        }
        document_ids_.erase(document_id);
        documents_.erase(document_id);
        document_words_freqs_.erase(document_id);
    }
    return;
}
void SearchServer::RemoveDocument(const execution::parallel_policy&, int document_id) { // S8 NEW
    if (document_words_freqs_.count(document_id) == 0) {
        return;
    }

    document_ids_.erase(document_id);
    documents_.erase(document_id);
    document_words_freqs_.erase(document_id);

    const auto& word_freqs = document_words_freqs_.at(document_id);
    vector<string_view> words(word_freqs.size());
    transform(
        execution::par,
        word_freqs.begin(), word_freqs.end(),
        words.begin(),
        [](const auto& item) { return item.first; }
    );
    for_each(
        execution::par,
        words.begin(), words.end(),
        [this, document_id](string_view word) {
            word_to_document_freqs_.at(word).erase(document_id);
        });
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    return FindTopDocuments(std::execution::seq, raw_query, [status](int document_id, DocumentStatus statusp, int rating) { return statusp == status; });
}
vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    return FindTopDocuments(std::execution::seq, raw_query, DocumentStatus::ACTUAL);
}


int SearchServer::GetDocumentCount() const {
    return documents_.size();
}

const std::map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const { // S5 NEW
    if (document_ids_.count(document_id) == 1) {
        return document_words_freqs_.at(document_id);
    }
    static std::map<std::string_view, double> empty_map;
    return empty_map;
}

std::set<int>::const_iterator SearchServer::begin() const { // S5 NEW
    return document_ids_.begin();
}
std::set<int>::const_iterator SearchServer::end() const { // S5 NEW
    return document_ids_.end();
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(string_view raw_query, int document_id) const { ///S8 9.3 NEW
    return MatchDocument(std::execution::seq, raw_query, document_id);
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(const execution::sequenced_policy&, string_view raw_query, int document_id) const { //S8 9.3 NEW
    if (document_ids_.count(document_id) == 0) {
        return { {}, {} };
    }
    const Query query = ParseQuery(raw_query);

    const auto word_checker =
        [this, document_id](string_view word) {
            const auto it = word_to_document_freqs_.find(word);
            return it != word_to_document_freqs_.end() && it->second.count(document_id);
        };


    if (any_of(execution::seq,
        query.minus_words.begin(), query.minus_words.end(),
        word_checker)) {
        vector<string_view> empty;
        return { empty, documents_.at(document_id).status };
    }

    vector<string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(execution::seq,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        word_checker
    );
    std::sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return make_tuple(matched_words, documents_.at(document_id).status);
}

SearchServer::MatchDocumentResult SearchServer::MatchDocument(const execution::parallel_policy&, string_view raw_query, int document_id) const {
    if (document_ids_.count(document_id) == 0) {
        return { {}, {} };
    }
    const Query query = ParseQuery(raw_query);

    const auto word_checker =
        [this, document_id](string_view word) {
            const auto it = word_to_document_freqs_.find(word);
            return it != word_to_document_freqs_.end();
        };

    if (any_of(execution::par,
        query.minus_words.begin(), query.minus_words.end(),
        word_checker)) {
        vector<string_view> empty;
        return { empty, documents_.at(document_id).status };
    }

    vector<string_view> matched_words(query.plus_words.size());
    auto words_end = copy_if(execution::par,
        query.plus_words.begin(), query.plus_words.end(),
        matched_words.begin(),
        word_checker
    );
    std::sort(matched_words.begin(), words_end);
    words_end = unique(matched_words.begin(), words_end);
    matched_words.erase(words_end, matched_words.end());

    return make_tuple(matched_words, documents_.at(document_id).status);
}

bool SearchServer::IsStopWord(string_view word) const {
    return stop_words_.count(word) > 0;
}

bool SearchServer::IsValidWord(string_view word) { // ??? WORK OR NOT?    
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string_view> SearchServer::SplitIntoWordsNoStop(string_view text) const { // S8 LESSON 9.3 NEW
    vector<string_view> words;
    for (const string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            string s_word(word.begin(), word.end());
            throw invalid_argument("Word "s + string(s_word) + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = 0;
    for (const int rating : ratings) {
        rating_sum += rating;
    }
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(string_view text) const { // S8 9.3 NEW    
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (text.empty() || text[0] == '-' || !IsValidWord(text)) {
        throw invalid_argument("Query word "s + string(text) + " is invalid"s);
    }
    return { text, is_minus, IsStopWord(text) };
}

SearchServer::Query SearchServer::ParseQuery(const string_view& text) const { // S8 9.3 NEW
    Query result = {};
    for (const string_view word : SplitIntoWords(text)) {
        QueryWord query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                result.minus_words.insert(query_word.data);
            }
            else {
                result.plus_words.insert(query_word.data);
            }
        }
    }
    return result;
}

double SearchServer::ComputeWordInverseDocumentFreq(string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
