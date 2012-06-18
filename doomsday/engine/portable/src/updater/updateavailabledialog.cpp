#include "updateavailabledialog.h"
#include "updatersettings.h"
#include "updatersettingsdialog.h"
#include "versioninfo.h"
#include "window.h"
#include <de/Log>
#include <QUrl>
#include <QDesktopServices>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QPushButton>
#include <QStackedLayout>
#include <QFont>
#include <QLabel>

struct UpdateAvailableDialog::Instance
{
    UpdateAvailableDialog* self;
    QStackedLayout* stack;
    QWidget* checkPage;
    QWidget* resultPage;
    bool emptyResultPage;
    QVBoxLayout* resultLayout;
    QLabel* checking;
    QLabel* info;
    QCheckBox* neverCheck;
    VersionInfo latestVersion;
    de::String changeLog;

    Instance(UpdateAvailableDialog* d) : self(d)
    {
        initForChecking();
    }

    Instance(UpdateAvailableDialog* d, const VersionInfo& latest) : self(d)
    {
        initForResult(latest);
    }

    void initForChecking(void)
    {
        init();
        createResultPage(VersionInfo());

        stack->addWidget(checkPage);
        stack->addWidget(resultPage);
    }

    void initForResult(const VersionInfo& latest)
    {
        init();
        createResultPage(latest);

        stack->addWidget(resultPage);
        stack->addWidget(checkPage);
    }

    void init()
    {
        self->setWindowTitle(tr("Doomsday %1").arg(VersionInfo().asText()));
        self->setWindowFlags(self->windowFlags() & ~Qt::WindowContextHelpButtonHint);

        emptyResultPage = true;
        stack = new QStackedLayout(self);
        checkPage = new QWidget(self);
        resultPage = new QWidget(self);

#ifdef MACOSX
        // Adjust spacing around all stacked widgets.
        self->setContentsMargins(9, 9, 9, 9);
#endif

        // Create the Check page.
        QVBoxLayout* checkLayout = new QVBoxLayout;
        checkPage->setLayout(checkLayout);

        checking = new QLabel(tr("Checking for available updates..."));
        checkLayout->addWidget(checking, 1, Qt::AlignCenter);

        QPushButton* stop = new QPushButton(tr("Cancel"));
        QObject::connect(stop, SIGNAL(clicked()), self, SLOT(reject()));
        checkLayout->addWidget(stop, 0, Qt::AlignHCenter);
        stop->setAutoDefault(false);
        stop->setDefault(false);

        self->setLayout(stack);
    }

    void updateResult(const VersionInfo& latest)
    {
        createResultPage(latest);
        stack->setCurrentWidget(resultPage);
    }

    void createResultPage(const VersionInfo& latest)
    {
        latestVersion = latest;

        // Get rid of the existing page.
        if(!emptyResultPage)
        {
            stack->removeWidget(resultPage);
            delete resultPage;
            resultPage = new QWidget(self);
            stack->addWidget(resultPage);
        }
        emptyResultPage = false;

        resultLayout = new QVBoxLayout;
        resultPage->setLayout(resultLayout);

        info = new QLabel;
        info->setTextFormat(Qt::RichText);

        VersionInfo currentVersion;
        int bigFontSize = self->font().pointSize() * 1.2;
        de::String channel = (UpdaterSettings().channel() == UpdaterSettings::Stable?
                                  "stable" : "unstable");
        bool askUpgrade = false;
        bool askDowngrade = false;

        if(latestVersion > currentVersion)
        {
            askUpgrade = true;
            info->setText(("<span style=\"font-weight:bold; font-size:%1pt;\">" +
                           tr("There is an update available.") + "</span><p>" +
                           tr("The latest %2 release is %3, while you are running %4."))
                          .arg(bigFontSize)
                          .arg(channel)
                          .arg(latestVersion.asText())
                          .arg(currentVersion.asText()));
        }
        else if(!channel.compareWithoutCase(DOOMSDAY_RELEASE_TYPE)) // same release type
        {
            info->setText(("<span style=\"font-weight:bold; font-size:%1pt;\">" +
                           tr("You are up to date.") + "</span><p>" +
                           tr("The installed %2 is the latest available %3 build."))
                          .arg(bigFontSize)
                          .arg(currentVersion.asText())
                          .arg(channel));
        }
        else if(latestVersion < currentVersion)
        {
            askDowngrade = true;
            info->setText(("<span style=\"font-weight:bold; font-size:%1pt;\">" +
                           tr("You are up to date.") + "</span><p>" +
                           tr("The installed %2 is newer than the latest available %3 build."))
                          .arg(bigFontSize)
                          .arg(currentVersion.asText())
                          .arg(channel));
        }

        neverCheck = new QCheckBox(tr("N&ever check for updates automatically"));
        neverCheck->setChecked(UpdaterSettings().onlyCheckManually());
        QObject::connect(neverCheck, SIGNAL(toggled(bool)), self, SLOT(neverCheckToggled(bool)));

        QDialogButtonBox* bbox = new QDialogButtonBox;

        if(askDowngrade)
        {
            QPushButton* yes = bbox->addButton(tr("Downgrade to &Older"), QDialogButtonBox::YesRole);
            QPushButton* no = bbox->addButton(tr("&Close"), QDialogButtonBox::RejectRole);
            QObject::connect(yes, SIGNAL(clicked()), self, SLOT(accept()));
            QObject::connect(no, SIGNAL(clicked()), self, SLOT(reject()));
            no->setDefault(true);
        }
        else if(askUpgrade)
        {
            QPushButton* yes = bbox->addButton(tr("&Upgrade"), QDialogButtonBox::YesRole);
            QPushButton* no = bbox->addButton(tr("&Not Now"), QDialogButtonBox::NoRole);
            QObject::connect(yes, SIGNAL(clicked()), self, SLOT(accept()));
            QObject::connect(no, SIGNAL(clicked()), self, SLOT(reject()));
            yes->setDefault(true);
        }
        else
        {
            QPushButton* reinst = bbox->addButton(tr("&Reinstall"), QDialogButtonBox::YesRole);
            QPushButton* ok = bbox->addButton(tr("&Close"), QDialogButtonBox::RejectRole);
            QObject::connect(reinst, SIGNAL(clicked()), self, SLOT(accept()));
            QObject::connect(ok, SIGNAL(clicked()), self, SLOT(reject()));
            reinst->setAutoDefault(false);
            ok->setDefault(true);
        }

        QPushButton* cfg = bbox->addButton(tr("&Settings..."), QDialogButtonBox::ActionRole);
        QObject::connect(cfg, SIGNAL(clicked()), self, SLOT(editSettings()));
        cfg->setAutoDefault(false);

        if(askUpgrade)
        {
            QPushButton* whatsNew = bbox->addButton(tr("&What's New?"), QDialogButtonBox::HelpRole);
            QObject::connect(whatsNew, SIGNAL(clicked()), self, SLOT(showWhatsNew()));
            whatsNew->setAutoDefault(false);
        }

        resultLayout->addWidget(info);
        resultLayout->addWidget(neverCheck);
        resultLayout->addWidget(bbox);
    }
};

UpdateAvailableDialog::UpdateAvailableDialog(QWidget *parent) : QDialog(parent)
{
    d = new Instance(this);
}

UpdateAvailableDialog::UpdateAvailableDialog(const VersionInfo& latestVersion, de::String changeLogUri,
                                             QWidget *parent)
    : QDialog(parent)
{
    d = new Instance(this, latestVersion);
    d->changeLog = changeLogUri;
}

UpdateAvailableDialog::~UpdateAvailableDialog()
{
    delete d;
}

void UpdateAvailableDialog::showResult(const VersionInfo& latestVersion, de::String changeLogUri)
{
    d->changeLog = changeLogUri;
    d->updateResult(latestVersion);
}

void UpdateAvailableDialog::neverCheckToggled(bool set)
{
    LOG_DEBUG("Never check for updates: %b") << set;
    UpdaterSettings().setOnlyCheckManually(set);
}

void UpdateAvailableDialog::showWhatsNew()
{
    QDesktopServices::openUrl(QUrl(d->changeLog));
}

void UpdateAvailableDialog::editSettings()
{
    UpdaterSettingsDialog st(this);
    if(st.exec())
    {
        d->neverCheck->setChecked(UpdaterSettings().onlyCheckManually());

        d->stack->setCurrentWidget(d->checkPage);

        // Rerun the check.
        emit checkAgain();
    }
}
