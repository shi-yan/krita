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
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QtConcurrent>
#include <KoColorSpaceRegistry.h>

RecorderDockerDock::RecorderDockerDock( )
    : QDockWidget(i18n("Recorder"))
    , m_canvas(0)
    , m_imageIdleWatcher(250)
{
    QWidget *page = new QWidget(this);
    m_layout = new QVBoxLayout(page);

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

    m_canvas = dynamic_cast<KisCanvas2*>(canvas);

    if (m_canvas) {
        m_imageIdleWatcher.setTrackedImage(m_canvas->image());

        connect(&m_imageIdleWatcher, &KisIdleWatcher::startedIdleMode, this, &RecorderDockerDock::generateThumbnail);

        connect(m_canvas->image(), SIGNAL(sigImageUpdated(QRect)),SLOT(startUpdateCanvasProjection()));
        connect(m_canvas->image(), SIGNAL(sigSizeChanged(QPointF, QPointF)),SLOT(startUpdateCanvasProjection()));

        generateThumbnail();
    }
}

void RecorderDockerDock::startUpdateCanvasProjection()
{
    m_imageIdleWatcher.startCountdown();
}

void RecorderDockerDock::unsetCanvas()
{
    setEnabled(false);
    m_canvas = 0;
}

void RecorderDockerDock::onRecordButtonToggled(bool enabled)
{
    bool enabled2 = enabled;
    enableRecord(enabled2, m_recordFileLocationLineEdit->text());

    if (enabled && !enabled2)
    {
        disconnect(m_recordToggleButton, SIGNAL(toggle(bool)), this, SLOT(onRecordButtonToggled(bool)));
        m_recordToggleButton->setChecked(false);
        connect(m_recordToggleButton, SIGNAL(toggle(bool)), this, SLOT(onRecordButtonToggled(bool)));
    }
}

void RecorderDockerDock::enableRecord(bool &enabled, const QString &path)
{
    m_recordEnabled = enabled;
    if (m_recordEnabled)
    {
        m_recordPath = path;

        QUrl fileUrl(m_recordPath);

        QString filename = fileUrl.fileName();
        QString dirPath = fileUrl.adjusted(QUrl::RemoveFilename).path();

        QDir dir(dirPath);

        if (!dir.exists())
        {
            if (!dir.mkpath(dirPath))
            {
                enabled = m_recordEnabled = false;
                return;
            }
        }

        QFileInfoList images = dir.entryInfoList({filename % "_*.png"});

        QRegularExpression namePattern("^"%filename%"_([0-9]{7}).png$");

        foreach(auto info, images)
        {
            QRegularExpressionMatch match = namePattern.match(info.fileName());
            if (match.hasMatch())
            {
                //qDebug() << "match" << info.fileName() << match.captured(1);
                QString count = match.captured(1);
                int numCount = count.toInt();

                if (m_recordCounter < numCount)
                {
                    m_recordCounter = numCount;
                }
                //qDebug() << QString("%1").arg((qulonglong)numCount, 7, 10, QChar('0'));
            }
        }

        if (m_canvas)
        {
            m_recordingCanvas = m_canvas;
            startUpdateCanvasProjection();
        }
        else
        {
            enabled = m_recordEnabled = false;
            return;
        }
    }
}

void RecorderDockerDock::generateThumbnail()
{
    if (m_recordEnabled)
    {
        //QMutexLocker locker(&mutex);
        if (m_canvas && m_recordingCanvas == m_canvas)
        {
            KisImageSP image = m_canvas->image();

            KisPaintDeviceSP dev = image->projection();

            QtConcurrent::run([=]() 
            {
                QImage recorderImage = dev->convertToQImage(KoColorSpaceRegistry::instance()->rgb8()->profile());
                QString filename = QString(m_recordPath % "_%1.png").arg(++m_recordCounter, 7, 10, QChar('0'));
                qDebug() << "save image" << filename;
                recorderImage.save(filename);
                // Code in this block will run in another thread
            });
        }
    }
}
