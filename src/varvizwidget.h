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

#ifndef VARVIZ_H
#define VARVIZ_H

#include "vizwidget.h"

struct varBlock
{
    QString name;
    qreal val;
    QRect block;
};

class VarViz : public VizWidget
{
    Q_OBJECT
public:
    VarViz(QWidget *parent = 0);
    ~VarViz();

signals:
    void variableSelected(int id);

public slots:
    void setVariable(QString var) 
        { variable = var; processData(); repaint(); }
    void setMaxVars(int num) 
        { numVariableBlocks = num; processData(); repaint(); }

protected:
    void processData();
    void selectionChangedSlot();
    void drawQtPainter(QPainter *painter);

    void mouseReleaseEvent(QMouseEvent *e);

private:
    int getVariableID(QString name);

private:
    QString variable;

    int margin;
    QRect drawSpace;

    int numVariableBlocks;

    QVector<varBlock> varBlocks;
    qreal varMaxVal;
};

#endif // VARVIZ_H
