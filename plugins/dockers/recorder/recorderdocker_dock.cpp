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

#include "recorderdocker_dock.h"
#include "recorderwidget.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QStatusBar>
#include <klocalizedstring.h>

#include "kis_canvas2.h"
#include <KisViewManager.h>
#include <kis_zoom_manager.h>
#include "kis_image.h"
#include "kis_paint_device.h"
#include "kis_signal_compressor.h"
#include <QDir>
#include <QStringBuilder>

RecorderDockerDock::RecorderDockerDock( )
    : QDockWidget(i18n("Recorder"))
    , m_zoomSlider(0)
    , m_canvas(0)
{
    QWidget *page = new QWidget(this);
    m_layout = new QVBoxLayout(page);

    m_recorderWidget = new RecorderWidget(this);
    m_recorderWidget->setMinimumHeight(50);
    m_recorderWidget->setBackgroundRole(QPalette::AlternateBase);
    m_recorderWidget->setAutoFillBackground(true); // paints background role before paint()

    m_layout->addWidget(m_recorderWidget, 1);
    m_recordFileLocationLineEdit = new QLineEdit(this);
    m_recordFileLocationLineEdit->setText(QDir::homePath() % "/snapshot/image");
    m_recordLayout = new QHBoxLayout(this);
    m_recordLayout->addWidget(m_recordFileLocationLineEdit);
    m_recordToggleButton = new QPushButton(this);
    m_recordToggleButton->setCheckable(true);
    m_recordToggleButton->setText("O");
    m_recordLayout->addWidget(m_recordToggleButton);
    m_layout->addLayout(m_recordLayout);

    connect(m_recordToggleButton, SIGNAL(toggled(bool)), this, SLOT(onRecordButtonToggled(bool)));
    setWidget(page);
}

void RecorderDockerDock::setCanvas(KoCanvasBase * canvas)
{
    if(m_canvas == canvas)
        return;

    setEnabled(canvas != 0);

    if (m_canvas) {
        m_canvas->disconnectCanvasObserver(this);
        m_canvas->image()->disconnect(this);
    }

    if (m_zoomSlider) {
        m_layout->removeWidget(m_zoomSlider);
        delete m_zoomSlider;
        m_zoomSlider = 0;
    }

    m_canvas = dynamic_cast<KisCanvas2*>(canvas);

    m_recorderWidget->setCanvas(canvas);
    if (m_canvas && m_canvas->viewManager() && m_canvas->viewManager()->zoomController() && m_canvas->viewManager()->zoomController()->zoomAction()) {
        m_zoomSlider = m_canvas->viewManager()->zoomController()->zoomAction()->createWidget(m_canvas->imageView()->KisView::statusBar());
        m_layout->addWidget(m_zoomSlider);
    }
}

void RecorderDockerDock::unsetCanvas()
{
    setEnabled(false);
    m_canvas = 0;
    m_recorderWidget->unsetCanvas();
}

void RecorderDockerDock::onRecordButtonToggled(bool enabled)
{
    bool enabled2 = enabled;
    m_recorderWidget->enableRecord(enabled2, m_recordFileLocationLineEdit->text());

    if (enabled && !enabled2)
    {
        disconnect(m_recordToggleButton, SIGNAL(toggle(bool)), this, SLOT(onRecordButtonToggled(bool)));
        m_recordToggleButton->setChecked(false);
        connect(m_recordToggleButton, SIGNAL(toggle(bool)), this, SLOT(onRecordButtonToggled(bool)));
    }
}


