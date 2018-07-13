/*
 *  Copyright (c) 2014 Boudewijn Rempt <boud@valdyas.org>
 *  Copyright (c) 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2.1 of the License, or
 *  (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "kis_heightmap_import.h"

#include <ctype.h>

#include <QApplication>
#include <qendian.h>

#include <kpluginfactory.h>
#include <KoDialog.h>

#include <KisImportExportManager.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoColorSpace.h>
#include <KoColorSpaceTraits.h>

#include <kis_debug.h>
#include <KisDocument.h>
#include <kis_group_layer.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_paint_device.h>
#include <kis_transaction.h>
#include <kis_iterator_ng.h>
#include <kis_random_accessor_ng.h>
#include <kis_config.h>

#include "kis_wdg_options_heightmap.h"
#include "kis_heightmap_utils.h"

K_PLUGIN_FACTORY_WITH_JSON(HeightMapImportFactory, "krita_heightmap_import.json", registerPlugin<KisHeightMapImport>();)

template<typename T>
void fillData(KisPaintDeviceSP pd, int w, int h, QDataStream &stream) {
    KIS_ASSERT_RECOVER_RETURN(pd);

    T pixel;

    for (int i = 0; i < h; ++i) {
        KisHLineIteratorSP it = pd->createHLineIteratorNG(0, i, w);
        do {
            stream >> pixel;
            KoGrayTraits<T>::setGray(it->rawData(), pixel);
            KoGrayTraits<T>::setOpacity(it->rawData(), OPACITY_OPAQUE_F, 1);
        } while(it->nextPixel());
    }
}

KisHeightMapImport::KisHeightMapImport(QObject *parent, const QVariantList &) : KisImportExportFilter(parent)
{
}

KisHeightMapImport::~KisHeightMapImport()
{
}

KisImportExportFilter::ConversionStatus KisHeightMapImport::convert(KisDocument *document, QIODevice *io, KisPropertiesConfigurationSP configuration)
{
    Q_UNUSED(configuration);
    KoID depthId = KisHeightmapUtils::mimeTypeToKoID(mimeType());
    if (depthId.id().isNull()) {
        document->setErrorMessage(i18n("Unknown file type"));
        return KisImportExportFilter::WrongFormat;
    }

    QApplication::restoreOverrideCursor();

    KoDialog* kdb = new KoDialog(0);
    kdb->setWindowTitle(i18n("Heightmap Import Options"));
    kdb->setButtons(KoDialog::Ok | KoDialog::Cancel);

    KisWdgOptionsHeightmap* wdg = new KisWdgOptionsHeightmap(kdb);

    kdb->setMainWidget(wdg);

    connect(wdg, SIGNAL(statusUpdated(bool)), kdb, SLOT(enableButtonOk(bool)));

    KisConfig config(true);

    QString filterConfig = config.importConfiguration(mimeType());
    KisPropertiesConfigurationSP cfg(new KisPropertiesConfiguration);
    cfg->fromXML(filterConfig);

    int w = 0;
    int h = 0;

    int endianness = cfg->getInt("endianness", 1);
    if (endianness == 0) {
        wdg->radioBig->setChecked(true);
    }
    else {
        wdg->radioLittle->setChecked(true);
    }

    KIS_ASSERT(io->isOpen());
    quint64 size = io->size();

    wdg->fileSizeLabel->setText(QString::number(size));

    if(depthId == Integer8BitsColorDepthID) {
        wdg->bppLabel->setText(QString::number(8));
        wdg->typeLabel->setText("Integer");
    }
    else if(depthId == Integer16BitsColorDepthID) {
        wdg->bppLabel->setText(QString::number(16));
        wdg->typeLabel->setText("Integer");
    }
    else if(depthId == Float32BitsColorDepthID) {
        wdg->bppLabel->setText(QString::number(32));
        wdg->typeLabel->setText("Float");
    }
    else {
        return KisImportExportFilter::InternalError;
    }

    if (!batchMode()) {
        if (kdb->exec() == QDialog::Rejected) {
            return KisImportExportFilter::UserCancelled;
        }
    }

    cfg->setProperty("endianness", wdg->radioBig->isChecked() ? 0 : 1);

    config.setImportConfiguration(mimeType(), cfg);

    w = wdg->widthInput->value();
    h = wdg->heightInput->value();

    QDataStream::ByteOrder bo = QDataStream::LittleEndian;
    cfg->setProperty("endianness", 1);
    if (wdg->radioBig->isChecked()) {
        bo = QDataStream::BigEndian;
        cfg->setProperty("endianness", 0);
    }
    KisConfig(true).setExportConfiguration(mimeType(), cfg);

    QDataStream s(io);
    s.setByteOrder(bo);
    // needed for 32bit float data
    s.setFloatingPointPrecision(QDataStream::SinglePrecision);

    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->colorSpace(GrayAColorModelID.id(), depthId.id(), 0);
    KisImageSP image = new KisImage(document->createUndoStore(), w, h, colorSpace, "imported heightmap");
    KisPaintLayerSP layer = new KisPaintLayer(image, image->nextLayerName(), 255);

    if (depthId == Float32BitsColorDepthID) {
        fillData<float>(layer->paintDevice(), w, h, s);
    }
    else if (depthId == Integer16BitsColorDepthID) {
        fillData<quint16>(layer->paintDevice(), w, h, s);
    }
    else if (depthId == Integer8BitsColorDepthID) {
        fillData<quint8>(layer->paintDevice(), w, h, s);
    }
    else {
        return KisImportExportFilter::InternalError;
    }

    image->addNode(layer.data(), image->rootLayer().data());
    document->setCurrentImage(image);
    return KisImportExportFilter::OK;
}

#include "kis_heightmap_import.moc"
