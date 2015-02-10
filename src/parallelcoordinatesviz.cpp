//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2014, Lawrence Livermore National Security, LLC. Produced
// at the Lawrence Livermore National Laboratory. Written by Alfredo
// Gimenez (alfredo.gimenez@gmail.com). LLNL-CODE-663358. All rights
// reserved.
//
// This file is part of MemAxes. For details, see
// https://github.com/scalability-tools/MemAxes
//
// Please also read this link – Our Notice and GNU Lesser General Public
// License. This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License (as
// published by the Free Software Foundation) version 2.1 dated February
// 1999.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the IMPLIED WARRANTY OF
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the terms and
// conditions of the GNU General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
//
// OUR NOTICE AND TERMS AND CONDITIONS OF THE GNU GENERAL PUBLIC LICENSE
// Our Preamble Notice
// A. This notice is required to be provided under our contract with the
// U.S. Department of Energy (DOE). This work was produced at the Lawrence
// Livermore National Laboratory under Contract No. DE-AC52-07NA27344 with
// the DOE.
// B. Neither the United States Government nor Lawrence Livermore National
// Security, LLC nor any of their employees, makes any warranty, express or
// implied, or assumes any liability or responsibility for the accuracy,
// completeness, or usefulness of any information, apparatus, product, or
// process disclosed, or represents that its use would not infringe
// privately-owned rights.
//////////////////////////////////////////////////////////////////////////////
#include "parallelcoordinatesviz.h"

#include <QPaintEvent>
#include <QMenu>

#include <iostream>
#include <algorithm>

#include "util.h"

ParallelCoordinatesVizWidget::ParallelCoordinatesVizWidget(QWidget *parent)
    : VizWidget(parent)
{
    colorMap.push_back(QColor(166,206,227));
    colorMap.push_back(QColor(31,120,180));
    colorMap.push_back(QColor(178,223,138));
    colorMap.push_back(QColor(51,160,44));
    colorMap.push_back(QColor(251,154,153));
    colorMap.push_back(QColor(227,26,28));
    colorMap.push_back(QColor(253,191,111));
    colorMap.push_back(QColor(255,127,0));
    colorMap.push_back(QColor(202,178,214));
    colorMap.push_back(QColor(106,61,154));
    colorMap.push_back(QColor(255,255,153));
    colorMap.push_back(QColor(177,89,40 ));

    selOpacity = 0.4;
    unselOpacity = 0.1;

    numHistBins = 100;
    showHistograms = false;

    cursorPos.setX(-1);
    selectionAxis = -1;
    animationAxis = -1;
    movingAxis = -1;

    // Event Filters
    this->installEventFilter(this);
    this->setMouseTracking(true);
    this->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), 
            this, SLOT(showContextMenu(const QPoint &)));
}

#define LINES_PER_DATAPT    (numDimensions-1)
#define POINTS_PER_LINE     2
#define FLOATS_PER_POINT    2
#define FLOATS_PER_COLOR    4

void ParallelCoordinatesVizWidget::processData()
{
    processed = false;

    if(dataSet->isEmpty())
        return;

    numDimensions = dataSet->at(0)->numDimensions;

    dimMins.resize(numDimensions);
    dimMaxes.resize(numDimensions);

    dimMins.fill(std::numeric_limits<double>::max());
    dimMaxes.fill(std::numeric_limits<double>::min());

    selMins.resize(numDimensions);
    selMaxes.resize(numDimensions);

    selMins.fill(-1);
    selMaxes.fill(-1);

    axesPositions.resize(numDimensions);
    axesOrder.resize(numDimensions);

    int numTotalElements = 0;
    for(int d=0; d<dataSet->size(); d++)
    {
        numTotalElements += dataSet->at(d)->numElements;
    }

    verts.resize(numTotalElements*LINES_PER_DATAPT*POINTS_PER_LINE*FLOATS_PER_POINT);
    colors.resize(numTotalElements*LINES_PER_DATAPT*POINTS_PER_LINE*FLOATS_PER_COLOR);

    histVals.resize(numDimensions);
    histMaxVals.resize(numDimensions);
    histMaxVals.fill(0);

    // Initial axis positions and order
    for(int i=0; i<numDimensions; i++)
    {
        if(!processed)
            axesOrder[i] = i;

        axesPositions[axesOrder[i]] = i*(1.0/(numDimensions-1));

        histVals[i].resize(numHistBins);
        histVals[i].fill(0);
    }

    processed = true;

    calcMinMaxes();
    calcHistBins();
    recalcLines();
}

void ParallelCoordinatesVizWidget::leaveEvent(QEvent *e)
{
    VizWidget::leaveEvent(e);
    needsRepaint = true;
}

void ParallelCoordinatesVizWidget::mousePressEvent(QMouseEvent *mouseEvent)
{
    if(!processed)
        return;

    QPoint mousePos = mouseEvent->pos();

    if(cursorPos.x() != -1 && mousePos.y() > plotBBox.top())
    {
        selectionAxis = cursorPos.x();
        firstSel = mousePos.y();
    }

    if(mousePos.y() > 0 && mousePos.y() < plotBBox.top())
    {
        movingAxis = getClosestAxis(mousePos.x());
    }
}

void ParallelCoordinatesVizWidget::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);

    if(!processed)
        return;

    if(event->button() == Qt::MouseButton::LeftButton && lastSel == -1)
    {
        selMins[selectionAxis] = -1;
        selMaxes[selectionAxis] = -1;
    }

    processSelection();

    movingAxis = -1;
    selectionAxis = -1;
    lastSel = -1;
}

bool ParallelCoordinatesVizWidget::eventFilter(QObject *obj, QEvent *event)
{
    Q_UNUSED(obj);

    if(!processed)
        return false;

    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    QPoint mousePos = mouseEvent->pos();
    QPoint mouseDelta = mousePos - prevMousePos;

    if (event->type() == QEvent::MouseMove)
    {
        // Dragging to create a selection
        if(selectionAxis != -1)
        {
            lastSel = clamp((qreal)mousePos.y(),plotBBox.top(),plotBBox.bottom());

            qreal selmin = std::min(firstSel,lastSel);
            qreal selmax = std::max(firstSel,lastSel);

            selMins[selectionAxis] = 1.0-scale(selmax,plotBBox.top(),plotBBox.bottom(),0,1);
            selMaxes[selectionAxis] = 1.0-scale(selmin,plotBBox.top(),plotBBox.bottom(),0,1);

            needsRepaint = true;
        }

        // Set cursor position
        int axis = getClosestAxis(mousePos.x());

        cursorPos.setX(axis);
        cursorPos.setY(std::max((int)mousePos.y(),(int)plotBBox.top()));
        cursorPos.setY(std::min((int)cursorPos.y(),(int)plotBBox.bottom()));

        if(cursorPos != prevCursorPos)
            needsRepaint = true;

        // Move axes
        if(movingAxis != -1 && mouseDelta.x() != 0)
        {
            axesPositions[movingAxis] += (qreal)mouseDelta.x()/plotBBox.width();

            // Sort moved axes
            for(int i=0; i<numDimensions-1; i++)
            {
                for(int j=i+1; j<numDimensions; j++)
                {
                    if(axesPositions[axesOrder[j]] <
                       axesPositions[axesOrder[i]])
                    {
                        int tmp = axesOrder[j];
                        axesOrder[j] = axesOrder[i];
                        axesOrder[i] = tmp;
                    }
                }
            }

            recalcLines();
            needsRepaint = true;
        }
    }

    prevMousePos = mousePos;
    prevCursorPos = cursorPos;

    return false;
}

int ParallelCoordinatesVizWidget::getClosestAxis(int xval)
{
    qreal dist;
    int closestDistance = plotBBox.width();
    int closestAxis = -1;
    for(int i=0; i<numDimensions; i++)
    {
        dist = abs(40+axesPositions[i]*plotBBox.width()-xval);
        if(dist < closestDistance)
        {
            closestDistance = dist;
            closestAxis = i;
        }
    }
    return closestAxis;
}

void ParallelCoordinatesVizWidget::processSelection()
{
    QVector<int> selDims;
    QVector<qreal> dataSelMins;
    QVector<qreal> dataSelMaxes;

    if(!processed)
        return;

    for(int i=0; i<selMins.size(); i++)
    {
        if(selMins[i] != -1)
        {
            selDims.push_back(i);
            dataSelMins.push_back(lerp(selMins[i],dimMins[i],dimMaxes[i]));
            dataSelMaxes.push_back(lerp(selMaxes[i],dimMins[i],dimMaxes[i]));
        }
    }

    if(selDims.isEmpty())
    {
        animationAxis = -1;
        selMins.fill(-1);
        selMaxes.fill(-1);
        dataSet->deselectAll();
    }
    else
    {
        dataSet->selectByMultiDimRange(selDims,dataSelMins,dataSelMaxes);
    }

    recalcLines();

    emit selectionChangedSig();
}

void ParallelCoordinatesVizWidget::calcMinMaxes()
{
    if(!processed)
        return;

    dimMins.fill(std::numeric_limits<double>::max());
    dimMaxes.fill(std::numeric_limits<double>::min());

    int elem;
    QVector<qreal>::Iterator p;
    for(int d=0; d<dataSet->size(); d++)
    {
        for(elem=0, p=dataSet->at(d)->begin; p!=dataSet->at(d)->end; elem++, p+=numDimensions)
        {
            if(!dataSet->at(d)->visible(elem))
                continue;

            for(int i=0; i<numDimensions; i++)
            {
                dimMins[i] = std::min(dimMins[i],*(p+i));
                dimMaxes[i] = std::max(dimMaxes[i],*(p+i));
            }
        }
    }
}

void ParallelCoordinatesVizWidget::calcHistBins()
{
    if(!processed)
        return;

    int elem;
    QVector<qreal>::Iterator p;
    for(int d=0; d<dataSet->size(); d++)
    {
        for(elem=0, p=dataSet->at(d)->begin; p!=dataSet->at(d)->end; elem++, p+=numDimensions)
        {
            if(dataSet->selectionDefined() && !dataSet->at(d)->selected(elem))
                continue;

            for(int i=0; i<numDimensions; i++)
            {
                int histBin = floor(scale(*(p+i),dimMins[i],dimMaxes[i],0,numHistBins));

                if(histBin >= numHistBins)
                    histBin = numHistBins-1;
                if(histBin < 0)
                    histBin = 0;

                histVals[i][histBin] += 1;
                histMaxVals[i] = std::max(histMaxVals[i],histVals[i][histBin]);
            }
        }
    }

    // Scale hist values to [0,1]
    for(int i=0; i<numDimensions; i++)
        for(int j=0; j<numHistBins; j++)
            histVals[i][j] = scale(histVals[i][j],0,histMaxVals[i],0,1);
}

void ParallelCoordinatesVizWidget::recalcLines(int dirtyAxis)
{
    QVector4D col;
    QVector2D a, b;
    int i, axis, nextAxis, elem, idx;
    QVector<double>::Iterator p;

    if(!processed)
        return;

    int allElems = 0;
    for(int d=0; d<dataSet->size(); d++)
    {

        QVector4D redVec = QVector4D(255,0,0,255);
        QColor dataSetColor = valToColor(d,0,dataSet->size(),colorMap);
        qreal Cr,Cg,Cb;
        dataSetColor.getRgbF(&Cr,&Cg,&Cb);
        QVector4D dataColor = QVector4D(Cr,Cg,Cb,1);

        for(p=dataSet->at(d)->begin, elem=0; p!=dataSet->at(d)->end; p+=numDimensions, elem++)
        {
            if(!dataSet->at(d)->visible(elem))
                col = QVector4D(0,0,0,0);
            else if(dataSet->at(d)->selected(elem))
            {
                col = redVec;
                col.setW(selOpacity);
            }
            else
            {
                col = dataColor;
                col.setW(unselOpacity);
            }


            idx = allElems*LINES_PER_DATAPT*POINTS_PER_LINE;
            allElems++;

            for(i=0; i<numDimensions-1; i++)
            {
                if(dirtyAxis != -1  && i != dirtyAxis && i != dirtyAxis-1)
                    continue;

                axis = axesOrder[i];
                nextAxis = axesOrder[i+1];

                float aVal = scale(*(p+axis),dimMins[axis],dimMaxes[axis],0,1);
                a = QVector2D(axesPositions[axis],aVal);

                float bVal = scale(*(p+nextAxis),dimMins[nextAxis],dimMaxes[nextAxis],0,1);
                b = QVector2D(axesPositions[nextAxis],bVal);

                int vertBaseIdx = idx*FLOATS_PER_POINT+i*POINTS_PER_LINE*FLOATS_PER_POINT;
                int colorBaseIdx = idx*FLOATS_PER_COLOR+i*POINTS_PER_LINE*FLOATS_PER_COLOR;

                verts[vertBaseIdx+0] = a.x();
                verts[vertBaseIdx+1] = a.y();

                verts[vertBaseIdx+2] = b.x();
                verts[vertBaseIdx+3] = b.y();

                colors[colorBaseIdx+0] = col.x();
                colors[colorBaseIdx+1] = col.y();
                colors[colorBaseIdx+2] = col.z();
                colors[colorBaseIdx+3] = col.w();

                colors[colorBaseIdx+4] = col.x();
                colors[colorBaseIdx+5] = col.y();
                colors[colorBaseIdx+6] = col.z();
                colors[colorBaseIdx+7] = col.w();
            }
        }
    }
}

void ParallelCoordinatesVizWidget::showContextMenu(const QPoint &pos)
{
    contextMenuMousePos = pos;

    QMenu contextMenu(tr("Axis Menu"), this);

    QAction actionAnimate("Animate!", this);
    connect(&actionAnimate, SIGNAL(triggered()), this, SLOT(beginAnimation()));
    contextMenu.addAction(&actionAnimate);

    contextMenu.exec(mapToGlobal(pos));
}

void ParallelCoordinatesVizWidget::selectionChangedSlot()
{
    calcHistBins();
    recalcLines();
    needsRepaint = true;
}

void ParallelCoordinatesVizWidget::visibilityChangedSlot()
{
    processData();
    calcMinMaxes();
    needsRepaint = true;
}

void ParallelCoordinatesVizWidget::setSelOpacity(int val)
{
    selOpacity = (qreal)val/1000.0;
    recalcLines();
    needsRepaint = true;
}

void ParallelCoordinatesVizWidget::setUnselOpacity(int val)
{
    unselOpacity = (qreal)val/1000.0;
    recalcLines();
    needsRepaint = true;
}

void ParallelCoordinatesVizWidget::setShowHistograms(bool checked)
{
    showHistograms = checked;
    needsRepaint = true;
}

void ParallelCoordinatesVizWidget::frameUpdate()
{
    if(animationAxis != -1)
    {
        selMins[animationAxis] += 0.005;
        selMaxes[animationAxis] += 0.005;

        processSelection();
        needsRepaint = true;

        if(selMaxes[animationAxis] >= 1)
            animationAxis = -1;
    }

    if(needsRepaint)
        repaint();
}

void ParallelCoordinatesVizWidget::beginAnimation()
{
    animationAxis = getClosestAxis(contextMenuMousePos.x());
    movingAxis = -1;

    qreal selDelta = selMaxes[animationAxis] - selMins[animationAxis];
    if(selDelta == 0)
        selDelta = 0.1;

    selMins[animationAxis] = 0;
    selMaxes[animationAxis] = selDelta;

    processSelection();
    needsRepaint = true;
}

void ParallelCoordinatesVizWidget::paintGL()
{
    glClear(GL_COLOR_BUFFER_BIT);

    if(!processed)
        return;

    makeCurrent();

    int mx=40;
    int my=30;

    glViewport(mx,
               my,
               width()-2*mx,
               height()-2*my);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 1.0, 0.0, 1.0, 0, 1);

    glShadeModel(GL_FLAT);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);

    glVertexPointer(FLOATS_PER_POINT,GL_FLOAT,0,verts.constData());
    glColorPointer(FLOATS_PER_COLOR,GL_FLOAT,0,colors.constData());

    glDrawArrays(GL_LINES,0,verts.size() / POINTS_PER_LINE / 10);

    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
}

void ParallelCoordinatesVizWidget::drawQtPainter(QPainter *painter)
{
    if(!processed)
        return;

    int mx=40;
    int my=30;

    plotBBox = QRectF(mx,my,
                      width()-mx-mx,
                      height()-my-my);

    // Draw axes
    QPointF a = plotBBox.bottomLeft();
    QPointF b = plotBBox.topLeft();

    QFontMetrics fm = painter->fontMetrics();
    painter->setBrush(QColor(0,0,0));
    for(int i=0; i<numDimensions; i++)
    {
        a.setX(plotBBox.left() + axesPositions[i]*plotBBox.width());
        b.setX(a.x());

        painter->drawLine(a,b);

        QString text = dataSet->at(0)->meta[i];
        QPointF center = b - QPointF(fm.width(text)/2,15);
        painter->drawText(center,text);

        text = QString::number(dimMins[i],'g',2);
        center = a - QPointF(fm.width(text)/2,-10);
        painter->drawText(center,text);

        text = QString::number(dimMaxes[i],'g',2);
        center = b - QPointF(fm.width(text)/2,0);
        painter->drawText(center,text);
    }

    // Draw cursor
    painter->setOpacity(1);
    if(cursorPos.x() != -1)
    {
        a.setX(plotBBox.left() + axesPositions[(int)cursorPos.x()]*plotBBox.width() - 10);
        a.setY(cursorPos.y());

        b.setX(plotBBox.left() + axesPositions[(int)cursorPos.x()]*plotBBox.width() + 10);
        b.setY(cursorPos.y());

        painter->drawLine(a,b);
    }

    // Draw selection boxes
    int cursorWidth = 28;
    int halfCursorWidth = cursorWidth/2;

    painter->setPen(QPen(Qt::yellow, 3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(Qt::NoBrush);
    for(int i=0; i<numDimensions; i++)
    {
        //if(selMins[i] != -1)
        {
            a = QPointF(plotBBox.left() + axesPositions[i]*plotBBox.width() - halfCursorWidth,
                        plotBBox.top() + plotBBox.height()*(1.0-selMins[i]));
            b = QPointF(a.x() + cursorWidth,
                        plotBBox.top() + plotBBox.height()*(1.0-selMaxes[i]));

            painter->drawRect(QRectF(a,b));
        }
    }

    if(showHistograms)
    {
        // Draw histograms
        a = plotBBox.bottomLeft();
        b = plotBBox.topLeft();

        painter->setPen(Qt::NoPen);
        painter->setBrush(QColor(31,120,180));
        painter->setOpacity(0.7);

        for(int i=0; i<numDimensions; i++)
        {
            a.setX(plotBBox.left() + axesPositions[i]*plotBBox.width());
            b.setX(a.x());

            for(int j=0; j<numHistBins; j++)
            {
                qreal histTop = a.y()-(j+1)*(plotBBox.height()/numHistBins);
                qreal histLeft = a.x();//-30*histVals[i][j];
                qreal histBottom = a.y()-(j)*(plotBBox.height()/numHistBins);
                qreal histRight = a.x()+60*histVals[i][j];
                painter->drawRect(QRectF(QPointF(histLeft,histTop),QPointF(histRight,histBottom)));
            }

            painter->drawLine(a,b);
        }
    }
}
