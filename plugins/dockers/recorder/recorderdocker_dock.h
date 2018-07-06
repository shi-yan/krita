/*
 *  Copyright (c) 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef _RECORDER_DOCK_H_
#define _RECORDER_DOCK_H_

#include <QPointer>
#include <QDockWidget>
#include <KoCanvasObserverBase.h>
#include <QPushButton>
#include <QLineEdit>
#include <QHBoxLayout>

#include <kis_canvas2.h>
#include "kis_idle_watcher.h"

class QVBoxLayout;
class RecorderWidget;

class RecorderDockerDock : public QDockWidget, public KoCanvasObserverBase {
    Q_OBJECT
public:
    RecorderDockerDock();
    QString observerName() override { return "RecorderDockerDock"; }
    void setCanvas(KoCanvasBase *canvas) override;
    void unsetCanvas() override;

private:
    QVBoxLayout *m_layout;
    QHBoxLayout *m_recordLayout;
    QPointer<KisCanvas2> m_recordingCanvas;
    QString m_recordPath;

    QPointer<KisCanvas2> m_canvas;
    QLineEdit *m_recordFileLocationLineEdit;
    QPushButton *m_recordToggleButton;
    KisIdleWatcher m_imageIdleWatcher;
    bool m_recordEnabled;
    int m_recordCounter;
    void enableRecord(bool &enabled, const QString &path);

private Q_SLOTS:
	void onRecordButtonToggled(bool enabled);
    void startUpdateCanvasProjection();
    void generateThumbnail();
};


#endif
