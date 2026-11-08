#include "gauges.hpp"

#include <cmath>

#include <QPainter>
#include <QPainterPath>
#include <QFont>

namespace head_test {

namespace {

constexpr double pi = 3.14159265358979;

struct dial_spec
{
    QString title, unit;
    double vmin, vmax;       // scale range
    double a0, a1;           // dial angles in degrees, clockwise from 12 o'clock
    double major, minor;     // tick steps
    double label_div = 1;    // label = value / label_div
    double needle;           // needle value
    QColor arc_green = QColor(0,0,0,0), arc_red = QColor(0,0,0,0);
    double green0 = 0, green1 = 0, red0 = 0, red1 = 0;
};

QPointF polar(QPointF c, double r, double deg)
{
    const double a = (deg - 90) * pi / 180;
    return { c.x() + r * std::cos(a), c.y() + r * std::sin(a) };
}

double angle_for(const dial_spec& s, double v)
{
    return s.a0 + (v - s.vmin) / (s.vmax - s.vmin) * (s.a1 - s.a0);
}

void bezel(QPainter& p, int sz)
{
    const QPointF c(sz / 2., sz / 2.);
    p.fillRect(0, 0, sz, sz, QColor(28, 28, 30));
    QRadialGradient g(c, sz / 2.);
    g.setColorAt(0.90, QColor(60, 60, 62));
    g.setColorAt(0.97, QColor(15, 15, 15));
    p.setBrush(g);
    p.setPen(Qt::NoPen);
    p.drawEllipse(c, sz / 2., sz / 2.);
    p.setBrush(QColor(12, 12, 14));
    p.drawEllipse(c, sz * .45, sz * .45);
}

void arc(QPainter& p, QPointF c, double r, double w, double a0, double a1, QColor col)
{
    if (col.alpha() == 0 || a1 <= a0)
        return;
    QPen pen(col, w, Qt::SolidLine, Qt::FlatCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    QRectF rr(c.x() - r, c.y() - r, 2 * r, 2 * r);
    // Qt: 0 deg = 3 o'clock, counter-clockwise positive, 1/16 deg units
    p.drawArc(rr, int((90 - a0) * 16), int(-(a1 - a0) * 16));
}

void needle(QPainter& p, QPointF c, double r, double deg, QColor col = QColor(240, 240, 240), double w = 9)
{
    QPainterPath path;
    const QPointF tip = polar(c, r, deg), tail = polar(c, -r * .18, deg);
    const QPointF side1 = polar(c, w, deg + 90), side2 = polar(c, w, deg - 90);
    path.moveTo(tip);
    path.lineTo(side1);
    path.lineTo(tail);
    path.lineTo(side2);
    path.closeSubpath();
    p.setPen(Qt::NoPen);
    p.setBrush(col);
    p.drawPath(path);
    p.setBrush(QColor(50, 50, 50));
    p.drawEllipse(c, w * 1.6, w * 1.6);
}

void dial(QPainter& p, int sz, const dial_spec& s)
{
    bezel(p, sz);
    const QPointF c(sz / 2., sz / 2.);
    const double R = sz * .42;
    p.setRenderHint(QPainter::Antialiasing);

    arc(p, c, R * .93, sz * .025, angle_for(s, s.green0), angle_for(s, s.green1), s.arc_green);
    arc(p, c, R * .93, sz * .025, angle_for(s, s.red0), angle_for(s, s.red1), s.arc_red);

    QFont f("Arial");
    f.setBold(true);
    f.setPixelSize(int(sz * .075));
    p.setFont(f);
    for (double v = s.vmin; v <= s.vmax + 1e-9; v += s.minor)
    {
        const bool major = std::fmod(std::fabs(v - s.vmin) + 1e-6, s.major) < 1e-3;
        const double a = angle_for(s, v);
        p.setPen(QPen(QColor(235, 235, 235), major ? sz * .012 : sz * .006));
        p.drawLine(polar(c, R, a), polar(c, R * (major ? .82 : .9), a));
        if (major)
        {
            const QPointF tp = polar(c, R * .66, a);
            QRectF tr(tp.x() - sz * .1, tp.y() - sz * .05, sz * .2, sz * .1);
            p.drawText(tr, Qt::AlignCenter, QString::number(v / s.label_div));
        }
    }
    f.setPixelSize(int(sz * .055));
    p.setFont(f);
    p.setPen(QColor(200, 200, 200));
    p.drawText(QRectF(0, sz * .60, sz, sz * .07), Qt::AlignCenter, s.title);
    f.setBold(false);
    f.setPixelSize(int(sz * .045));
    p.setFont(f);
    p.drawText(QRectF(0, sz * .67, sz, sz * .07), Qt::AlignCenter, s.unit);

    needle(p, c, R * .9, angle_for(s, s.needle));
}

void attitude(QPainter& p, int sz)
{
    bezel(p, sz);
    const QPointF c(sz / 2., sz / 2.);
    const double R = sz * .43;
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath clip;
    clip.addEllipse(c, R, R);
    p.setClipPath(clip);
    p.save();
    p.translate(c);
    p.rotate(-6);            // slight bank
    p.translate(0, sz * .03); // slight nose-up
    p.fillRect(QRectF(-sz, -sz, 2 * sz, sz), QColor(40, 120, 205));
    p.fillRect(QRectF(-sz, 0, 2 * sz, sz), QColor(130, 85, 45));
    p.setPen(QPen(Qt::white, sz * .008));
    p.drawLine(QPointF(-sz, 0), QPointF(sz, 0));
    for (int k = -3; k <= 3; k++)
    {
        if (!k) continue;
        const double y = -k * sz * .06, w = (k % 2) ? sz * .07 : sz * .13;
        p.drawLine(QPointF(-w, y), QPointF(w, y));
    }
    p.restore();
    p.setClipping(false);
    // aircraft symbol
    p.setPen(QPen(QColor(255, 200, 0), sz * .02, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(c.x() - sz * .2, c.y()), QPointF(c.x() - sz * .07, c.y()));
    p.drawLine(QPointF(c.x() + sz * .07, c.y()), QPointF(c.x() + sz * .2, c.y()));
    p.drawPoint(c);
    // bank scale
    p.setPen(QPen(Qt::white, sz * .01));
    for (int a : { -60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60 })
        p.drawLine(polar(c, R, a), polar(c, R * (a % 30 ? .93 : .88), a));
}

void heading(QPainter& p, int sz)
{
    bezel(p, sz);
    const QPointF c(sz / 2., sz / 2.);
    const double R = sz * .42;
    p.setRenderHint(QPainter::Antialiasing);
    QFont f("Arial");
    f.setBold(true);
    f.setPixelSize(int(sz * .08));
    p.setFont(f);
    const double hdg = 35;
    for (int d = 0; d < 360; d += 5)
    {
        const double a = d - hdg;
        const bool major = d % 30 == 0;
        p.setPen(QPen(Qt::white, major ? sz * .01 : sz * .005));
        p.drawLine(polar(c, R, a), polar(c, R * (d % 10 ? .93 : .86), a));
        if (major)
        {
            static const char* names[] = { "N", "3", "6", "E", "12", "15", "S", "21", "24", "W", "30", "33" };
            p.save();
            p.translate(polar(c, R * .72, a));
            p.rotate(a);
            p.drawText(QRectF(-sz * .08, -sz * .05, sz * .16, sz * .1), Qt::AlignCenter, names[d / 30]);
            p.restore();
        }
    }
    // lubber line + plane
    p.setPen(QPen(QColor(255, 140, 0), sz * .015));
    p.drawLine(polar(c, R * 1.02, 0), polar(c, R * .8, 0));
    p.setPen(QPen(QColor(255, 200, 0), sz * .02, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(c.x(), c.y() - sz * .14), QPointF(c.x(), c.y() + sz * .14));
    p.drawLine(QPointF(c.x() - sz * .12, c.y() - sz * .02), QPointF(c.x() + sz * .12, c.y() - sz * .02));
    p.drawLine(QPointF(c.x() - sz * .05, c.y() + sz * .12), QPointF(c.x() + sz * .05, c.y() + sz * .12));
}

void turn(QPainter& p, int sz)
{
    bezel(p, sz);
    const QPointF c(sz / 2., sz / 2.);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::white, sz * .012));
    for (int a : { -110, -90, 90, 110 })
        p.drawLine(polar(c, sz * .42, a), polar(c, sz * .34, a));
    QFont f("Arial");
    f.setBold(true);
    f.setPixelSize(int(sz * .07));
    p.setFont(f);
    p.drawText(QRectF(0, sz * .22, sz, sz * .1), Qt::AlignCenter, "TURN COORDINATOR");
    p.drawText(QRectF(sz * .08, sz * .55, sz * .15, sz * .1), Qt::AlignCenter, "L");
    p.drawText(QRectF(sz * .77, sz * .55, sz * .15, sz * .1), Qt::AlignCenter, "R");
    // ball
    p.setBrush(QColor(60, 60, 60));
    p.setPen(QPen(Qt::white, sz * .006));
    p.drawRoundedRect(QRectF(sz * .3, sz * .7, sz * .4, sz * .08), sz * .04, sz * .04);
    p.setBrush(Qt::black);
    p.drawEllipse(QPointF(c.x(), sz * .74), sz * .035, sz * .035);
    // aircraft
    p.setPen(QPen(Qt::white, sz * .025, Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(c.x() - sz * .3, c.y()), QPointF(c.x() + sz * .3, c.y()));
    p.setBrush(Qt::white);
    p.drawEllipse(c, sz * .045, sz * .045);
}

} // namespace

QImage gauge_image(const QString& kind, int sz)
{
    QImage img(sz, sz, QImage::Format_RGBA8888);
    img.fill(Qt::black);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);

    if (kind == "ai")
        attitude(p, sz);
    else if (kind == "hdg")
        heading(p, sz);
    else if (kind == "tc")
        turn(p, sz);
    else
    {
        dial_spec s {};
        if (kind == "asi")
            s = { "AIRSPEED", "KNOTS", 0, 200, -150, 150, 20, 10, 1, 112 };
        else if (kind == "alt")
            s = { "ALT", "FEET x100", 0, 100, 0, 360, 10, 2, 10, 50 };
        else if (kind == "vsi")
            s = { "VERT SPEED", "x1000 FT/MIN", -2000, 2000, -170, -10, 500, 100, 1000, 0 };
        else if (kind == "rpm")
            s = { "RPM", "x1000", 0, 9000, -135, 135, 1000, 500, 1000, 4200 };
        else if (kind == "kmh")
            s = { "SPEED", "KM/H", 0, 300, -135, 135, 20, 10, 1, 0 };
        else if (kind == "temp")
            s = { "WATER", "C", 40, 120, -60, 60, 20, 10, 1, 88 };
        else if (kind == "fuel")
            s = { "FUEL", "", 0, 4, -60, 60, 1, 0.5, 1, 3.2 };
        if (kind == "asi")
        {
            s.arc_green = QColor(40, 190, 60); s.green0 = 50; s.green1 = 150;
            s.arc_red = QColor(220, 40, 30); s.red0 = 180; s.red1 = 200;
        }
        else if (kind == "rpm")
        {
            s.arc_red = QColor(220, 40, 30); s.red0 = 7500; s.red1 = 9000;
        }
        else if (kind == "kmh")
            s.major = 40;
        else if (kind == "vsi")
        {
            // zero at 9 o'clock, climb clockwise
            s.a0 = 270 - 170; s.a1 = 270 + 170;
            s.major = 1000;
        }
        dial(p, sz, s);
    }
    return img;
}

QImage panel_image(const QString& kind)
{
    const int w = 512, h = 256;
    QImage img(w, h, QImage::Format_RGBA8888);
    img.fill(QColor(8, 10, 12));
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f("Consolas");
    f.setStyleHint(QFont::Monospace);
    f.setBold(true);

    if (kind == "lap")
    {
        // shift lights
        for (int i = 0; i < 10; i++)
        {
            QColor col = i < 4 ? QColor(40, 220, 60) : i < 7 ? QColor(255, 200, 0) : QColor(255, 40, 30);
            if (i > 5) col = col.darker(300);
            p.setBrush(col);
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(46 + i * 46, 30), 14, 14);
        }
        f.setPixelSize(120);
        p.setFont(f);
        p.setPen(QColor(255, 255, 255));
        p.drawText(QRect(0, 50, w / 2, 160), Qt::AlignCenter, "3");
        f.setPixelSize(38);
        p.setFont(f);
        p.setPen(QColor(120, 230, 255));
        p.drawText(QRect(w / 2, 60, w / 2, 50), Qt::AlignCenter, "LAP 4/12");
        p.setPen(QColor(255, 220, 60));
        p.drawText(QRect(w / 2, 115, w / 2, 50), Qt::AlignCenter, "1:31.408");
        p.setPen(QColor(90, 255, 120));
        p.drawText(QRect(w / 2, 170, w / 2, 50), Qt::AlignCenter, "-0.214");
    }
    else // "mfd"
    {
        p.setPen(QPen(QColor(60, 255, 120), 3));
        p.setBrush(Qt::NoBrush);
        p.drawRect(10, 10, w - 20, h - 20);
        // simple moving map: range rings + track
        const QPointF c(w * .35, h * .55);
        for (int r = 40; r <= 160; r += 40)
            p.drawEllipse(c, r, r);
        p.setPen(QPen(QColor(255, 0, 255), 4));
        p.drawLine(c, QPointF(c.x() + 60, c.y() - 160));
        p.setBrush(QColor(60, 255, 120));
        p.setPen(Qt::NoPen);
        QPolygonF plane;
        plane << QPointF(c.x(), c.y() - 12) << QPointF(c.x() + 9, c.y() + 10) << QPointF(c.x() - 9, c.y() + 10);
        p.drawPolygon(plane);
        f.setPixelSize(26);
        p.setFont(f);
        p.setPen(QColor(60, 255, 120));
        p.drawText(QRect(w * .62, 30, w * .35, 200), Qt::AlignLeft | Qt::AlignTop,
                   "GS  112\nALT 5000\nHDG 035\nWPT KILO\nDST 12.4");
    }
    return img;
}

} // namespace head_test
