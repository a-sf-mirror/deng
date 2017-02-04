/** @file packagepopupwidget.cpp  Popup showing information about a package.
 *
 * @authors Copyright (c) 2016 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#include "ui/dialogs/packageinfodialog.h"
#include "ui/widgets/packagecontentoptionswidget.h"
#include "resource/idtech1image.h"
#include "dd_main.h"

#include <doomsday/DataBundle>
#include <doomsday/DoomsdayApp>
#include <doomsday/Games>
#include <doomsday/LumpCatalog>

#include <de/App>
#include <de/ArchiveEntryFile>
#include <de/CallbackAction>
#include <de/DirectoryFeed>
#include <de/DocumentWidget>
#include <de/Folder>
#include <de/ImageFile>
#include <de/LabelWidget>
#include <de/Loop>
#include <de/NativeFile>
#include <de/PackageLoader>
#include <de/PopupMenuWidget>
#include <de/SequentialLayout>
#include <de/SignalAction>

#include <QDesktopServices>
#include <QRegularExpression>

using namespace de;

DENG_GUI_PIMPL(PackageInfoDialog)
{
    LabelWidget *title;
    LabelWidget *path;
    DocumentWidget *description;
    LabelWidget *icon;
    LabelWidget *metaInfo;
    IndirectRule *targetHeight;
    IndirectRule *descriptionWidth;
    IndirectRule *minContentHeight;
    String packageId;
    String compatibleGame; // guessed
    NativePath nativePath;
    SafeWidgetPtr<PopupWidget> configurePopup;
    SafeWidgetPtr<PopupMenuWidget> profileMenu;
    enum MenuMode { AddToProfile, PlayInProfile };
    MenuMode menuMode;

    Impl(Public *i) : Base(i)
    {
        targetHeight = new IndirectRule;
        descriptionWidth = new IndirectRule;
        minContentHeight = new IndirectRule;

        self().useInfoStyle();

        // The Close button is always available. Other actions are shown depending
        // on what kind of package is being displayed.
        self().buttons()
                << new DialogButtonItem(Default | Accept, tr("Close"));

        createWidgets();
    }

    ~Impl()
    {
        releaseRef(targetHeight);
        releaseRef(descriptionWidth);
        releaseRef(minContentHeight);
    }

    void createWidgets()
    {
        auto &area = self().area();

        // Left column.
        title = LabelWidget::newWithText("", &area);
        title->setFont("title");
        title->setSizePolicy(ui::Filled, ui::Expand);
        title->setTextColor("inverted.accent");
        title->setTextLineAlignment(ui::AlignLeft);
        title->margins().setBottom("");

        path = LabelWidget::newWithText("", &area);
        path->setSizePolicy(ui::Filled, ui::Expand);
        path->setTextColor("inverted.text");
        path->setTextLineAlignment(ui::AlignLeft);
        path->margins().setTop("unit");

        AutoRef<Rule> contentHeight(OperatorRule::maximum(*minContentHeight, *targetHeight));

        description = new DocumentWidget;
        description->setFont("small");
        description->setWidthPolicy(ui::Fixed);
        description->rule().setInput(Rule::Height, contentHeight - title->rule().height()
                                     - path->rule().height());
        area.add(description);

        SequentialLayout layout(area.contentRule().left(),
                                area.contentRule().top(),
                                ui::Down);
        layout.setOverrideWidth(*descriptionWidth);
        layout << *title
               << *path
               << *description;

        // Right column.
        icon = LabelWidget::newWithText("", &area);
        icon->setSizePolicy(ui::Filled, ui::Filled);
        //icon->setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
        //icon->setStyleImage("package.large");
        //icon->setImageColor(style().colors().colorf("inverted.accent"));
        icon->rule().setInput(Rule::Height, rule("dialog.packageinfo.icon.height"));

        metaInfo = LabelWidget::newWithText("", &area);
        metaInfo->setSizePolicy(ui::Filled, ui::Expand);
        metaInfo->setTextLineAlignment(ui::AlignLeft);
        metaInfo->setFont("small");
        metaInfo->setTextColor("inverted.altaccent");

        SequentialLayout rightLayout(title->rule().right(), title->rule().top(), ui::Down);
        rightLayout.setOverrideWidth(rule("dialog.packageinfo.metadata.width"));
        rightLayout << *icon
                    << *metaInfo;

        targetHeight->setSource(rightLayout.height());
        area.setContentSize(layout.width() + rightLayout.width(), contentHeight);
    }

    void setPackageIcon(Image const &iconImage)
    {
        icon->setImage(iconImage);
        icon->setImageColor(Vector4f(1, 1, 1, 1));
        icon->setImageFit(ui::FitToWidth | ui::OriginalAspectRatio);
        icon->setImageScale(1);
        icon->setBehavior(ContentClipping, true);
    }

    bool useGameTitlePicture(DataBundle const &bundle)
    {
        auto *lumpDir = bundle.lumpDirectory();
        if (!lumpDir || (!lumpDir->has("TITLEPIC") && !lumpDir->has("TITLE")))
        {
            return false;
        }

        Game const &game = Games::get()[compatibleGame];

        res::LumpCatalog catalog;
        catalog.setPackages(game.requiredPackages() + StringList({ packageId }));
        Image img = IdTech1Image::makeGameLogo(game,
                                               catalog,
                                               IdTech1Image::NullImageIfFails |
                                               IdTech1Image::UnmodifiedAppearance);
        if (!img.isNull())
        {
            setPackageIcon(img);
            return true;
        }
        return false;
    }

    void useDefaultIcon()
    {
        icon->setStyleImage("package.large");
        icon->setImageColor(style().colors().colorf("inverted.altaccent"));
        icon->setImageFit(ui::FitToSize | ui::OriginalAspectRatio);
        //icon->setImageScale(.75f);
        icon->setOpacity(.5f);
        icon->setBehavior(ContentClipping, false);
    }

    void useIconFile(String const &packagePath)
    {
        try
        {
            static StringList const imageExts({ ".jpg", ".jpeg", ".png" });
            foreach (String ext, imageExts)
            {
                String const imgPath = packagePath / "icon" + ext;
                if (ImageFile const *img = FS::get().root().tryLocate<ImageFile const>(imgPath))
                {
                    Image iconImage = img->image();
                    if (iconImage.width() > 512 || iconImage.height() > 512)
                    {
                        throw Error("PackageInfoDialog::useIconFile",
                                    "Icon file " + img->description() + " is too large (max 512x512)");
                    }
                    setPackageIcon(iconImage);
                    return;
                }
            }
        }
        catch (Error const &er)
        {
            LOG_RES_WARNING("Failed to use package icon image: %s") << er.asText();
        }
        useDefaultIcon();
    }

    bool setup(File const *file)
    {
        if (!file) return false; // Not a package?

        // Look up the package metadata.
        Record const &names = file->objectNamespace();
        if (!names.has(Package::VAR_PACKAGE))
        {
            return false;
        }
        Record const &meta = names.subrecord(Package::VAR_PACKAGE);

        packageId = Package::versionedIdentifierForFile(*file);
        nativePath = file->correspondingNativePath();
        String fileDesc = file->source()->description();

        String format;
        DataBundle const *bundle = file->target().maybeAs<DataBundle>();
        if (bundle)
        {
            format         = bundle->formatAsText().upperFirstChar();
            compatibleGame = bundle->guessCompatibleGame();

            if (!useGameTitlePicture(*bundle))
            {
                useDefaultIcon();
            }

            if (bundle->format() == DataBundle::Collection)
            {
                fileDesc = file->target().description();
            }
        }
        else
        {
            format = tr("Doomsday 2 Package");
            useIconFile(file->path());
        }

        if (file->source()->is<ArchiveEntryFile>())
        {
            // The file itself makes for a better description.
            fileDesc = file->description();
        }

        title->setText(meta.gets(Package::VAR_TITLE));
        path->setText(String(_E(b) "%1" _E(.) "\n%2")
                      .arg(format)
                      .arg(fileDesc.upperFirstChar()));

        // Metadata.
        String metaMsg = String(_E(Ta)_E(l) "Version: " _E(.)_E(Tb) "%1\n"
                                _E(Ta)_E(l) "Tags: "    _E(.)_E(Tb) "%2\n"
                                _E(Ta)_E(l) "License: " _E(.)_E(Tb) "%3")
                    .arg(meta.gets("version"))
                    .arg(meta.gets("tags"))
                    .arg(meta.gets("license"));
        if (meta.has("author"))
        {
            metaMsg += String("\n" _E(Ta)_E(l) "Author: "
                              _E(.)_E(Tb) "%1").arg(meta.gets("author"));

        }
        if (meta.has("contact"))
        {
            metaMsg += String("\n" _E(Ta)_E(l) "Contact: "
                              _E(.)_E(Tb) "%1").arg(meta.gets("contact"));
        }
        metaInfo->setText(metaMsg);

        // Full package description.
        String msg;

        // Start with a generic description of the package format.


        if (compatibleGame.isEmpty())
        {
            QRegularExpression reg(DataBundle::anyGameTagPattern());
            if (!reg.match(meta.gets("tags")).hasMatch())
            {
                msg = "Not enough information to determine which game this package is for.";
            }
        }
        else
        {
            msg = String("This package is likely meant for " _E(b) "%1" _E(.) ".")
                    .arg(compatibleGame);
        }

        if (bundle && bundle->lumpDirectory() &&
            bundle->lumpDirectory()->mapType() != res::LumpDirectory::None)
        {
            int const mapCount = bundle->lumpDirectory()->findMaps().count();
            msg += QString("\n\nContains %1 map%2: ")
                    .arg(mapCount)
                    .arg(DENG2_PLURAL_S(mapCount)) +
                    String::join(bundle->lumpDirectory()->mapsInContiguousRangesAsText(), ", ");
        }

        if (meta.has("notes"))
        {
            msg += "\n\n" + meta.gets("notes") + _E(r);
        }

        if (!bundle)
        {
            if (meta.has("requires"))
            {
                msg += "\n\nRequires:";
                ArrayValue const &reqs = meta.geta("requires");
                for (Value const *val : reqs.elements())
                {
                    msg += "\n - " _E(>) + val->asText() + _E(<);
                }
            }
            if (meta.has("dataFiles") && meta.geta("dataFiles").size() > 0)
            {
                msg += "\n\nData files:";
                ArrayValue const &files = meta.geta("dataFiles");
                for (Value const *val : files.elements())
                {
                    msg += "\n - " _E(>) + val->asText() + _E(<);
                }
            }
        }

        description->setText(msg.trimmed());

        // Show applicable package actions.
        self().buttons()
                << new DialogButtonItem(Action | Id2,
                                        style().images().image("play"),
                                        tr("Play in..."),
                                        new SignalAction(thisPublic, SLOT(playInGame())))
                << new DialogButtonItem(Action | Id3,
                                        style().images().image("create"),
                                        tr("Add to..."),
                                        new SignalAction(thisPublic, SLOT(addToProfile())));

        if (!nativePath.isEmpty())
        {
            self().buttons()
                    << new DialogButtonItem(Action,
                                            tr("Show File"),
                                            new SignalAction(thisPublic, SLOT(showFile())));
        }
        if (Package::hasOptionalContent(*file))
        {
            self().buttons()
                    << new DialogButtonItem(Action | Id1,
                                            style().images().image("gear"),
                                            tr("Options"),
                                            new SignalAction(thisPublic, SLOT(configure())));
        }
        return true;
    }

    static String visibleFamily(String const &family)
    {
        if (family.isEmpty()) return tr("Other");
        return family.upperFirstChar();
    }

    /**
     * Opens a popup menu listing the available game profiles. The compatible game
     * is highlighted, as well as profiles that already have this package added.
     *
     * @param anchor        Popup menu anchor.
     * @param playableOnly  Only show profiles that can be started at the moment.
     */
    void openProfileMenu(RuleRectangle const &anchor, bool playableOnly)
    {
        if (profileMenu) return;

        profileMenu.reset(new PopupMenuWidget);
        profileMenu->setDeleteAfterDismissed(true);
        profileMenu->setAnchorAndOpeningDirection(anchor, ui::Left);

        QList<GameProfile *> profs = DoomsdayApp::gameProfiles().profilesSortedByFamily();

        auto &items = profileMenu->items();
        String lastFamily;
        for (GameProfile *prof : profs)
        {
            if (playableOnly && !prof->isPlayable()) continue;

            if (lastFamily != prof->game().family())
            {
                if (!items.isEmpty())
                {
                    items << new ui::Item(ui::Item::Separator);
                }
                items << new ui::Item(ui::Item::ShownAsLabel | ui::Item::Separator,
                                      visibleFamily(prof->game().family()));
                lastFamily = prof->game().family();
            }

            String label = prof->name();
            String color;
            if (prof->packages().contains(packageId))
            {
                color = _E(C);
                label += " " _E(s)_E(b)_E(D) + tr("ADDED");
            }
            if (!compatibleGame.isEmpty() && prof->gameId() == compatibleGame)
            {
                label = _E(b) + label;
            }
            if (prof->isUserCreated())
            {
                color = _E(F);
            }
            label = color + label;

            items << new ui::ActionItem(label, new CallbackAction([this, prof] ()
            {
                profileSelectedFromMenu(*prof);
            }));
        }

        self().add(profileMenu);
        profileMenu->open();
    }

    void profileSelectedFromMenu(GameProfile &profile)
    {
        switch (menuMode)
        {
        case AddToProfile:
            profile.appendPackage(packageId);
            break;

        case PlayInProfile: {
            auto &prof = DoomsdayApp::app().adhocProfile();
            prof = profile;
            prof.appendPackage(packageId);
            Loop::timer(0.1, [] {
                // Switch the game.
                GLWindow::main().glActivate();
                BusyMode_FreezeGameForBusyMode();
                DoomsdayApp::app().changeGame(DoomsdayApp::app().adhocProfile(),
                                              DD_ActivateGameWorker);
            });
            self().accept();
            break; }
        }
    }
};

PackageInfoDialog::PackageInfoDialog(String const &packageId)
    : DialogWidget("packagepopup")
    , d(new Impl(this))
{
    if (!d->setup(App::packageLoader().select(packageId)))
    {
        //document().setText(packageId);
    }
}

PackageInfoDialog::PackageInfoDialog(File const *packageFile)
    : DialogWidget("packagepopup")
    , d(new Impl(this))
{
    if (!d->setup(packageFile))
    {
        //document().setText(tr("No package"));
    }
}

void PackageInfoDialog::prepare()
{
    DialogWidget::prepare();

    // Monospace text implies that there is an DOS-style readme text
    // file included in the notes.
    bool const useWideLayout = d->description->text().contains(_E(m));

    // Update the width of the dialog. Don't let it get wider than the window.
    d->descriptionWidth->setSource(OperatorRule::minimum(
                                       useWideLayout?
                                           rule("dialog.packageinfo.description.wide")
                                         : rule("dialog.packageinfo.description.normal"),
                                       root().viewWidth() - margins().width() -
                                       d->metaInfo->rule().width()));

    d->minContentHeight->setSource(useWideLayout? rule("dialog.packageinfo.content.minheight")
                                                : ConstantRule::zero());
}

void PackageInfoDialog::playInGame()
{
    d->menuMode = Impl::PlayInProfile;
    d->openProfileMenu(buttonWidget(Id2)->rule(), true);
}

void PackageInfoDialog::addToProfile()
{
    d->menuMode = Impl::AddToProfile;
    d->openProfileMenu(buttonWidget(Id3)->rule(), false);
}

void PackageInfoDialog::configure()
{
    if (d->configurePopup) return; // Let it close itself.

    PopupWidget *pop = PackageContentOptionsWidget::makePopup(
                d->packageId, rule("dialog.packages.left.width"), root().viewHeight());
    d->configurePopup.reset(pop);
    pop->setAnchorAndOpeningDirection(buttonWidget(Id1)->rule(), ui::Left);
    pop->closeButton().setActionFn([pop] ()
    {
        pop->close();
    });

    add(pop);
    pop->open();
}

void PackageInfoDialog::showFile()
{
    if (!d->nativePath.isEmpty())
    {
        QDesktopServices::openUrl(QUrl::fromLocalFile(
                d->nativePath.isDirectory()? d->nativePath : d->nativePath.fileNamePath()));
    }
}
