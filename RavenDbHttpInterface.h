#pragma once
#include "Async/Future.h"
#include "Interfaces/IHttpResponse.h"
#include "Interfaces/IHttpRequest.h"
#include "RavenDbRequestResponse.h"

class RavenDbHttpInterface
{
public:
	static TFuture<RavenBatchCommandResponse> AtomicUpdate(const RavenBatchCommandRequest& commandRequest, const DatabaseSelector database);
	static TFuture<UpdateDocumentResponse> UpdateDocumentRequest(const FString& documentId, const FString& documentJson, const DatabaseSelector databaseSelection);
	static TFuture<void> DeleteDocumentRequest(const FString& documentId, const DatabaseSelector databaseSelection);
	static TFuture<bool> DeleteDocumentRequest(const FString& documentId, const FString& expectedChangeVector, const DatabaseSelector databaseSelection);
	static TFuture<TMap<FString, FString>> GetDocumentsRequest(const TArray<FString>& documentIds, const DatabaseSelector databaseSelection);

	template<typename RequestFunc, typename ResponseType>
	static TFuture<TimedDatabaseResponse<ResponseType>> SendRequestAndMeasureLatency(RequestFunc requestFunction);
private:
    static TFuture<RavenBatchCommandResponse> BatchCommandRequest(const FString& commandsJson, const DatabaseSelector databaseSelection);
    static TSharedRef<IHttpRequest, ESPMode::ThreadSafe> CreateHttpRequest(const FString& verb, const FString& url, const FString& contentType);

    static FString GetDatabaseServerUrl();
    static FString GenerateRequestUrl(const FString& endpoint, const TArray<FString>& documentIds, DatabaseSelector databaseSelection);
    static ResponseCode HttpStatusCodeToResponseCode(const int32 httpResponseCode);
};