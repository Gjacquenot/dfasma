/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

#ifndef FTLABELS_H
#define FTLABELS_H

#include <deque>
#include <vector>
#include <iostream>

#include <QString>
#include <QColor>
#include <QAction>
#include <QGraphicsTextItem>

#include "filetype.h"

class QGraphicsSimpleTextItem;
class QGraphicsLineItem;
class FTLabels;
class FTFZero;

class FTGraphicsLabelItem : public QGraphicsTextItem
{
    FTLabels* m_ftl;
    static bool s_isEditing;
    QString m_prevText;

public:
    FTGraphicsLabelItem(FTLabels* ftl, const QString & text);
    FTGraphicsLabelItem(FTGraphicsLabelItem* ftgi, FTLabels* ftl); // copy ctor

    virtual void keyPressEvent(QKeyEvent * event);
    virtual void focusInEvent(QFocusEvent * event);
    virtual void focusOutEvent(QFocusEvent * event);

    virtual void paint(QPainter *painter,const QStyleOptionGraphicsItem *option,QWidget *widget);

    static bool isEditing() {return s_isEditing;}
};


class FTLabels : public QObject, public FileType
{
    Q_OBJECT

public:
    enum FileFormat {FFNotSpecified=0, FFAutoDetect, FFTEXTAutoDetect, FFTEXTTimeText, FFTEXTSegmentsFloat, FFTEXTSegmentsSample, FFTEXTSegmentsHTK, FFSDIF};
    static std::deque<QString> s_formatstrings;

private:
    static struct ClassConstructor{ClassConstructor();} s_class_constructor;
    void constructor_internal();
    void constructor_external();
    void load();
    void sort(); // For keeping files in ascending order
    QString extractCenterLabel(const QString& txt);

    QAction* m_actionSave;
    QAction* m_actionSaveAs;

    FileFormat m_fileformat;

public:
    FTLabels(QObject *parent);
    FTLabels(const FTLabels& ft);
    FTLabels(const QString& _fileName, QObject* parent, FileType::FileContainer container=FileType::FCUNSET, FileFormat fileformat=FFNotSpecified);
    virtual FileType* duplicate();

    std::deque<double> starts; // TODO Get ride of ?
    std::deque<FTGraphicsLabelItem*> waveform_labels;
    std::deque<QGraphicsSimpleTextItem*> spectrogram_labels;
    std::deque<QGraphicsLineItem*> waveform_lines;
    std::deque<QGraphicsLineItem*> spectrogram_lines;

    virtual QString info() const;
    virtual double getLastSampleTime() const;
    virtual void fillContextMenu(QMenu& contextmenu);
    void updateTextsGeometryWaveform();
    void updateTextsGeometrySpectrogram();

    int getNbLabels() const                      {return int(starts.size());}
    void moveLabel(int index, double position);
    void moveAllLabel(double delay);
    void finishEditing()                         {sort();}
    void changeText(int index, const QString& text);
    void setColor(const QColor& _color);

    ~FTLabels();

    // Estimation
    FTLabels(QObject* parent, FTFZero *ftfzero, double tstart=-1.0, double tend=-1.0);
    void estimate(FTFZero *ftfzero, double tstart=-1.0, double tend=-1.0);
    static QString createFileNameFromFZero(const QString& fzerofilename);
    FTFZero* m_src_fzero;

public slots:
    bool reload();
    void save();
    void saveAs();
    void clear();
    void removeLabel(int index);
    void addLabel(double position, const QString& text, QString showntxt="");
    void setVisible(bool shown);
};

#endif // FTLABELS_H
