#pragma once

#include <QString>
#include <QTextBoundaryFinder>

inline QString ClampTextToMaxCharacters(const QString& text, int max_characters)
{
    if (max_characters <= 0 || text.size() <= max_characters) {
        return text;
    }

    QTextBoundaryFinder finder(QTextBoundaryFinder::Grapheme, text);
    int count = 0;
    int boundary = 0;
    while ((boundary = finder.toNextBoundary()) >= 0) {
        ++count;
        if (count == max_characters) {
            return text.left(boundary);
        }
    }
    return text;
}
