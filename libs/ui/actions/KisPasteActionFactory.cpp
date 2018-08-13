/*
 *  Copyright (c) 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "KisPasteActionFactory.h"

#include "kis_image.h"
#include "KisViewManager.h"
#include "kis_tool_proxy.h"
#include "kis_canvas2.h"
#include "kis_canvas_controller.h"
#include "kis_paint_device.h"
#include "kis_paint_layer.h"
#include "kis_shape_layer.h"
#include "kis_import_catcher.h"
#include "kis_clipboard.h"
#include "commands/kis_image_layer_add_command.h"
#include "kis_processing_applicator.h"

#include <KoSvgPaste.h>
#include <KoShapeController.h>
#include <KoShapeManager.h>
#include <KoSelection.h>
#include <KoSelectedShapesProxy.h>
#include "kis_algebra_2d.h"
#include <KoShapeMoveCommand.h>
#include <KoShapeReorderCommand.h>
#include "kis_time_range.h"
#include "kis_keyframe_channel.h"
#include "kis_raster_keyframe_channel.h"
#include "kis_painter.h"

namespace {
QPointF getFittingOffset(QList<KoShape*> shapes,
                         const QPointF &shapesOffset,
                         const QRectF &documentRect,
                         const qreal fitRatio)
{
    QPointF accumulatedFitOffset;

    Q_FOREACH (KoShape *shape, shapes) {
        const QRectF bounds = shape->boundingRect();

        const QPointF center = bounds.center() + shapesOffset;

        const qreal wMargin = (0.5 - fitRatio) * bounds.width();
        const qreal hMargin = (0.5 - fitRatio) * bounds.height();
        const QRectF allowedRect = documentRect.adjusted(-wMargin, -hMargin, wMargin, hMargin);

        const QPointF fittedCenter = KisAlgebra2D::clampPoint(center, allowedRect);

        accumulatedFitOffset += fittedCenter - center;
    }

    return accumulatedFitOffset;
}

bool tryPasteShapes(bool pasteAtCursorPosition, KisViewManager *view)
{
    bool result = false;

    KoSvgPaste paste;

    if (paste.hasShapes()) {
        KoCanvasBase *canvas = view->canvasBase();

        QSizeF fragmentSize;
        QList<KoShape*> shapes =
            paste.fetchShapes(canvas->shapeController()->documentRectInPixels(),
                              canvas->shapeController()->pixelsPerInch(), &fragmentSize);

        if (!shapes.isEmpty()) {
            KoShapeManager *shapeManager = canvas->shapeManager();
            shapeManager->selection()->deselectAll();

            // adjust z-index of the shapes so that they would be
            // pasted on the top of the stack
            QList<KoShape*> topLevelShapes = shapeManager->topLevelShapes();
            auto it = std::max_element(topLevelShapes.constBegin(), topLevelShapes.constEnd(), KoShape::compareShapeZIndex);
            if (it != topLevelShapes.constEnd()) {
                const int zIndexOffset = (*it)->zIndex();

                std::stable_sort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);

                QList<KoShapeReorderCommand::IndexedShape> indexedShapes;
                std::transform(shapes.constBegin(), shapes.constEnd(),
                               std::back_inserter(indexedShapes),
                    [zIndexOffset] (KoShape *shape) {
                        KoShapeReorderCommand::IndexedShape indexedShape(shape);
                        indexedShape.zIndex += zIndexOffset;
                        return indexedShape;
                    });

                indexedShapes = KoShapeReorderCommand::homogenizeZIndexesLazy(indexedShapes);

                KoShapeReorderCommand cmd(indexedShapes);
                cmd.redo();
            }

            KUndo2Command *parentCommand = new KUndo2Command(kundo2_i18n("Paste shapes"));
            canvas->shapeController()->addShapesDirect(shapes, 0, parentCommand);

            QPointF finalShapesOffset;


            if (pasteAtCursorPosition) {
                QRectF boundingRect = KoShape::boundingRect(shapes);
                const QPointF cursorPos = canvas->canvasController()->currentCursorPosition();
                finalShapesOffset = cursorPos - boundingRect.center();

            } else {
                bool foundOverlapping = false;

                QRectF boundingRect = KoShape::boundingRect(shapes);
                const QPointF offsetStep = 0.1 * QPointF(boundingRect.width(), boundingRect.height());

                QPointF offset;

                Q_FOREACH (KoShape *shape, shapes) {
                    QRectF br1 = shape->boundingRect();

                    bool hasOverlappingShape = false;

                    do {
                        hasOverlappingShape = false;

                        // we cannot use shapesAt() here, because the groups are not
                        // handled in the shape manager's tree
                        QList<KoShape*> conflicts = shapeManager->shapes();

                        Q_FOREACH (KoShape *intersectedShape, conflicts) {
                            if (intersectedShape == shape) continue;

                            QRectF br2 = intersectedShape->boundingRect();

                            const qreal tolerance = 2.0; /* pt */
                            if (KisAlgebra2D::fuzzyCompareRects(br1, br2, tolerance)) {
                                br1.translate(offsetStep.x(), offsetStep.y());
                                offset += offsetStep;

                                hasOverlappingShape = true;
                                foundOverlapping = true;
                                break;
                            }
                        }
                    } while (hasOverlappingShape);

                    if (foundOverlapping) break;
                }

                if (foundOverlapping) {
                    finalShapesOffset = offset;
                }
            }

            const QRectF documentRect = canvas->shapeController()->documentRect();
            finalShapesOffset += getFittingOffset(shapes, finalShapesOffset, documentRect, 0.1);

            if (!finalShapesOffset.isNull()) {
                new KoShapeMoveCommand(shapes, finalShapesOffset, parentCommand);
            }

            canvas->addCommand(parentCommand);

            Q_FOREACH (KoShape *shape, shapes) {
                canvas->selectedShapesProxy()->selection()->select(shape);
            }

            result = true;
        }
    }

    return result;
}

}

void KisPasteActionFactory::run(bool pasteAtCursorPosition, KisViewManager *view)
{
    KisImageSP image = view->image();
    if (!image) return;

    if (tryPasteShapes(pasteAtCursorPosition, view)) {
        return;
    }

    KisTimeRange range;
    const QRect fittingBounds = pasteAtCursorPosition ? QRect() : image->bounds();
    KisPaintDeviceSP clip = KisClipboard::instance()->clip(fittingBounds, true, &range);

    if (clip) {
        if (pasteAtCursorPosition) {
            const QPointF docPos = view->canvasBase()->canvasController()->currentCursorPosition();
            const QPointF imagePos = view->canvasBase()->coordinatesConverter()->documentToImage(docPos);

            const QPointF offset = (imagePos - QRectF(clip->exactBounds()).center()).toPoint();

            clip->setX(clip->x() + offset.x());
            clip->setY(clip->y() + offset.y());
        }

        KisImportCatcher::adaptClipToImageColorSpace(clip, image);
        KisPaintLayerSP newLayer = new KisPaintLayer(image.data(),
                                                     image->nextLayerName() + i18n("(pasted)"),
                                                     OPACITY_OPAQUE_U8);
        KisNodeSP aboveNode = view->activeLayer();
        KisNodeSP parentNode = aboveNode ? aboveNode->parent() : image->root();

        if (range.isValid()) {
            newLayer->enableAnimation();
            KisKeyframeChannel *channel = newLayer->getKeyframeChannel(KisKeyframeChannel::Content.id(), true);
            KisRasterKeyframeChannel *rasterChannel = dynamic_cast<KisRasterKeyframeChannel*>(channel);
            rasterChannel->importFrame(range.start(), clip, 0);

            if (!range.isInfinite()) {
                rasterChannel->addKeyframe(range.end() + 1, 0);
            }
        } else {
            const QRect rc = clip->extent();
            KisPainter::copyAreaOptimized(rc.topLeft(), clip, newLayer->paintDevice(), rc);
        }

        KUndo2Command *cmd = new KisImageLayerAddCommand(image, newLayer, parentNode, aboveNode);
        KisProcessingApplicator *ap = beginAction(view, cmd->text());
        ap->applyCommand(cmd, KisStrokeJobData::SEQUENTIAL, KisStrokeJobData::NORMAL);
        endAction(ap, KisOperationConfiguration(id()).toXML());
    } else {
        // XXX: "Add saving of XML data for Paste of shapes"
        view->canvasBase()->toolProxy()->paste();
    }
}
