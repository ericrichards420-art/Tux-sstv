#pragma once
#include <QImage>
#include <QList>
#include <QString>
namespace sstv {
struct TestPattern {
    QString id,name,era,description;
    bool monochrome=false;
};
const QList<TestPattern>& testPatterns();
const TestPattern* testPatternById(const QString& id);
// Original drawings for SSTV image comparisons, not composite-video generators.
// "test-pattern" returns the original 640x496 image without any pixel changes.
QImage renderTestPattern(const QString& id);
}
