#pragma once

#include <QImage>
#include <QString>

namespace head_test {

// Instrument faces drawn with QPainter.
// Aircraft: "asi", "ai", "alt", "hdg", "vsi", "tc". Car: "rpm", "kmh", "temp", "fuel".
QImage gauge_image(const QString& kind, int size = 512);

// Small rectangular panels: "mfd" (aircraft display), "lap" (race car dash display).
QImage panel_image(const QString& kind);

} // namespace head_test
