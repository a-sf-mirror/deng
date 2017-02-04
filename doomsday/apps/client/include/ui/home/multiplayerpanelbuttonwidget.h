/** @file multiplayerpanelbuttonwidget.h
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

#ifndef DENG_CLIENT_UI_HOME_MULTIPLAYERPANELBUTTONWIDGET_H
#define DENG_CLIENT_UI_HOME_MULTIPLAYERPANELBUTTONWIDGET_H

#include "../widgets/panelbuttonwidget.h"
#include <de/ButtonWidget>
#include <de/shell/ServerInfo>

class MultiplayerPanelButtonWidget : public PanelButtonWidget
{
    Q_OBJECT

public:
    DENG2_DEFINE_AUDIENCE2(AboutToJoin, void aboutToJoinMultiplayerGame(de::shell::ServerInfo const &))

public:
    MultiplayerPanelButtonWidget();

    de::ButtonWidget &joinButton();

    void setSelected(bool selected) override;
    void itemRightClicked() override;

    void updateContent(de::shell::ServerInfo const &info);

public slots:
    void joinGame();

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_UI_HOME_MULTIPLAYERPANELBUTTONWIDGET_H
