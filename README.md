# SearchServer
Documents searching server that allows to find documents with ranging it to better correspondence to user's request.

Requests to searchserver:
- AddDocument(int document_id, std::string_view document, DocumentStatus status, const std::vector<int>& ratings) - adds document to database 

- RemoveDocument(int document_id) - removes document from database

- FindTopDocuments(std::string_view raw_query, DocumentPredicate document_predicate) - find documents according raw query. Some additional options can be made via doc predicate
(id, status, rating). Request result is a list of documents that are ranged in accordance of TF (term freq) -IDF (inverse document freq) parameters 
- MatchDocument(std::string_view raw_query, int document_id) - define words in doc that correspond to user request.

RemoveDocument, FindTopDocuments, MatchDocument can be executed in sequenced or parallel mode.
