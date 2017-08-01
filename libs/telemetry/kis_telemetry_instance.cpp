/* This file is part of the KDE project
   Copyright (C) 2017 Alexey Kapustin <akapust1n@mail.ru>


   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
*/
#include "kis_telemetry_instance.h"
#include <kis_assert.h>

Q_GLOBAL_STATIC(KisTelemetryInstance, s_instance)

KisTelemetryInstance* KisTelemetryInstance::instance()
{
    return s_instance;
}

void KisTelemetryInstance::setProvider(KisTelemetryAbstract* provider)
{
    if (!telemetryProvider.isNull()) {
        KIS_SAFE_ASSERT_RECOVER_RETURN(false);
    }
    telemetryProvider.reset(provider);
}

void KisTelemetryInstance::notifyToolAcion(KisTelemetryInstance::Actions action, QString id)
{
    if (telemetryProvider.isNull()) {
        return;
    }
    switch (action) {
    case ToolActivate: {
        id = getToolId(id, UseMode::Activate);
        telemetryProvider->putTimeTicket(id);
        break;
    }
    case ToolDeactivate: {
        id = getToolId(id, UseMode::Activate);
        telemetryProvider->getTimeTicket(id);
        break;
    }
    case ToolsStartUse: {
        id = getToolId(id, UseMode::Use);
        telemetryProvider->putTimeTicket(id);
        break;
    }
    case ToolsStopUse: {
        id = getToolId(id, UseMode::Use);
        telemetryProvider->getTimeTicket(id);
        break;
    }
    default:
        break;
    }
}

void KisTelemetryInstance::notifySaveImageProperties(KisImagePropertiesTicket::ImageInfo imageInfo, QString id)
{
    if (telemetryProvider.isNull()) {
        return;
    }
    telemetryProvider->saveImageProperites(id, imageInfo);
}

void KisTelemetryInstance::notifySaveActionInfo(KisActionInfoTicket::ActionInfo actionInfo, QString id)
{
    if (telemetryProvider.isNull()) {
        return;
    }
    telemetryProvider->saveActionInfo(id, actionInfo);
}

void KisTelemetryInstance::sendData(QString path, QString adress)
{
    if (telemetryProvider.isNull()) {
        return;
    }
    telemetryProvider->sendData(path, adress);
}

QString KisTelemetryInstance::getToolId(QString id, KisTelemetryInstance::UseMode mode)
{
    QString toolId = "Tool" + getUseMode(mode);
    toolId += id;
    return toolId;
}

QString KisTelemetryInstance::getUseMode(KisTelemetryInstance::UseMode mode)
{
    switch (mode) {
    case Activate:
        return "/Activate/";
    case Use:
        return "/Use/";
    default:
        return "/Activate/";
    }
}