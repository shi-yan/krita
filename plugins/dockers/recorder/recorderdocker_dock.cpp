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
#include <QFileDialog>
#include <kis_icon_utils.h>
#include <klocalizedstring.h>
#include <kactioncollection.h>
#include <QRegExp>
#include <QRegExpValidator>

RecorderDockerDock::RecorderDockerDock( )
    : QDockWidget(i18n("Recorder"))
    , m_canvas(0)
    , m_imageIdleWatcher(1500)
    , m_recordEnabled(false)
    , m_recordCounter(0)
{
    QWidget *page = new QWidget(this);
    m_layout = new QGridLayout(page);
    m_recordDirectoryLabel = new QLabel(this);
    m_recordDirectoryLabel->setText("Directory:");

    m_layout->addWidget(m_recordDirectoryLabel, 0, 0,1,2);

    m_recordDirectoryLineEdit = new QLineEdit(this);
    m_recordDirectoryLineEdit->setText(QDir::homePath());
    m_recordDirectoryLineEdit->setReadOnly(true);
    m_layout->addWidget(m_recordDirectoryLineEdit, 1, 0);

    m_recordDirectoryPushButton = new QPushButton(this);
    m_recordDirectoryPushButton->setIcon(KisIconUtils::loadIcon("folder"));
    m_recordDirectoryPushButton->setToolTip(i18n("Record Image"));

    m_layout->addWidget(m_recordDirectoryPushButton, 1, 1);

    m_imageNameLabel = new QLabel(this);
    m_imageNameLabel->setText("Image Name:");

    m_layout->addWidget(m_imageNameLabel, 2,0,1,2);

    m_imageNameLineEdit = new QLineEdit(this);
    m_imageNameLineEdit->setText("image");

    QRegExp rx("[0-9a-zA-z_]+");
    QValidator *validator = new QRegExpValidator(rx, this);
    m_imageNameLineEdit->setValidator(validator);

    m_layout->addWidget(m_imageNameLineEdit, 3, 0);

    m_recordToggleButton = new QPushButton(this);
    m_recordToggleButton->setCheckable(true);
    m_recordToggleButton->setIcon(KisIconUtils::loadIcon("media-record"));
    m_recordToggleButton->setToolTip(i18n("Record Image"));
    m_layout->addWidget(m_recordToggleButton, 3, 1);

    m_logLabel = new QLabel(this);
    m_logLabel->setText("Recent Save:");
    m_layout->addWidget(m_logLabel, 4,0,1,2);
    m_logLineEdit = new QLineEdit(this);
    m_logLineEdit->setReadOnly(true);
    m_layout->addWidget(m_logLineEdit, 5,0,1,2);

    m_spacer = new QSpacerItem(1, 1, QSizePolicy::Minimum,QSizePolicy::Expanding);
    m_layout->addItem(m_spacer, 6, 0,1,2);
    connect(m_recordDirectoryPushButton, SIGNAL(clicked()), this, SLOT(onSelectRecordFolderButtonClicked()));
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

        //generateThumbnail();
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
    enableRecord(enabled2, m_recordDirectoryLineEdit->text() % "/" % m_imageNameLineEdit->text());

    if (enabled && !enabled2)
    {
        disconnect(m_recordToggleButton, SIGNAL(toggle(bool)), this, SLOT(onRecordButtonToggled(bool)));
        m_recordToggleButton->setChecked(false);
        connect(m_recordToggleButton, SIGNAL(toggle(bool)), this, SLOT(onRecordButtonToggled(bool)));
    }
}

void RecorderDockerDock::onSelectRecordFolderButtonClicked()
{
    QFileDialog dialog(this);
    dialog.setFileMode(QFileDialog::DirectoryOnly);
    QString folder = dialog.getExistingDirectory(this, tr("Select Output Folder"), m_recordDirectoryLineEdit->text(), QFileDialog::ShowDirsOnly);
    m_recordDirectoryLineEdit->setText(folder);
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
        //m_imageIdleWatcher.stopCont
        //QMutexLocker locker(&m_eventMutex);
        if (m_canvas && m_recordingCanvas == m_canvas)
        {
            disconnect(&m_imageIdleWatcher, &KisIdleWatcher::startedIdleMode, this, &RecorderDockerDock::generateThumbnail);
            KisImageSP image = m_canvas->image();

            KisPaintDeviceSP dev = image->projection();

            //QtConcurrent::run([&]() 
            {
                //QMutexLocker locker(&m_saveMutex);
                QImage recorderImage = dev->convertToQImage(KoColorSpaceRegistry::instance()->rgb8()->profile());
                QString filename = QString(m_recordPath % "_%1.png").arg(++m_recordCounter, 7, 10, QChar('0'));
                qDebug() << "save image" << filename;
                recorderImage.save(filename);
                m_logLineEdit->setText(filename % " saved!");
                // Code in this block will run in another thread
            }//);
            connect(&m_imageIdleWatcher, &KisIdleWatcher::startedIdleMode, this, &RecorderDockerDock::generateThumbnail);
        }
    }
}
