#pragma once

enum DatabaseSelector : uint8
{
    EXCHANGE,
    INVENTORY
};

static const TMap<DatabaseSelector, FString> DatabaseNamesMap = {
    { DatabaseSelector::EXCHANGE, TEXT("Exchange") },
    { DatabaseSelector::INVENTORY, TEXT("Inventory") }
};

enum ResponseCode : uint8 {
    UNKNOWN,
    OK,
    INVALID_REQUEST,
    NOT_FOUND,
    FOUND,
    NOT_MODIFIED,
    CONCURRENCY_EXCEPTION,
    SERVER_ERROR,
    CLIENT_ERROR,
    ACCESS_FORBIDDEN,
    CONNECTION_REFUSED,
    DESERIALIZATION_ERROR,
    SERIALIZATION_ERROR,
};

static const TMap<ResponseCode, FString> ResponseCodeNamesMap = {
    { ResponseCode::UNKNOWN, TEXT("UNKNOWN") },
    { ResponseCode::OK, TEXT("OK") },
    { ResponseCode::INVALID_REQUEST, TEXT("INVALID_REQUEST") },
    { ResponseCode::NOT_FOUND, TEXT("NOT_FOUND") },
    { ResponseCode::FOUND, TEXT("FOUND") },
    { ResponseCode::NOT_MODIFIED, TEXT("NOT_MODIFIED") },
    { ResponseCode::CONCURRENCY_EXCEPTION, TEXT("CONCURRENCY_EXCEPTION") },
    { ResponseCode::SERVER_ERROR, TEXT("SERVER_ERROR") },
    { ResponseCode::CLIENT_ERROR, TEXT("CLIENT_ERROR") },
    { ResponseCode::ACCESS_FORBIDDEN, TEXT("ACCESS_FORBIDDEN") },
    { ResponseCode::CONNECTION_REFUSED, TEXT("CONNECTION_REFUSED") },
    { ResponseCode::DESERIALIZATION_ERROR, TEXT("DESERIALIZATION_ERROR") },
    { ResponseCode::SERIALIZATION_ERROR, TEXT("SERIALIZATION_ERROR") },
};

template<typename ResponseType>
struct TimedDatabaseResponse
{
    double latency;
    ResponseType response;
};

struct UpdateDocumentResponse
{
    bool success = false;
    ResponseCode code;
    FString id;
    FString changeVector;

    void FromJson(TSharedPtr<FJsonObject> jsonObject)
    {
        jsonObject->TryGetStringField(TEXT("Id"), id);
        jsonObject->TryGetStringField(TEXT("ChangeVector"), changeVector);
        success = true;
    }
};


struct RavenCommand
{
    virtual ~RavenCommand() {}

    FString type;

    virtual void ToJson(TSharedPtr<FJsonObject> jsonObject) const
    {
        jsonObject->SetStringField(TEXT("Type"), type);
    }
};

struct RavenBatchCommandRequest
{
    TArray<TSharedPtr<RavenCommand>> commands;
};

struct RavenPutCommand : public RavenCommand
{
    RavenPutCommand()
    {
        type = "PUT";
    }

    FString id;
    TSharedPtr<FJsonObject> documentJsonObject;
    FString changeVector;

    virtual void ToJson(TSharedPtr<FJsonObject> jsonObject) const override
    {
        RavenCommand::ToJson(jsonObject);
        jsonObject->SetStringField(TEXT("Id"), id);
        if (documentJsonObject.IsValid())
        {
            jsonObject->SetObjectField(TEXT("Document"), documentJsonObject);
        }
        if (!changeVector.IsEmpty())
        {
            jsonObject->SetStringField(TEXT("ChangeVector"), changeVector);
        }
        else
        {
            jsonObject->SetField(TEXT("ChangeVector"), MakeShareable(new FJsonValueNull()));
        }
    }
};

struct RavenDeleteCommand : public RavenCommand
{
    RavenDeleteCommand()
    {
        type = "DELETE";
    }
    FString id;
    FString changeVector;

    virtual void ToJson(TSharedPtr<FJsonObject> jsonObject) const override
    {
        RavenCommand::ToJson(jsonObject);
        jsonObject->SetStringField(TEXT("Id"), id);
        if (!changeVector.IsEmpty())
        {
            jsonObject->SetStringField(TEXT("ChangeVector"), changeVector);
        }
        else
        {
            jsonObject->SetField(TEXT("ChangeVector"), MakeShareable(new FJsonValueNull()));
        }
    }
};

struct RavenCommandResponse
{
    virtual ~RavenCommandResponse() {}
    FString type;

    virtual void FromJson(TSharedPtr<FJsonObject> jsonObject)
    {
        jsonObject->TryGetStringField(TEXT("Type"), type);
    }
};

struct RavenBatchCommandResponse
{
    bool success = false;
    ResponseCode code = ResponseCode::UNKNOWN;
    TArray<RavenCommandResponse> results;
};

struct RavenDeleteCommandResponse : public RavenCommandResponse
{
    FString id;
    bool deleted;

    virtual void FromJson(TSharedPtr<FJsonObject> jsonObject) override
    {
        RavenCommandResponse::FromJson(jsonObject);
        jsonObject->TryGetStringField(TEXT("Id"), id);
        jsonObject->TryGetBoolField(TEXT("Deleted"), deleted);
    }
};

struct RavenPutCommandResponse : public RavenCommandResponse
{
    FString id;
    FString changeVector;

    virtual void FromJson(TSharedPtr<FJsonObject> jsonObject) override
    {
        RavenCommandResponse::FromJson(jsonObject);
        jsonObject->TryGetStringField(TEXT("@id"), id);
        jsonObject->TryGetStringField(TEXT("@change-vector"), changeVector);
    }
};
