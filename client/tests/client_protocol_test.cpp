#include <QCoreApplication>
#include <QJsonObject>
#include <QTextStream>

#include "chatprotocol.h"
#include "textutils.h"

namespace {

bool Expect(bool condition, const QString& message)
{
    if (!condition) {
        QTextStream(stderr) << "FAILED: " << message << Qt::endl;
        return false;
    }
    return true;
}

QJsonObject TextMessage(const QString& id, const QString& content)
{
    QJsonObject message;
    message["unique_id"] = id;
    message["content"] = content;
    return message;
}

}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    const QString mixed = QString::fromUtf8("A😀中文");
    const QString clamped = ClampTextToMaxCharacters(mixed, 3);
    if (!Expect(clamped == QString::fromUtf8("A😀中"), "UTF-8 safe character clamp")) {
        return 1;
    }

    QJsonArray batch;
    const auto first = ChatProtocol::TryAppendText(
        1001,
        batch,
        TextMessage("first", QString(1000, QLatin1Char('a'))));
    if (!Expect(first == ChatProtocol::AppendResult::Appended, "first message should fit")) {
        return 1;
    }

    const auto second = ChatProtocol::TryAppendText(
        1001,
        batch,
        TextMessage("second", QString(1000, QLatin1Char('b'))));
    if (!Expect(second == ChatProtocol::AppendResult::NeedNewBatch, "second message should start a new batch")) {
        return 1;
    }
    if (!Expect(batch.size() == 1, "failed append must not mutate the current batch")) {
        return 1;
    }

    const auto payload = ChatProtocol::BuildTextRequest(1001, batch);
    if (!Expect(payload.size() <= ChatProtocol::MaxTextPayloadBytes, "payload must fit 2048 bytes")) {
        return 1;
    }

    QJsonArray oversized_batch;
    const auto oversized = ChatProtocol::TryAppendText(
        1001,
        oversized_batch,
        TextMessage("oversized", QString(1024, QChar(0x4E2D))));
    if (!Expect(oversized == ChatProtocol::AppendResult::TooLarge, "oversized single message must be rejected")) {
        return 1;
    }
    if (!Expect(oversized_batch.isEmpty(), "oversized message must not be partially appended")) {
        return 1;
    }

    QTextStream(stdout) << "client protocol tests passed" << Qt::endl;
    return 0;
}
