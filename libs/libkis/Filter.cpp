/*
 *  Copyright (c) 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */
#include "Filter.h"

#include <kis_filter.h>
#include <kis_filter.h>
#include <kis_properties_configuration.h>
#include <kis_filter_configuration.h>
#include <kis_filter_registry.h>
#include <InfoObject.h>
#include <Node.h>

struct Filter::Private {
    Private() {}
    QString name;
    InfoObject *configuration {0};
};

Filter::Filter()
    : QObject(0)
    , d(new Private)
{
}

Filter::~Filter() 
{
    qDebug() << "Deleting filter" << d->name;
    delete d->configuration;
    delete d;
}


QString Filter::name() const
{
    return d->name;
}

void Filter::setName(const QString &name)
{
    d->name = name;
    delete d->configuration;

    KisFilterSP filter = KisFilterRegistry::instance()->value(d->name);
    d->configuration = new InfoObject(filter->defaultConfiguration());
}

InfoObject* Filter::configuration() const
{
    return d->configuration;
}

void Filter::setConfiguration(InfoObject* value)
{
    d->configuration = value;
}

bool Filter::apply(Node *node, int x, int y, int w, int h)
{
    if (node->locked()) return false;

    KisFilterSP filter = KisFilterRegistry::instance()->value(d->name);
    if (!filter) return false;

    KisPaintDeviceSP dev = node->paintDevice();
    if (!dev) return false;


    QRect applyRect = QRect(x, y, w, h);

    KisImageSP image = node->image();
    if (image) {
        image->lock();
    }

    KisFilterConfigurationSP config = static_cast<KisFilterConfiguration*>(d->configuration->configuration().data());

    filter->process(dev, applyRect, config);

    if (image) {
        image->unlock();
        image->initialRefreshGraph();
    }

    qDebug() << "filter applied!" << filter->changedRect(QRect(x, y, w, h), config, 0);

    return true;

}


