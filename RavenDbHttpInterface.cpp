#include "RavenDbHttpInterface.h"
#include "HttpModule.h"
#include "Modules/ModuleManager.h"
#include <chrono>
#include <ctime>

TFuture<RavenBatchCommandResponse> RavenDbHttpInterface::AtomicUpdate(const RavenBatchCommandRequest& commandRequest, const DatabaseSelector database)
{
    TSharedRef<FJsonObject> jsonObject = MakeShareable(new FJsonObject());
    TArray<TSharedPtr<FJsonValue>> commandsArray;

	for (const TSharedPtr<RavenCommand>& command : commandRequest.commands)
	{
	    TSharedPtr<FJsonObject> commandObject = MakeShareable(new FJsonObject());
	    command->ToJson(commandObject);
	    commandsArray.Add(MakeShareable(new FJsonValueObject(commandObject)));
	}

    jsonObject->SetArrayField("Commands", commandsArray);
    FString commandsJson;
    TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> jsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&commandsJson);
    FJsonSerializer::Serialize(jsonObject, jsonWriter);
    return BatchCommandRequest(commandsJson, database);
}

TFuture<RavenBatchCommandResponse> RavenDbHttpInterface::BatchCommandRequest(const FString& commandsJson, const DatabaseSelector databaseSelection)
{
    TSharedPtr<TPromise<RavenBatchCommandResponse>> returnPromise = MakeShared<TPromise<RavenBatchCommandResponse>>();
    if (commandsJson.IsEmpty())
    {
        //returnPromise->SetValue(RavenBatchCommandResponse{false, ResponseCode::INVALID_REQUEST, {}});
        //return returnPromise->GetFuture();
    }

    TSharedRef<IHttpRequest> httpRequest = CreateHttpRequest(TEXT("POST"), GenerateRequestUrl(TEXT("/bulk_docs"), {}, databaseSelection), TEXT("application/json"));
    httpRequest->SetContentAsString(commandsJson);
    httpRequest->OnProcessRequestComplete().BindLambda([returnPromise](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool success)
    {
        RavenBatchCommandResponse batchResponse;
        if (!success || !httpResponse.IsValid() || (httpResponse.IsValid() && httpResponse->GetResponseCode() != EHttpResponseCodes::Created))
        {
            batchResponse.code = HttpStatusCodeToResponseCode(httpResponse->GetResponseCode());
            returnPromise->SetValue(batchResponse);
            return;
        }

        FString resultsJson = httpResponse->GetContentAsString();

        TSharedPtr<FJsonObject> jsonObject;
        TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(resultsJson);
        if (!FJsonSerializer::Deserialize(reader, jsonObject) || !jsonObject.IsValid())
        {
            batchResponse.code = DESERIALIZATION_ERROR;
            returnPromise->SetValue(batchResponse);
            return;
        }

        const TArray<TSharedPtr<FJsonValue>>* resultsArrayPtr = nullptr;
        if (!jsonObject->TryGetArrayField(TEXT("Results"), resultsArrayPtr) || resultsArrayPtr == nullptr)
        {
            batchResponse.code = DESERIALIZATION_ERROR;
            returnPromise->SetValue(batchResponse);
            return;
        }

        const TArray<TSharedPtr<FJsonValue>>& resultsArray = *resultsArrayPtr;
        for (const TSharedPtr<FJsonValue>& resultJson : resultsArray)
        {
            TSharedPtr<FJsonObject> resultObject = resultJson->AsObject();
            if (!resultObject.IsValid())
            {
                continue;
            }

            FString type;
            if (!resultObject->TryGetStringField(TEXT("Type"), type))
            {
                continue;
            }

            RavenCommandResponse result;
            if (type.Equals(TEXT("PUT")))
            {
                result = RavenPutCommandResponse{};
            }
            else if (type.Equals(TEXT("DELETE")))
            {
                result = RavenDeleteCommandResponse{};
            }
            else
            {
                // Unsupported command type
                continue;
            }

            result.FromJson(resultObject);
            batchResponse.results.Add(result);
        }

        batchResponse.success = true;
        batchResponse.code = ResponseCode::OK;
        returnPromise->SetValue(batchResponse);
    });
    httpRequest->ProcessRequest();
    return returnPromise->GetFuture();
}


TFuture<TMap<FString, FString>> RavenDbHttpInterface::GetDocumentsRequest(const TArray<FString>& documentIds, const DatabaseSelector databaseSelection)
{
    TSharedPtr<TPromise<TMap<FString, FString>>> returnPromise = MakeShared<TPromise<TMap<FString, FString>>>();
    TSharedRef<IHttpRequest> httpRequest = CreateHttpRequest(TEXT("GET"), GenerateRequestUrl(TEXT("/docs"), documentIds, databaseSelection), TEXT(""));
    httpRequest->OnProcessRequestComplete().BindLambda([returnPromise, documentIds](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool success)
    {
        TMap<FString, FString> responseMap;
        if (success && httpResponse->GetResponseCode() == EHttpResponseCodes::Ok)
        {
            FString payloadString = httpResponse->GetContentAsString();
            TSharedPtr<FJsonObject> jsonObject = MakeShareable(new FJsonObject());
            TSharedRef<TJsonReader<>> jsonReader = TJsonReaderFactory<>::Create(payloadString);

            if (FJsonSerializer::Deserialize(jsonReader, jsonObject) && jsonObject->HasTypedField<EJson::Array>(TEXT("Results")))
            {
                TArray<TSharedPtr<FJsonValue>> resultsArray = jsonObject->GetArrayField(TEXT("Results"));

                for (int32 i = 0; i < resultsArray.Num(); ++i)
                {
                    if (i < documentIds.Num())
                    {
                        FString docId = documentIds[i];
                        if (resultsArray[i].IsValid() && resultsArray[i]->Type == EJson::Object)
                        {
                            TSharedPtr<FJsonObject> docObject = resultsArray[i]->AsObject();
                            FString docJson;
                            TSharedRef<TJsonWriter<>> jsonWriter = TJsonWriterFactory<>::Create(&docJson);
                            if (FJsonSerializer::Serialize(docObject.ToSharedRef(), jsonWriter))
                            {
                                responseMap.Add(docId, docJson);
                            }
                        }
                    }
                }
            }
        }
        else 
        {
            for (const FString& docId : documentIds)
            {
                responseMap.Add(docId, TEXT(""));
            }
        }
        returnPromise->SetValue(responseMap);
    });
    httpRequest->ProcessRequest();
    return returnPromise->GetFuture();
}

TFuture<UpdateDocumentResponse> RavenDbHttpInterface::UpdateDocumentRequest(const FString& documentId, const FString& documentJson, const DatabaseSelector databaseSelection)
{
    TSharedPtr<TPromise<UpdateDocumentResponse>> returnPromise = MakeShared<TPromise<UpdateDocumentResponse>>();
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = CreateHttpRequest(TEXT("PUT"), GenerateRequestUrl(TEXT("/docs"), {documentId}, databaseSelection), TEXT("application/json"));
    httpRequest->SetContentAsString(documentJson);
    httpRequest->OnProcessRequestComplete().BindLambda([&, returnPromise](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool success)
    {
    	UpdateDocumentResponse updateResponse;
        if (success && httpResponse.IsValid() && httpResponse->GetResponseCode() == EHttpResponseCodes::Created)
        {
            FString payloadString = httpResponse->GetContentAsString();
            TSharedPtr<FJsonObject> jsonObject = MakeShareable(new FJsonObject());
            TSharedRef<TJsonReader<>> reader = TJsonReaderFactory<>::Create(payloadString);
            if (FJsonSerializer::Deserialize(reader, jsonObject) && jsonObject.IsValid())
            {
                updateResponse.FromJson(jsonObject);
                updateResponse.code = HttpStatusCodeToResponseCode(httpResponse->GetResponseCode());
            }
            else
            {
                updateResponse.code = DESERIALIZATION_ERROR;
            }
        }
        else
        {
            updateResponse.code = CLIENT_ERROR;
        }
        returnPromise->SetValue(updateResponse);
    });
    httpRequest->ProcessRequest();
    return returnPromise->GetFuture();
}

TFuture<void> RavenDbHttpInterface::DeleteDocumentRequest(const FString& documentId, DatabaseSelector databaseSelection)
{
	TSharedPtr<TPromise<void>> returnPromise = MakeShared<TPromise<void>>();
	DeleteDocumentRequest(documentId, TEXT(""), databaseSelection).Next([returnPromise](bool response)
	{
		 returnPromise->SetValue();
	});
	return returnPromise->GetFuture();
}

TFuture<bool> RavenDbHttpInterface::DeleteDocumentRequest(const FString& documentId, const FString& expectedChangeVector, DatabaseSelector databaseSelection)
{
	TSharedPtr<TPromise<bool>> returnPromise = MakeShared<TPromise<bool>>();
	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = CreateHttpRequest(TEXT("DELETE"), GenerateRequestUrl(TEXT("/docs"), {documentId}, databaseSelection), TEXT(""));
	if (!expectedChangeVector.IsEmpty())
	{
		httpRequest->SetHeader(TEXT("If-Match"), expectedChangeVector);
	}
	httpRequest->OnProcessRequestComplete().BindLambda([returnPromise](FHttpRequestPtr httpRequest, FHttpResponsePtr httpResponse, bool success)
	{
		bool successResponse = httpResponse->GetResponseCode() == EHttpResponseCodes::NoContent;
		//bool concurrencyException = httpResponse->GetResponseCode() == EHttpResponseCodes::Conflict;
		returnPromise->SetValue(success && successResponse);
	});
    httpRequest->ProcessRequest();
	return returnPromise->GetFuture();
}

template<typename RequestFunc, typename ResponseType>
TFuture<TimedDatabaseResponse<ResponseType>> RavenDbHttpInterface::SendRequestAndMeasureLatency(RequestFunc requestFunction)
{
    TSharedPtr<TPromise<TimedDatabaseResponse<ResponseType>>> returnPromise = MakeShared<TPromise<TimedDatabaseResponse<ResponseType>>>();
    auto start = std::chrono::steady_clock::now();

    requestFunction().Next([returnPromise, start](ResponseType&& response) {
        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        TimedDatabaseResponse<ResponseType> timedResponse;
        timedResponse.latency = elapsed.count();
        timedResponse.response = response;
        returnPromise->SetValue(timedResponse);
    });
    return returnPromise->GetFuture();
}

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> RavenDbHttpInterface::CreateHttpRequest(const FString& verb, const FString& url, const FString& contentType)
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> httpRequest = FHttpModule::Get().CreateRequest();
    httpRequest->SetVerb(verb);
    httpRequest->SetURL(url);
    httpRequest->SetHeader("Content-Type", contentType);
    return httpRequest;
}

FString RavenDbHttpInterface::GenerateRequestUrl(const FString& endpoint, const TArray<FString>& documentIds, DatabaseSelector databaseSelection)
{
    FString url = GetDatabaseServerUrl() + TEXT("/databases/") + DatabaseNamesMap[databaseSelection] + endpoint;
    if (documentIds.Num() > 0)
    {
        url += TEXT("?id=") + documentIds[0];
        for (int32 i = 1; i < documentIds.Num(); ++i)
        {
            url += FString::Printf(TEXT("&id=%s"), *documentIds[i]);
        }
    }
    return url;
}

FString RavenDbHttpInterface::GetDatabaseServerUrl()
{
	return "http://127.0.0.1:8080";
}

ResponseCode RavenDbHttpInterface::HttpStatusCodeToResponseCode(const int32 httpResponseCode)
{
    switch (httpResponseCode)
    {
    case EHttpResponseCodes::Unknown:
        return CONNECTION_REFUSED;
    case EHttpResponseCodes::NotModified:
        return NOT_MODIFIED;
    case EHttpResponseCodes::NotFound:
        return NOT_FOUND;
    case EHttpResponseCodes::Conflict:
        return CONCURRENCY_EXCEPTION;
    case EHttpResponseCodes::ServerError:
        return SERVER_ERROR;
    case EHttpResponseCodes::Forbidden:
        return ACCESS_FORBIDDEN;
    case EHttpResponseCodes::ServiceUnavail:
        return CONNECTION_REFUSED;
    }
    return UNKNOWN;
}
