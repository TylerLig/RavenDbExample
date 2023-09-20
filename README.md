# RavenDbHttpInterface

Contains files that allow you to interact with the RavenDB REST API in Unreal Engine. By default, the URL for the RavenDB server is the local RavenDb URL `http://127.0.0.1:8080`, connecting to local instances of RavenDb.

`PrivateDependencyModuleNames` or `PublicDependencyModuleNames` should include `HTTP` and `Json` in .Build.cs
```cs
PrivateDependencyModuleNames.AddRange(
            new string[] {         
                /* other private dependency names */       
                "HTTP",
                "Json",
            }
```

## Using your Database Names

In `RavenDbRequestResponse.h` modify `DatabaseSelector` and the `DatabaseNamesMap` to include your database names:
```cpp
enum DatabaseSelector : uint8
{
    EXAMPLE_DATABASE_NAME, // Add your custom database enum values here
};
static const TMap<DatabaseSelector, FString> DatabaseNamesMap = {
    { DatabaseSelector::EXAMPLE_DATABASE_NAME, TEXT("ExampleDatabaseName") },  // Add your custom database names here
};
```

## API Functions

### 1. AtomicUpdate

Performs an atomic update on the selected RavenDB database, executing a set of commands within a single batch operation. Returns a `RavenBatchCommandResponse` with the results of each command executed. Currently, `PUT` and `DELETE` commands are supported.

```cpp
RavenBatchCommandRequest commandRequest;

TSharedPtr<RavenPutCommand> putCommand = MakeShared<RavenPutCommand>();
putCommand->id = "doc1";
putCommand->document = /* Your FJsonObject */;
commandRequest.commands.Add(putCommand);

TSharedPtr<RavenDeleteCommand> deleteCommand = MakeShared<RavenDeleteCommand>();
deleteCommand->id = "doc2";
deleteCommand->changeVector = "A:1"; //optional
commandRequest.commands.Add(deleteCommand);

DatabaseSelector databaseSelection = DatabaseSelector::EXAMPLE_DATABASE_NAME;

TFuture<RavenBatchCommandResponse> atomicUpdateFuture = RavenDbHttpInterface::AtomicUpdate(commandRequest, databaseSelection);
atomicUpdateFuture.Next([](const RavenBatchCommandResponse& response) {
    if (response.success) {
        // Atomic update was successful
    } else {
        // Atomic update failed
    }
});
```

### 2. GetDocumentsRequest

Retrieves one or more documents from the RavenDB server.

```cpp
TArray<FString> documentIds = { "docId1", "docId2", "docId3" };

TFuture<GetDocumentsResponse> getDocumentsFuture = RavenDbHttpInterface::GetDocumentsRequest(documentIds, DatabaseSelector::EXAMPLE_DATABASE_NAME);
getDocumentsFuture.Next([](const GetDocumentsResponse& response) {
    if (response.success) {
        // Documents were retrieved successfully, process the JSON
    } else {
        // Retrieving documents failed
    }
});
```

```cpp
TFuture<GetDocumentsResponse> getDocumentsFuture = RavenDbHttpInterface::GetDocumentsRequest({ "docId1" }, DatabaseSelector::EXAMPLE_DATABASE_NAME);
getDocumentsFuture.Next([](const GetDocumentsResponse& response) {
    if (response.success) {
        // Document was retrieved successfully, process the JSON
    } else {
        // Retrieving the document failed
    }
});
```

### 3. UpdateDocumentRequest

Add or update a document in the RavenDB server.

```cpp
FString documentId = "docId";
FString documentJson = /* Your document JSON string */;

TFuture<UpdateDocumentResponse> updateDocumentFuture = RavenDbHttpInterface::UpdateDocumentRequest(documentId, documentJson, DatabaseSelector::EXAMPLE_DATABASE_NAME);
updateDocumentFuture.Next([](const UpdateDocumentResponse& response) {
    if (response.success) {
        // Document was updated/created successfully
    } else {
        // Updating/creating the document failed
    }
});
```

```cpp
FString documentId = "docId";
FString documentJson = /* Your document JSON string */;

RavenDbHttpInterface::UpdateDocumentRequest(documentId, documentJson, DatabaseSelector::EXAMPLE_DATABASE_NAME);
```

### 4. DeleteDocumentRequest (without expectedChangeVector)

Deletes a document from the RavenDB server.

```cpp
FString documentId = "docId";

TFuture<void> deleteDocumentFuture = RavenDbHttpInterface::DeleteDocumentRequest(documentId, DatabaseSelector::EXAMPLE_DATABASE_NAME);
deleteDocumentFuture.Next([] {
    // The document was successfully deleted, or no document with the specified ID exists.
});
```

```cpp
RavenDbHttpInterface::DeleteDocumentRequest("docId", DatabaseSelector::EXAMPLE_DATABASE_NAME);
```

### 5. DeleteDocumentRequest (with expectedChangeVector)

Deletes a document from the RavenDB server if the provided change vector matches the server's change vector.

```cpp
FString documentId = "docId";
FString expectedChangeVector = /* Your expected change vector */;

TFuture<bool> deleteDocumentFuture = RavenDbHttpInterface::DeleteDocumentRequest(documentId, expectedChangeVector, DatabaseSelector::EXAMPLE_DATABASE_NAME);
deleteDocumentFuture.Next([](bool isDeleted) {
    if (isDeleted) {
        // Document has been deleted
    } else {
        // Document has not been deleted due to mismatched change vector
    }
});
```

### 6. SendRequestAndMeasureLatency

This function can measure the latency of a request.

Example using UpdateDocumentRequest:
```cpp
FString documentId = "docId";
FString documentJson = /* Your document JSON */;

TFuture<UpdateDocumentResponse> updateDocumentRequestFunction = [&documentId, &documentJson]() -> TFuture<UpdateDocumentResponse> {
    return RavenDbHttpInterface::UpdateDocumentRequest(documentId, documentJson, DatabaseSelector::EXAMPLE_DATABASE_NAME);
};

TFuture<TimedDatabaseResponse<UpdateDocumentResponse>> timedUpdateDocumentFuture = RavenDbHttpInterface::SendRequestAndMeasureLatency(updateDocumentRequestFunction);
timedUpdateDocumentFuture.Next([](const TimedDatabaseResponse<UpdateDocumentResponse>& timedResponse) {
    double latency = timedResponse.latency;
    UpdateDocumentResponse response = timedResponse.response;
    // Process the response and latency
});
```