#include "software-update-dialog.hpp"
#include "opentrack-org.hxx"

#include <QRegularExpression>
#include <QVersionNumber>

namespace {

// Trackhat version strings look like "trackhat-opentrack-3.3", optionally with a pre-release
// suffix ("-rc1", "-beta2") and/or a git-describe suffix ("-12-gabcdef12").
struct th_version
{
    QVersionNumber number;
    bool prerelease = false;
    bool valid = false;
};

th_version parse_th_version(const QString& s)
{
    static const QRegularExpression re(QStringLiteral(R"(^trackhat-opentrack-(\d+(?:\.\d+)*)(-(?:rc|beta|alpha)\d*)?)"));
    th_version v;
    const QRegularExpressionMatch m = re.match(s);
    if (m.hasMatch())
    {
        v.number = QVersionNumber::fromString(m.captured(1));
        v.prerelease = m.hasCaptured(2);
        v.valid = !v.number.isNull();
    }
    return v;
}

// Only offer an update when the advertised release is newer than what's running.
// The old string-inequality check nagged on every build that wasn't byte-identical to the
// advertised version (e.g. a newer build while the site still says 3.2).
bool is_newer_release(const QString& advertised, const QString& running)
{
    const th_version adv = parse_th_version(advertised), cur = parse_th_version(running);
    if (!adv.valid)
        return false;  // unknown scheme on the website; don't nag
    if (!cur.valid)
        return false;  // untagged developer build
    const int c = QVersionNumber::compare(adv.number, cur.number);
    if (c != 0)
        return c > 0;
    return cur.prerelease && !adv.prerelease;  // e.g. running 3.3-rc1, site says 3.3
}

} // namespace

update_dialog::update_dialog(QWidget* parent, update_query& q, const QString& new_version)
    : QDialog(parent), q(q)
{
    ui.setupUi(this);
    ui.ver_current->setText(const_cast<const char*>(opentrack_version));
    ui.ver_new->setTextFormat(Qt::RichText);
    ui.ver_new->setText("<a href='https://www.trackhat.org/trackhat-opentrack'>" + new_version + "</a>");
    ui.ver_new->setOpenExternalLinks(true);
    connect(ui.buttonBox, &QDialogButtonBox::clicked, this, &update_dialog::close);
}

void update_query::on_finished()
{
    if (!t.isActive())
        return;
    t.stop();
    if (r->error() != QNetworkReply::NoError)
    {
        qDebug() << "updater: error" << r->error() << r->errorString();
        return;
    }
    QString str(buf);
    static const QRegularExpression re(QStringLiteral("SOFTWARE-UPDATE-V3: ([a-zA-Z0-9_.+-]+)"));
    const QRegularExpressionMatch match = re.match(str);
    if (match.hasMatch())
    {
        str = match.captured(1);
        QSettings s(OPENTRACK_ORG);
        QString quiet_version = s.value("quiet-update-version").toString();

        if (!str.isEmpty() && str != quiet_version && is_newer_release(str, QString::fromUtf8(opentrack_version)))
        {
            qDebug() << "updater: new version" << str;
            update_dialog dlg(qobject_cast<QWidget*>(parent()), *this, str);
            dlg.show();
            dlg.raise();
            dlg.exec();
            if (dlg.ui.disable_reminder->isChecked())
                s.setValue("quiet-update-version", str);
        }
    }
    else
    {
        if (buf.isEmpty())
            qDebug() << "updater: empty response";
        else
            qDebug() << "updater: can't parse response";
    }
    buf.clear();
    r->deleteLater();
}

void update_query::maybe_show_dialog()
{
    t.stop();
    t.setSingleShot(true);
    t.start(1000 * 10);

    r = qnam.get(QNetworkRequest(QStringLiteral("https://www.trackhat.org/thotv3-version")));

    QObject::connect(r, &QIODevice::readyRead, this, &update_query::on_ready);
    QObject::connect(r, &QNetworkReply::finished, this, &update_query::on_finished);
}
