#include "process_queries.h"

#include <algorithm>
#include <execution>

using namespace std;

vector<vector<Document>> ProcessQueries(const SearchServer& search_server, const vector<string>& queries)
{
    vector<vector<Document>> result(queries.size());
    transform(execution::par, queries.begin(), queries.end(), result.begin(),
        [&search_server](const std::string& query) { 
            return search_server.FindTopDocuments(query); 
        });
    return result;
}

vector<Document> ProcessQueriesJoined(const SearchServer& search_server, const vector<string>& queries)
{
    vector<Document> result;
    for (const auto& query_response : ProcessQueries(search_server, queries)) {
        result.insert(result.end(), query_response.begin(), query_response.end());
    }
    return result;
}
