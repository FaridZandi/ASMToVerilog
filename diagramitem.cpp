/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Digia Plc and its Subsidiary(-ies) nor the names
**     of its contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "diagramitem.h"
#include "arrow.h"
#include "diagramtextitem.h"

#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QPainter>
#include <iostream>
#include <QTextCursor>

DiagramItem::DiagramItem(DiagramType diagramType, QMenu *contextMenu,
             QGraphicsItem *parent)
    : QGraphicsPolygonItem(parent)
{
    scaleRate = 1;
    myDiagramType = diagramType;
    myContextMenu = contextMenu;
    textItem = NULL;
    QPainterPath path;
    switch (myDiagramType) {
        case StartEnd:
            path.moveTo(200, 50);
            path.arcTo(150, 0, 50, 50, 0, 90);
            path.arcTo(50, 0, 50, 50, 90, 90);
            path.arcTo(50, 50, 50, 50, 180, 90);
            path.arcTo(150, 50, 50, 50, 270, 90);
            path.lineTo(200, 25);
            myPolygon = path.toFillPolygon();
            break;
        case Conditional:
            myPolygon << QPointF(-100, 0) << QPointF(0, 100)
                      << QPointF(100, 0) << QPointF(0, -100)
                      << QPointF(-100, 0);
            break;
        case Step:
            myPolygon << QPointF(-100, -50) << QPointF(100, -50)
                      << QPointF(100, 50) << QPointF(-100, 50)
                      << QPointF(-100, -50);
            break;
        case Point:
            path.addEllipse(-5,-5,10,10);
            myPolygon = path.toFillPolygon();
            break;
        default:
            path.moveTo(75, 25);
            path.arcTo(25, -25, 50, 50, 0, 90);
            path.arcTo(-75, -25, 50, 50, 90, 90);
            path.arcTo(-75, 25, 50, 50, 180, 90);
            path.arcTo(25, 25, 50, 50, 270, 90);
            path.lineTo(75, 0);
            myPolygon = path.toFillPolygon();
            break;
            break;
    }
    setPolygon(myPolygon);
    setFlag(QGraphicsItem::ItemIsMovable, true);
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
}

DiagramItem::~DiagramItem()
{
    if(textItem){
        scene()->removeItem(textItem);
        delete textItem;
    }
}

void DiagramItem::removeArrow(Arrow *arrow)
{
    int index = arrows.indexOf(arrow);

    if (index != -1)
        arrows.removeAt(index);
}

void DiagramItem::removeArrows()
{
    foreach (Arrow *arrow, arrows) {
        arrow->startItem()->removeArrow(arrow);
        arrow->endItem()->removeArrow(arrow);
        scene()->removeItem(arrow);
        delete arrow;
    }
}

void DiagramItem::addArrow(Arrow *arrow)
{
    arrows.append(arrow);
}

QPixmap DiagramItem::image() const
{
    QPixmap pixmap(250, 250);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setPen(QPen(Qt::black, 8));
    painter.translate(125, 125);
    painter.drawPolyline(myPolygon);

    return pixmap;
}

void DiagramItem::enlarge(){
    scaleRate *= 1.25;
    QPolygonF qpf = polygon();
    QTransform trans;
    trans=trans.scale(scaleRate,scaleRate);
    QPolygonF qpf2 = trans.map(qpf);
    setPolygon(qpf2);
    setTextItemPosition();
}

void DiagramItem::shrink(){
    scaleRate /= 1.25;
    QPolygonF qpf = polygon();
    QTransform trans;
    trans=trans.scale(scaleRate,scaleRate);
    QPolygonF qpf2 = trans.map(qpf);
    setPolygon(qpf2);
    setTextItemPosition();
}

void DiagramItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event)
{
    scene()->clearSelection();
    setSelected(true);
    myContextMenu->exec(event->screenPos());
}

QVariant DiagramItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemPositionChange) {
        foreach (Arrow *arrow, arrows) {
            arrow->updatePosition();
        }
    }

    return value;
}

void DiagramItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if(textItem){
        setTextItemPosition();
    }

    for(auto a : arrows){
        a->setTextItemPosition();
    }

    QGraphicsItem::mouseMoveEvent(event);
}

QList<Arrow *> DiagramItem::getArrows() const
{
    return arrows;
}

qreal DiagramItem::getScaleRate() const
{
    return scaleRate;
}

void DiagramItem::setTextItemPosition()
{
    if(not textItem) return;


    switch(myDiagramType){
        case Io:{
            auto this_pos = this->pos();
            this_pos.setX(this_pos.x() - this->boundingRect().size().width() / 2 + 10);
            this_pos.setY(this_pos.y() - this->boundingRect().size().height() / 2 + 30);
            textItem->setPos(this_pos);
            break;
        }
        case Step:{
            auto this_pos = this->pos();
            this_pos.setX(this_pos.x() - this->boundingRect().size().width() / 2 + 10);
            this_pos.setY(this_pos.y() - this->boundingRect().size().height() / 2 + 10);
            textItem->setPos(this_pos);
            break;
        }
        case Conditional:{
            auto this_pos = this->pos();
            this_pos.setX(this_pos.x() - this->boundingRect().size().width() / 4);
            this_pos.setY(this_pos.y() - this->boundingRect().size().height() / 4);
            textItem->setPos(this_pos);
            break;
        }
        default:
            break;
        }
}

void DiagramItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent *event)
{
    if(diagramType() == Point) return;

    if(textItem == NULL)
    {
        textItem = new DiagramTextItem("Text");
        textItem->setTextInteractionFlags(Qt::TextEditorInteraction);
        textItem->setZValue(1000.0);
        textItem->setFlag(QGraphicsItem::ItemIsMovable, false);
        textItem->setFlag(QGraphicsItem::ItemIsSelectable, false);

        setTextItemPosition();

        scene()->addItem(textItem);
        textItem->mouseDoubleClickEvent(event);
        textItem->setFocus(Qt::MouseFocusReason);
        textItem->setSelected(true);
    }
    else{
        textItem->mouseDoubleClickEvent(event);
        textItem->setFocus(Qt::MouseFocusReason);
        textItem->setSelected(true);
        QTextCursor c = textItem->textCursor();
        c.clearSelection();
        textItem->setTextCursor(c);
    }

}
