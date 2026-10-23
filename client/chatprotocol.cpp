#include "chatprotocol.h"

#include <QJsonDocument>
#include <QJsonObject>

QByteArray ChatProtocol::BuildTextRequest(int to_uid, const QJsonArray& messages)
{
    QJsonObject request;
    request["to_uid"] = to_uid;
    request["text_array"] = messages;
    return QJsonDocument(request).toJson(QJsonDocument::Compact);
}

ChatProtocol::AppendResult ChatProtocol::TryAppendText(int to_uid, QJsonArray& batch, const QJsonObject& message)
{
    QJsonArray candidate = batch;
    candidate.append(message);
    if (BuildTextRequest(to_uid, candidate).size() <= MaxTextPayloadBytes) {
        batch = candidate;
        return AppendResult::Appended;
    }

    return batch.isEmpty() ? AppendResult::TooLarge : AppendResult::NeedNewBatch;
}
