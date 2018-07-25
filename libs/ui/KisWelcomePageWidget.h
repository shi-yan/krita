/* This file is part of the KDE project
 * Copyright (C) 2018 Scott Petrovic <scottpetrovic@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef KISWELCOMEPAGEWIDGET_H
#define KISWELCOMEPAGEWIDGET_H

#include "kritaui_export.h"
#include "KisViewManager.h"
#include "KisMainWindow.h"

#include <QWidget>
#include "ui_KisWelcomePage.h"
#include <QStandardItemModel>

/// A widget for diplaying if no documents are open. This will display in the MDI area
class KRITAUI_EXPORT KisWelcomePageWidget : public QWidget, public Ui::KisWelcomePage
{
    Q_OBJECT

    public:
    explicit KisWelcomePageWidget(QWidget *parent);
    ~KisWelcomePageWidget() override;

    void setMainWindow(KisMainWindow* mainWindow);

public Q_SLOTS:
    /// if a document is placed over this area, a dotted line will appear as an indicator
    /// that it is a droppable area. KisMainwindow is what triggers this
    void showDropAreaIndicator(bool show);

    void slotUpdateThemeColors();

    /// this could be called multiple times. If a recent document doesn't
    /// have a preview, an icon is used that needs to be updated
    void populateRecentDocuments();


private:
    KisMainWindow* mainWindow;
    QStandardItemModel *recentFilesModel;

private Q_SLOTS:
    void slotNewFileClicked();
    void slotOpenFileClicked();
    void slotClearRecentFiles();

    void recentDocumentClicked(QModelIndex index);

    /// go to URL links
    void slotGoToManual();

    void slotGettingStarted();
    void slotSupportKrita();
    void slotUserCommunity();
    void slotKritaWebsite();
    void slotSourceCode();
    void slotKDESiteLink();

};

#endif // KISWELCOMEPAGEWIDGET_H
