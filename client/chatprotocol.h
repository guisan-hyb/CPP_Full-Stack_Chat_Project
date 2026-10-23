#pragma once

#include <QByteArray>
#include <QJsonArray>

namespace ChatProtocol {

constexpr int MaxTextPayloadBytes = 2048;

enum class AppendResult {
    Appended,
    NeedNewBatch,
    TooLarge
};

QByteArray BuildTextRequest(int to_uid, const QJsonArray& messages);
AppendResult TryAppendText(int to_uid, QJsonArray& batch, const QJsonObject& message);

}
