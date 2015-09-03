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

#include "gvamplitudespectrum.h"

#include "wmainwindow.h"
#include "ui_wmainwindow.h"

#include "wdialogsettings.h"
#include "ui_wdialogsettings.h"

#include "gvamplitudespectrumwdialogsettings.h"
#include "ui_gvamplitudespectrumwdialogsettings.h"

#include "gvwaveform.h"
#include "gvphasespectrum.h"
#include "gvspectrumgroupdelay.h"
#include "gvspectrogram.h"
#include "ftsound.h"
#include "ftfzero.h"
#include "sigproc.h"

#include <iostream>
#include <algorithm>
using namespace std;

#include <QWheelEvent>
#include <QToolBar>
#include <QAction>
#include <QSpinBox>
#include <QGraphicsLineItem>
#include <QStaticText>
#include <QDebug>
#include <QTime>
#include <QMessageBox>
#include <QScrollBar>
#include <QToolTip>

#include "qthelper.h"

QGVAmplitudeSpectrum::QGVAmplitudeSpectrum(WMainWindow* parent)
    : QGraphicsView(parent)
{
    setStyleSheet("QGraphicsView { border-style: none; }");
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);
    setHorizontalScrollBar(new QScrollBarHover(Qt::Horizontal, this));
    setVerticalScrollBar(new QScrollBarHover(Qt::Vertical, this));

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_dlgSettings = new GVAmplitudeSpectrumWDialogSettings(this);
    gMW->m_settings.add(gMW->ui->sldAmplitudeSpectrumMin);

    m_aShowProperties = new QAction(tr("&Properties"), this);
    m_aShowProperties->setStatusTip(tr("Open the properties configuration panel of the spectrum view"));
    m_aShowProperties->setIcon(QIcon(":/icons/settings.svg"));

    m_gridFontPen.setColor(QColor(128,128,128));
    m_gridFontPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)

    m_aAmplitudeSpectrumShowGrid = new QAction(tr("Show &grid"), this);
    m_aAmplitudeSpectrumShowGrid->setObjectName("m_aAmplitudeSpectrumShowGrid");
    m_aAmplitudeSpectrumShowGrid->setStatusTip(tr("Show or hide the grid"));
    m_aAmplitudeSpectrumShowGrid->setIcon(QIcon(":/icons/grid.svg"));
    m_aAmplitudeSpectrumShowGrid->setCheckable(true);
    m_aAmplitudeSpectrumShowGrid->setChecked(true);
    gMW->m_settings.add(m_aAmplitudeSpectrumShowGrid);
    connect(m_aAmplitudeSpectrumShowGrid, SIGNAL(toggled(bool)), m_scene, SLOT(invalidate()));

    connect(gMW->m_gvWaveform->m_aWaveformShowSelectedWaveformOnTop, SIGNAL(triggered()), m_scene, SLOT(update()));

    m_aAmplitudeSpectrumShowWindow = new QAction(tr("Show &window"), this);
    m_aAmplitudeSpectrumShowWindow->setObjectName("m_aAmplitudeSpectrumShowWindow");
    m_aAmplitudeSpectrumShowWindow->setStatusTip(tr("Show &window"));
    m_aAmplitudeSpectrumShowWindow->setCheckable(true);
    m_aAmplitudeSpectrumShowWindow->setIcon(QIcon(":/icons/window.svg"));
    gMW->m_settings.add(m_aAmplitudeSpectrumShowWindow);
    connect(m_aAmplitudeSpectrumShowWindow, SIGNAL(toggled(bool)), this, SLOT(updateDFTs()));

    m_aAmplitudeSpectrumShowLoudnessCurve = new QAction(tr("Show &loudness curve"), this);
    m_aAmplitudeSpectrumShowLoudnessCurve->setObjectName("m_aAmplitudeSpectrumShowLoudnessCurve");
    m_aAmplitudeSpectrumShowLoudnessCurve->setStatusTip(tr("Show the loudness curve which is use for the spectrogram weighting."));
    m_aAmplitudeSpectrumShowLoudnessCurve->setCheckable(true);
    m_aAmplitudeSpectrumShowLoudnessCurve->setChecked(false);
    m_aAmplitudeSpectrumShowLoudnessCurve->setIcon(QIcon(":/icons/noun_29196_cc.svg"));
    gMW->m_settings.add(m_aAmplitudeSpectrumShowLoudnessCurve);
    connect(m_aAmplitudeSpectrumShowLoudnessCurve, SIGNAL(toggled(bool)), m_scene, SLOT(update()));

    m_aFollowPlayCursor = new QAction(tr("Follow the play cursor"), this);;
    m_aFollowPlayCursor->setObjectName("m_aFollowPlayCursor");
    m_aFollowPlayCursor->setStatusTip(tr("Update the DFT view according to the play cursor position"));
    m_aFollowPlayCursor->setCheckable(true);
    m_aFollowPlayCursor->setChecked(false);
    gMW->m_settings.add(m_aFollowPlayCursor);

    m_fft = new sigproc::FFTwrapper();
    sigproc::FFTwrapper::setTimeLimitForPlanPreparation(m_dlgSettings->ui->sbAmplitudeSpectrumFFTW3MaxTimeForPlanPreparation->value());
    m_fftresizethread = new FFTResizeThread(m_fft, this);

    // Cursor
    m_giCursorHoriz = new QGraphicsLineItem(0, -1000, 0, 1000);
    QPen cursorPen(QColor(64, 64, 64));
    cursorPen.setWidth(0);
    m_giCursorHoriz->setPen(cursorPen);
    m_giCursorHoriz->hide();
    m_scene->addItem(m_giCursorHoriz);
    m_giCursorVert = new QGraphicsLineItem(0, 0, gFL->getFs()/2.0, 0);
    m_giCursorVert->setPen(cursorPen);
    m_giCursorVert->hide();
    m_scene->addItem(m_giCursorVert);
    QFont font("Helvetica", 10);
    m_giCursorPositionXTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionXTxt->setBrush(QColor(64, 64, 64));
    m_giCursorPositionXTxt->setFont(font);
    m_giCursorPositionXTxt->hide();
    m_scene->addItem(m_giCursorPositionXTxt);
    m_giCursorPositionYTxt = new QGraphicsSimpleTextItem();
    m_giCursorPositionYTxt->setBrush(QColor(64, 64, 64));
    m_giCursorPositionYTxt->setFont(font);
    m_giCursorPositionYTxt->hide();
    m_scene->addItem(m_giCursorPositionYTxt);

    // Selection
    m_currentAction = CANothing;
    m_giShownSelection = new QGraphicsRectItem();
    m_giShownSelection->hide();
    m_scene->addItem(m_giShownSelection);
    m_giSelectionTxt = new QGraphicsSimpleTextItem();
    m_giSelectionTxt->hide();
    m_giSelectionTxt->setBrush(QColor(64, 64, 64));
    m_giSelectionTxt->setFont(font);
    m_scene->addItem(m_giSelectionTxt);
    QPen selectionPen(QColor(64, 64, 64));
    selectionPen.setWidth(0);
    QBrush selectionBrush(QColor(192, 192, 192));
    m_giShownSelection->setPen(selectionPen);
    m_giShownSelection->setBrush(selectionBrush);
    m_giShownSelection->setOpacity(0.5);
    gMW->ui->lblSpectrumSelectionTxt->setText("No selection");

    // Min and max limits of the color range
    cursorPen = QPen(QColor(255, 0, 0));
    cursorPen.setWidth(0);
    m_giSpectrogramMax = new QGraphicsLineItem(0, 0, gFL->getFs()/2.0, 0);
    m_giSpectrogramMax->setPen(cursorPen);
    m_giSpectrogramMax->hide();
    m_scene->addItem(m_giSpectrogramMax);
    m_giSpectrogramMin = new QGraphicsLineItem(0, 0, gFL->getFs()/2.0, 0);
    m_giSpectrogramMin->setPen(cursorPen);
    m_giSpectrogramMin->hide();
    m_scene->addItem(m_giSpectrogramMin);

    // Build actions
    m_aZoomIn = new QAction(tr("Zoom In"), this);;
    m_aZoomIn->setStatusTip(tr("Zoom In"));
    m_aZoomIn->setShortcut(Qt::Key_Plus);
    m_aZoomIn->setIcon(QIcon(":/icons/zoomin.svg"));
    connect(m_aZoomIn, SIGNAL(triggered()), this, SLOT(azoomin()));
    m_aZoomOut = new QAction(tr("Zoom Out"), this);;
    m_aZoomOut->setStatusTip(tr("Zoom Out"));
    m_aZoomOut->setShortcut(Qt::Key_Minus);
    m_aZoomOut->setIcon(QIcon(":/icons/zoomout.svg"));
    connect(m_aZoomOut, SIGNAL(triggered()), this, SLOT(azoomout()));
    m_aUnZoom = new QAction(tr("Un-Zoom"), this);
    m_aUnZoom->setStatusTip(tr("Un-Zoom"));
    m_aUnZoom->setIcon(QIcon(":/icons/unzoomxy.svg"));
    connect(m_aUnZoom, SIGNAL(triggered()), this, SLOT(aunzoom()));
    m_aZoomOnSelection = new QAction(tr("&Zoom on selection"), this);
    m_aZoomOnSelection->setStatusTip(tr("Zoom on selection"));
    m_aZoomOnSelection->setEnabled(false);
    //m_aZoomOnSelection->setShortcut(Qt::Key_S); // This one creates "ambiguous" shortcuts
    m_aZoomOnSelection->setIcon(QIcon(":/icons/zoomselectionxy.svg"));
    connect(m_aZoomOnSelection, SIGNAL(triggered()), this, SLOT(selectionZoomOn()));

    m_aSelectionClear = new QAction(tr("Clear selection"), this);
    m_aSelectionClear->setStatusTip(tr("Clear the current selection"));
    QIcon selectionclearicon(":/icons/selectionclear.svg");
    m_aSelectionClear->setIcon(selectionclearicon);
    m_aSelectionClear->setEnabled(false);
    connect(m_aSelectionClear, SIGNAL(triggered()), this, SLOT(selectionClear()));

    m_aAutoUpdateDFT = new QAction(tr("Auto-Update DFT"), this);;
    m_aAutoUpdateDFT->setStatusTip(tr("Auto-Update the DFT view when the time selection is modified"));
    m_aAutoUpdateDFT->setCheckable(true);
    m_aAutoUpdateDFT->setChecked(true);
    m_aAutoUpdateDFT->setIcon(QIcon(":/icons/autoupdate.svg"));
    connect(m_aAutoUpdateDFT, SIGNAL(toggled(bool)), this, SLOT(settingsModified()));

    gMW->ui->lblSpectrumSelectionTxt->setText("No selection");

    showScrollBars(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow->isChecked());
    connect(gMW->m_dlgSettings->ui->cbViewsScrollBarsShow, SIGNAL(toggled(bool)), this, SLOT(showScrollBars(bool)));
    setResizeAnchor(QGraphicsView::AnchorViewCenter);
    setMouseTracking(true);

    gMW->ui->pgbFFTResize->hide();
    gMW->ui->lblSpectrumInfoTxt->setText("");

    connect(m_fftresizethread, SIGNAL(fftResized(int,int)), this, SLOT(updateDFTs()));
    connect(m_fftresizethread, SIGNAL(fftResizing(int,int)), this, SLOT(fftResizing(int,int)));

    // Fill the toolbar
    m_toolBar = new QToolBar(this);
//    m_toolBar->addAction(m_aAutoUpdateDFT);
//    m_toolBar->addSeparator();
//    m_toolBar->addAction(m_aZoomIn);
//    m_toolBar->addAction(m_aZoomOut);
    m_toolBar->addAction(m_aUnZoom);
//    m_toolBar->addSeparator();
    m_toolBar->addAction(m_aZoomOnSelection);
    m_toolBar->addAction(m_aSelectionClear);
    m_toolBar->setIconSize(QSize(gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value(), gMW->m_dlgSettings->ui->sbViewsToolBarSizes->value()));
    m_toolBar->setOrientation(Qt::Vertical);
    gMW->ui->lSpectraToolBar->addWidget(m_toolBar);

    // Build the context menu
    m_contextmenu.addAction(m_aAmplitudeSpectrumShowGrid);
//    m_contextmenu.addAction(gMW->m_gvWaveform->m_aShowSelectedWaveformOnTop);
    m_contextmenu.addAction(m_aAmplitudeSpectrumShowWindow);
    m_contextmenu.addAction(m_aAmplitudeSpectrumShowLoudnessCurve);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aAutoUpdateDFT);
    m_contextmenu.addAction(m_aFollowPlayCursor);
    m_contextmenu.addSeparator();
    m_contextmenu.addAction(m_aShowProperties);
    connect(m_aShowProperties, SIGNAL(triggered()), m_dlgSettings, SLOT(exec()));
    connect(m_dlgSettings, SIGNAL(accepted()), this, SLOT(settingsModified()));

    connect(gMW->ui->sldAmplitudeSpectrumMin, SIGNAL(valueChanged(int)), this, SLOT(amplitudeMinChanged()));
}

void QGVAmplitudeSpectrum::showScrollBars(bool show) {
    if(show) {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
        if(!gMW->ui->actionShowPhaseSpectrum->isChecked())
            setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
    }
    else {
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }
}

void QGVAmplitudeSpectrum::settingsModified(){
//    COUTD << "QGVAmplitudeSpectrum::settingsModified " << gMW->m_gvWaveform->m_mouseSelection << endl;
    if(gMW->m_gvWaveform)
        gMW->m_gvWaveform->selectionSet(gMW->m_gvWaveform->m_mouseSelection, true);
}

void QGVAmplitudeSpectrum::updateAmplitudeExtent(){
//    COUTD << "QGVAmplitudeSpectrum::updateAmplitudeExtent" << endl;

    if(gFL->ftsnds.size()>0){
        // Get the maximum QSNR among all sound files
        WAVTYPE maxsqnr = -std::numeric_limits<WAVTYPE>::infinity();
        for(unsigned int si=0; si<gFL->ftsnds.size(); si++){
            if(gFL->ftsnds[si]->format().sampleSize()==-1)
                maxsqnr = std::max(maxsqnr, 20*std::log10(std::pow(2.0, 8*sizeof(WAVTYPE))));
            else
                maxsqnr = std::max(maxsqnr, 20*std::log10(std::pow(2.0, gFL->ftsnds[si]->format().sampleSize())));
        }

        gMW->ui->sldAmplitudeSpectrumMin->setMaximum(0);
        gMW->ui->sldAmplitudeSpectrumMin->setMinimum(-3*maxsqnr); // to give a margin

        updateSceneRect();
    }

//    COUTD << "QGVAmplitudeSpectrum::~updateAmplitudeExtent" << endl;
}

void QGVAmplitudeSpectrum::amplitudeMinChanged() {
//    COUTD << "QGVAmplitudeSpectrum::amplitudeMinChanged " << gMW->ui->sldAmplitudeSpectrumMin->value() << endl;

    if(!gMW->isLoading())
        QToolTip::showText(QCursor::pos(), QString("%1dB").arg(gMW->ui->sldAmplitudeSpectrumMin->value()), this);

    updateSceneRect();
    viewSet(m_scene->sceneRect(), false);

//    cout << "QGVAmplitudeSpectrum::~amplitudeMinChanged" << endl;
}

void QGVAmplitudeSpectrum::updateSceneRect() {
//    COUTD << "QGVAmplitudeSpectrum::updateSceneRect " << gMW->getFs() << endl;
    m_scene->setSceneRect(0.0, -10, gFL->getFs()/2, (10-gMW->ui->sldAmplitudeSpectrumMin->value()));
}

void QGVAmplitudeSpectrum::fftResizing(int prevSize, int newSize){
    Q_UNUSED(prevSize);

    gMW->ui->pgbFFTResize->show();
    gMW->ui->lblSpectrumInfoTxt->setText(QString("Optimizing DFT for %1").arg(newSize));
}

void QGVAmplitudeSpectrum::setWindowRange(qreal tstart, qreal tend){
//    COUTD << "QGVAmplitudeSpectrum::setWindowRange " << tstart << "," << tend << endl;

    if(tstart==tend)
        return;

    if(m_dlgSettings->ui->cbAmplitudeSpectrumLimitWindowDuration->isChecked() && (tend-tstart)>m_dlgSettings->ui->sbAmplitudeSpectrumWindowDurationLimit->value())
        tend = tstart+m_dlgSettings->ui->sbAmplitudeSpectrumWindowDurationLimit->value();

    unsigned int nl = std::max(0, int(0.5+tstart*gFL->getFs()));
    unsigned int nr = int(0.5+std::min(gFL->getMaxLastSampleTime(),tend)*gFL->getFs());

    if((nr-nl+1)%2==0
       && m_dlgSettings->ui->cbAmplitudeSpectrumWindowSizeForcedOdd->isChecked())
        nr++;

    if(nl==nr)
        return;

    int winlen = nr-nl+1;
    if(winlen<2)
        return;

    // The window's shape
    int wintype = m_dlgSettings->ui->cbAmplitudeSpectrumWindowType->currentIndex();

    int normtype = m_dlgSettings->ui->cbAmplitudeSpectrumWindowsNormalisation->currentIndex();

    FTSound::DFTParameters newDFTParams(nl, nr, winlen, wintype, normtype);

    if(m_trgDFTParameters.isEmpty()
       || m_trgDFTParameters.winlen!=newDFTParams.winlen
       || m_trgDFTParameters.wintype!=newDFTParams.wintype
       || m_trgDFTParameters.normtype!=newDFTParams.normtype
       || wintype>7){

        if(wintype==0)
            m_win = sigproc::rectangular(newDFTParams.winlen);
        else if(wintype==1)
            m_win = sigproc::hamming(newDFTParams.winlen);
        else if(wintype==2)
            m_win = sigproc::hann(newDFTParams.winlen);
        else if(wintype==3)
            m_win = sigproc::blackman(newDFTParams.winlen);
        else if(wintype==4)
            m_win = sigproc::blackmannutall(newDFTParams.winlen);
        else if(wintype==5)
            m_win = sigproc::blackmanharris(newDFTParams.winlen);
        else if(wintype==6)
            m_win = sigproc::nutall(newDFTParams.winlen);
        else if(wintype==7)
            m_win = sigproc::flattop(newDFTParams.winlen);
        else if(wintype==8)
            m_win = sigproc::normwindow(newDFTParams.winlen, m_dlgSettings->ui->spAmplitudeSpectrumWindowNormSigma->value());
        else if(wintype==9)
            m_win = sigproc::expwindow(newDFTParams.winlen, m_dlgSettings->ui->spAmplitudeSpectrumWindowExpDecay->value());
        else if(wintype==10)
            m_win = sigproc::gennormwindow(newDFTParams.winlen, m_dlgSettings->ui->spAmplitudeSpectrumWindowNormSigma->value(), m_dlgSettings->ui->spAmplitudeSpectrumWindowNormPower->value());
        else
            throw QString("No window selected");

        double winsum = 0.0;
        if(normtype==0) {
            // Normalize the window's sum to 1
            for(int n=0; n<newDFTParams.winlen; n++)
                winsum += m_win[n];
        }
        else if(normtype==1) {
            // Normalize the window's energy to 1
            for(int n=0; n<newDFTParams.winlen; n++)
                winsum += m_win[n]*m_win[n];
        }
        for(int n=0; n<newDFTParams.winlen; n++)
            m_win[n] /= winsum;

        newDFTParams.win = m_win;
    }

    // Set the DFT length
    if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==0)
        newDFTParams.dftlen = std::max(newDFTParams.winlen, m_dlgSettings->ui->sbAmplitudeSpectrumDFTSize->value());
    else if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==1)
        newDFTParams.dftlen = std::pow(2.0, std::ceil(log2(float(newDFTParams.winlen)))+m_dlgSettings->ui->sbAmplitudeSpectrumOversamplingFactor->value());
    else if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==2){
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        int dftlen = viewport()->rect().width()/((viewrect.right()-viewrect.left())/gFL->getFs());
        dftlen = std::max(dftlen, newDFTParams.winlen);
        newDFTParams.dftlen = std::pow(2.0, std::ceil(log2(float(dftlen))));
    }

    if(newDFTParams==m_trgDFTParameters)
        return;

    if(newDFTParams.winlen!=m_trgDFTParameters.winlen
        && gMW->m_gvSpectrumGroupDelay
        && gMW->ui->actionShowGroupDelaySpectrum->isChecked()){
        gMW->m_gvSpectrumGroupDelay->updateSceneRect(((newDFTParams.winlen-1)/2)/gFL->getFs());
    }

    // From now on we want the new parameters ...
    m_trgDFTParameters = newDFTParams;

    // Update the visible window in the waveform
    if(m_trgDFTParameters.win.size()>0) {
        double fs = gFL->getFs();

        FFTTYPE winmax = 0.0;
        for(size_t n=0; n<m_trgDFTParameters.win.size(); n++)
            winmax = std::max(winmax, m_trgDFTParameters.win[n]);
        winmax = 1.0/winmax;

        QPainterPath path;

        qreal prevx = 0;
        qreal prevy = m_trgDFTParameters.win[0]*winmax;
        path.moveTo(QPointF(prevx, -prevy));
        qreal y;
        for(size_t n=1; n<m_trgDFTParameters.win.size(); n++) {
            qreal x = n/fs;
            y = m_trgDFTParameters.win[n];
            y *= winmax;
            path.lineTo(QPointF(x, -y));
            prevx = x;
            prevy = y;
        }

        // Add the vertical line
        qreal winrelcenter = ((m_trgDFTParameters.win.size()-1)/2.0)/fs;
        path.moveTo(QPointF(winrelcenter, 2.0));
        path.lineTo(QPointF(winrelcenter, -1.0));

        gMW->m_gvWaveform->m_giWindow->setPath(path);
        gMW->m_gvWaveform->m_giWindow->setPos(tstart, 0.0);
    }

    // ... so let's see which DFTs we have to update.
    if(m_aAutoUpdateDFT->isChecked())
        updateDFTs();
}

void QGVAmplitudeSpectrum::updateDFTs(){
//    COUTD << "QGVAmplitudeSpectrum::updateDFTs " << endl;
    if(m_trgDFTParameters.win.size()<2) // Avoid the DFT of one sample ...
        return;

    m_fftresizethread->resize(m_trgDFTParameters.dftlen);

    if(m_fftresizethread->m_mutex_resizing.tryLock()){
        int dftlen = m_fft->size(); // Local copy of the actual dftlen

        gMW->ui->pgbFFTResize->hide();
        gMW->ui->lblSpectrumInfoTxt->setText(QString("DFT size=%1").arg(dftlen));

        std::vector<FFTTYPE>& win = m_trgDFTParameters.win; // For speeding up access

        bool didany = false;
        for(unsigned int fi=0; fi<gFL->ftsnds.size(); fi++){
            FTSound* snd = gFL->ftsnds[fi];
            if(!snd->isVisible())
                continue;

            if(!snd->m_dftparams.isEmpty()
               && snd->m_dftparams==m_trgDFTParameters
               && snd->m_dftparams.wav==snd->wavtoplay
               && snd->m_dftparams.ampscale==snd->m_ampscale
               && snd->m_dftparams.delay==snd->m_delay)
                continue;

            WAVTYPE gain = snd->m_ampscale;

            snd->m_dft_min = std::numeric_limits<WAVTYPE>::infinity();
            snd->m_dft_max = -std::numeric_limits<WAVTYPE>::infinity();
            if(gFL->ftsnds[fi]->m_actionInvPolarity->isChecked())
                gain *= -1;

            int n = 0;
            int wn = 0;
            for(; n<m_trgDFTParameters.winlen; n++){
                wn = m_trgDFTParameters.nl+n - snd->m_delay;

                if(wn>=0 && wn<int(snd->wavtoplay->size())) {
                    WAVTYPE value = gain*(*(snd->wavtoplay))[wn];

                    if(value>1.0)       value = 1.0;
                    else if(value<-1.0) value = -1.0;

                    m_fft->in[n] = value*win[n];
                }
                else
                    m_fft->in[n] = 0.0;
            }
            for(; n<dftlen; n++)
                m_fft->in[n] = 0.0;

            m_fft->execute(); // Compute the DFT

            // Store first the complex values of the DFT
            // (so that it can be used to compute the group delay)
            std::vector<std::complex<WAVTYPE> >* pdft = &(snd->m_dft);
            snd->m_dft.resize(dftlen/2+1);
            for(n=0; n<dftlen/2+1; n++)
                (*pdft)[n] = m_fft->out[n];

            // If the group delay is requested, update its data
            if(gMW->ui->actionShowGroupDelaySpectrum->isChecked()){
                // y = nx[n]
                for(int n=0; n<m_trgDFTParameters.winlen; n++)
                    m_fft->in[n] *= n;

                m_fft->execute(); // Compute the DFT of y

                // (Xr*Yr+Xi*Yi) / |X|^2
                snd->m_gd.resize(dftlen/2+1);
                WAVTYPE fs = gFL->getFs();
                for(int n=0; n<dftlen/2+1; n++) {
                    WAVTYPE xp2 = std::real(snd->m_dft[n])*std::real(snd->m_dft[n]) + std::imag(snd->m_dft[n])*std::imag(snd->m_dft[n]);
                    snd->m_gd[n] = (std::real(snd->m_dft[n])*std::real(m_fft->out[n]) + std::imag(snd->m_dft[n])*std::imag(m_fft->out[n]))/xp2;
                    snd->m_gd[n] -= (m_trgDFTParameters.winlen-1)/2; // Remove the window's delay

                    snd->m_gd[n] /= fs; // measure it in [second]
                }
            }

            // Convert the spectrum values to log values
            pdft = &(snd->m_dft);
            for(n=0; n<dftlen/2+1; n++) {
                (*pdft)[n] = std::complex<WAVTYPE>(std::log(std::abs((*pdft)[n])),std::arg((*pdft)[n]));
                WAVTYPE y = (*pdft)[n].real();
                snd->m_dft_min = std::min(snd->m_dft_min, y);
                snd->m_dft_max = std::max(snd->m_dft_max, y);
            }
            snd->m_dftparams = m_trgDFTParameters;
            snd->m_dftparams.wav = snd->wavtoplay;
            snd->m_dftparams.ampscale = snd->m_ampscale;
            snd->m_dftparams.delay = snd->m_delay;

            didany = true;
        }

        // Compute the window's DFT
        if (m_aAmplitudeSpectrumShowWindow->isChecked()) {
            int n = 0;
            for(; n<m_trgDFTParameters.winlen; n++)
                m_fft->in[n] = m_trgDFTParameters.win[n];
            for(; n<dftlen; n++)
                m_fft->in[n] = 0.0;

            m_fft->execute();

            m_windft.resize(dftlen/2+1);
            for(n=0; n<dftlen/2+1; n++)
                m_windft[n] = std::complex<WAVTYPE>(std::log(std::abs(m_fft->out[n])),std::arg(m_fft->out[n]));

            didany = true;
        }

        m_fftresizethread->m_mutex_resizing.unlock();

        if(didany){
            m_scene->update();
            if(gMW->m_gvPhaseSpectrum)
                gMW->m_gvPhaseSpectrum->m_scene->update();
            if(gMW->m_gvSpectrumGroupDelay)
                gMW->m_gvSpectrumGroupDelay->m_scene->update();
        }
    }

//    COUTD << "~QGVAmplitudeSpectrum::updateDFTs" << endl;
}

void QGVAmplitudeSpectrum::viewSet(QRectF viewrect, bool sync) {
//    cout << "QGVAmplitudeSpectrum::viewSet" << endl;

    QRectF currentviewrect = mapToScene(viewport()->rect()).boundingRect();

    if(viewrect==QRect() || viewrect!=currentviewrect) {
        if(viewrect==QRectF())
            viewrect = currentviewrect;

        if(viewrect.top()<=m_scene->sceneRect().top())
            viewrect.setTop(m_scene->sceneRect().top());
        if(viewrect.bottom()>=m_scene->sceneRect().bottom())
            viewrect.setBottom(m_scene->sceneRect().bottom());
        if(viewrect.left()<m_scene->sceneRect().left())
            viewrect.setLeft(m_scene->sceneRect().left());
        if(viewrect.right()>m_scene->sceneRect().right())
            viewrect.setRight(m_scene->sceneRect().right());

        // This is not perfect and might never be because:
        // 1) The workaround removeHiddenMargin is apparently simplistic
        // 2) This position in real coordinates involves also the position of the
        //    scrollbars which fit integers only. Thus the final position is always
        //    aproximative.
        // A solution would be to subclass QSplitter, catch the view when the
        // splitter's handle is clicked, and repeat this view until button released.
        fitInView(removeHiddenMargin(this, viewrect));

        for(size_t fi=0; fi<gFL->ftfzeros.size(); fi++)
            gFL->ftfzeros[fi]->updateTextsGeometry();

        if(sync){
            if(gMW->m_gvPhaseSpectrum && gMW->ui->actionShowPhaseSpectrum->isChecked()) {
                QRectF phaserect = gMW->m_gvPhaseSpectrum->mapToScene(gMW->m_gvPhaseSpectrum->viewport()->rect()).boundingRect();
                phaserect.setLeft(viewrect.left());
                phaserect.setRight(viewrect.right());
                gMW->m_gvPhaseSpectrum->viewSet(phaserect, false);
            }

            if(gMW->m_gvSpectrumGroupDelay && gMW->ui->actionShowGroupDelaySpectrum->isChecked()) {
                QRectF gdrect = gMW->m_gvSpectrumGroupDelay->mapToScene(gMW->m_gvSpectrumGroupDelay->viewport()->rect()).boundingRect();
                gdrect.setLeft(viewrect.left());
                gdrect.setRight(viewrect.right());
                gMW->m_gvSpectrumGroupDelay->viewSet(gdrect, false);
            }
        }
    }

    if(m_dlgSettings->ui->cbAmplitudeSpectrumDFTSizeType->currentIndex()==2)
        setWindowRange(gMW->m_gvWaveform->m_selection.left(), gMW->m_gvWaveform->m_selection.right());

//    cout << "QGVAmplitudeSpectrum::~viewSet" << endl;
}

void QGVAmplitudeSpectrum::resizeEvent(QResizeEvent* event){
//    COUTD << "QGVAmplitudeSpectrum::resizeEvent" << endl;

    // Note: Resized is called for all views so better to not forward modifications
    if(event->oldSize().isEmpty() && !event->size().isEmpty()) {

        updateSceneRect();

        if(gMW->m_gvPhaseSpectrum->viewport()->rect().width()*gMW->m_gvPhaseSpectrum->viewport()->rect().height()>0){
            QRectF phaserect = gMW->m_gvPhaseSpectrum->mapToScene(gMW->m_gvPhaseSpectrum->viewport()->rect()).boundingRect();

            QRectF viewrect;
            viewrect.setLeft(phaserect.left());
            viewrect.setRight(phaserect.right());
            viewrect.setTop(-10);
            viewrect.setBottom(-gMW->ui->sldAmplitudeSpectrumMin->value());
            viewSet(viewrect, false);
        }
        else
            viewSet(m_scene->sceneRect(), false);
    }
    else if(!event->oldSize().isEmpty() && !event->size().isEmpty())
    {
        viewSet(mapToScene(QRect(QPoint(0,0), event->oldSize())).boundingRect(), false);
    }

    viewUpdateTexts();
    setMouseCursorPosition(QPointF(-1,0), false);

    if((event->oldSize().width()==-1 || event->oldSize().height()==-1
            || event->oldSize().width()*event->oldSize().height()==0)
       && event->size().width()*event->size().height()>0)
        updateDFTs();

//    COUTD << "QGVAmplitudeSpectrum::~resizeEvent" << endl;
}

void QGVAmplitudeSpectrum::scrollContentsBy(int dx, int dy) {
//    cout << "QGVAmplitudeSpectrum::scrollContentsBy" << endl;

    // Invalidate the necessary parts
    // Ensure the y ticks labels will be redrawn
    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
    QTransform trans = transform();

    QRectF r = QRectF(viewrect.left(), viewrect.top(), 5*14/trans.m11(), viewrect.height());
    m_scene->invalidate(r);

    r = QRectF(viewrect.left(), viewrect.top()+viewrect.height()-14/trans.m22(), viewrect.width(), 14/trans.m22());
    m_scene->invalidate(r);

    viewUpdateTexts();
    setMouseCursorPosition(QPointF(-1,0), false);

    QGraphicsView::scrollContentsBy(dx, dy);
}

void QGVAmplitudeSpectrum::wheelEvent(QWheelEvent* event) {

    qreal numDegrees = (event->angleDelta() / 8).y();
    // Clip to avoid flipping (workaround of a Qt bug ?)
    if(numDegrees>90) numDegrees = 90;
    if(numDegrees<-90) numDegrees = -90;

//    cout << "QGVAmplitudeSpectrum::wheelEvent " << numDegrees << endl;

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    if((viewrect.width()>10.0/gFL->getFs() && numDegrees>0) || (viewrect.height()>10.0/gFL->getFs() && numDegrees<0)) {
        double gx = double(mapToScene(event->pos()).x()-viewrect.left())/viewrect.width();
        double gy = double(mapToScene(event->pos()).y()-viewrect.top())/viewrect.height();
        QRectF newrect = mapToScene(viewport()->rect()).boundingRect();
        newrect.setLeft(newrect.left()+gx*0.01*viewrect.width()*numDegrees);
        newrect.setRight(newrect.right()-(1-gx)*0.01*viewrect.width()*numDegrees);
        if(newrect.width()<10.0/gFL->getFs()){
           newrect.setLeft(newrect.center().x()-5.0/gFL->getFs());
           newrect.setRight(newrect.center().x()+5.0/gFL->getFs());
        }
        newrect.setTop(newrect.top()+gy*0.01*viewrect.height()*numDegrees);
        newrect.setBottom(newrect.bottom()-(1-gy)*0.01*viewrect.height()*numDegrees);
        if(newrect.height()<10.0/gFL->getFs()){
           newrect.setTop(newrect.center().y()-5.0/gFL->getFs());
           newrect.setBottom(newrect.center().y()+5.0/gFL->getFs());
        }

        viewSet(newrect);

        m_aZoomOnSelection->setEnabled(!m_selection.isEmpty());
        m_aZoomOut->setEnabled(true);
        m_aZoomIn->setEnabled(true);
//        m_aUnZoom->setEnabled(true);
    }
}

void QGVAmplitudeSpectrum::mousePressEvent(QMouseEvent* event){
//    std::cout << "QGVWaveform::mousePressEvent" << endl;

    QPointF p = mapToScene(event->pos());
    QRect selview = mapFromScene(m_selection).boundingRect();

    if(event->buttons()&Qt::LeftButton){
        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)) {
                // When moving the spectrum's view
                m_currentAction = CAMoving;
                setMouseCursorPosition(QPointF(-1,0), false);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && m_selection.height()>0 && abs(selview.left()-event->x())<5 && event->y()>=selview.top() && event->y()<=selview.bottom()) {
                // Resize left boundary of the selection
                m_currentAction = CAModifSelectionLeft;
                m_selection_pressedp = QPointF(p.x()-m_selection.left(), 0);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && m_selection.height()>0 && abs(selview.right()-event->x())<5 && event->y()>=selview.top() && event->y()<=selview.bottom()){
                // Resize right boundary of the selection
                m_currentAction = CAModifSelectionRight;
                m_selection_pressedp = QPointF(p.x()-m_selection.right(), 0);
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && m_selection.height()>0 && abs(selview.top()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right()){
                // Resize top boundary of the selection
                m_currentAction = CAModifSelectionTop;
                m_selection_pressedp = QPointF(0, p.y()-m_selection.top());
            }
            else if(!event->modifiers().testFlag(Qt::ControlModifier) && m_selection.width()>0 && m_selection.height()>0 && abs(selview.bottom()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right()){
                // Resize bottom boundary of the selection
                m_currentAction = CAModifSelectionBottom;
                m_selection_pressedp = QPointF(0, p.y()-m_selection.bottom());
            }
            else if((m_selection.width()>0 && m_selection.height()>0) && (event->modifiers().testFlag(Qt::ControlModifier) || (p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom()))){
                // When scroling the selection
                m_currentAction = CAMovingSelection;
                m_selection_pressedp = p;
                m_mouseSelection = m_selection;
                setCursor(Qt::ClosedHandCursor);
        //            gMW->ui->lblSpectrumSelectionTxt->setText(QString("Selection [%1s").arg(m_selection.left()).append(",%1s] ").arg(m_selection.right()).append("%1s").arg(m_selection.width()));
            }
            else {
                // When selecting
                m_currentAction = CASelecting;
                m_selection_pressedp = p;
                m_mouseSelection.setTopLeft(m_selection_pressedp);
                m_mouseSelection.setBottomRight(m_selection_pressedp);
                selectionSet(m_mouseSelection, true);
            }
        }
        else if(gMW->ui->actionEditMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
//                 TODO
            }
            else{
                // When scaling the waveform
                m_currentAction = CAWaveformScale;
                m_selection_pressedp = p;
                gMW->setEditing(gFL->getCurrentFTSound());
                setCursor(Qt::SizeVerCursor);
            }
        }
    }
    else if(event->buttons()&Qt::RightButton) {
        if (event->modifiers().testFlag(Qt::ShiftModifier)) {
            setCursor(Qt::CrossCursor);
            m_currentAction = CAZooming;
            m_selection_pressedp = p;
            m_pressed_mouseinviewport = mapFromScene(p);
            m_pressed_viewrect = mapToScene(viewport()->rect()).boundingRect();
        }
        else {
            QPoint posglobal = mapToGlobal(mapFromScene(p)+QPoint(0,0));
            m_contextmenu.exec(posglobal);
        }
    }

    QGraphicsView::mousePressEvent(event);
//    std::cout << "~QGVWaveform::mousePressEvent " << p.x() << endl;
}

void QGVAmplitudeSpectrum::mouseMoveEvent(QMouseEvent* event){
//    std::cout << "QGVAmplitudeSpectrum::mouseMoveEvent" << endl;

    QPointF p = mapToScene(event->pos());

    setMouseCursorPosition(p, true);

//    std::cout << "QGVWaveform::mouseMoveEvent action=" << m_currentAction << " x=" << p.x() << " y=" << p.y() << endl;

    if(m_currentAction==CAMoving) {
        // When scrolling the view around the scene
        setMouseCursorPosition(QPointF(-1,0), false);
    }
    else if(m_currentAction==CAZooming) {
        double dx = -(event->pos()-m_pressed_mouseinviewport).x()/100.0;
        double dy = (event->pos()-m_pressed_mouseinviewport).y()/100.0;

        QRectF newrect = m_pressed_viewrect;

        newrect.setLeft(m_selection_pressedp.x()-(m_selection_pressedp.x()-m_pressed_viewrect.left())*exp(dx));
        newrect.setRight(m_selection_pressedp.x()+(m_pressed_viewrect.right()-m_selection_pressedp.x())*exp(dx));

        newrect.setTop(m_selection_pressedp.y()-(m_selection_pressedp.y()-m_pressed_viewrect.top())*exp(dy));
        newrect.setBottom(m_selection_pressedp.y()+(m_pressed_viewrect.bottom()-m_selection_pressedp.y())*exp(dy));
        viewSet(newrect);

        viewUpdateTexts();
        setMouseCursorPosition(mapToScene(event->pos()), true);

        m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
    }
    else if(m_currentAction==CAModifSelectionLeft){
        m_mouseSelection.setLeft(p.x()-m_selection_pressedp.x());
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAModifSelectionRight){
        m_mouseSelection.setRight(p.x()-m_selection_pressedp.x());
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAModifSelectionTop){
        m_mouseSelection.setTop(p.y()-m_selection_pressedp.y());
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAModifSelectionBottom){
        m_mouseSelection.setBottom(p.y()-m_selection_pressedp.y());
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAMovingSelection){
        // When scroling the selection
        m_mouseSelection.adjust(p.x()-m_selection_pressedp.x(), p.y()-m_selection_pressedp.y(), p.x()-m_selection_pressedp.x(), p.y()-m_selection_pressedp.y());
        selectionSet(m_mouseSelection, true);
        m_selection_pressedp = p;
    }
    else if(m_currentAction==CASelecting){
        // When selecting
        m_mouseSelection.setTopLeft(m_selection_pressedp);
        m_mouseSelection.setBottomRight(p);
        selectionSet(m_mouseSelection, true);
    }
    else if(m_currentAction==CAWaveformScale){
        // When scaling the waveform
        FTSound* currentftsound = gFL->getCurrentFTSound();
        if(currentftsound){
            if(!currentftsound->m_actionShow->isChecked()) {
                QMessageBox::warning(this, "Editing a hidden file", "<p>The selected file is hidden.<br/><br/>For edition, please select only visible files.</p>");
                gMW->setEditing(NULL);
                m_currentAction = CANothing;
            }
            else {
                currentftsound->m_ampscale *= std::pow(10, -(p.y()-m_selection_pressedp.y())/20.0);

                m_selection_pressedp = p;

                if(currentftsound->m_ampscale>1e10)
                    currentftsound->m_ampscale = 1e10;
                else if(currentftsound->m_ampscale<1e-10)
                    currentftsound->m_ampscale = 1e-10;

                currentftsound->needDFTUpdate();

                currentftsound->setStatus();
                updateDFTs();
                gMW->m_gvWaveform->m_scene->update();
                gFL->fileInfoUpdate();
                gMW->ui->pbSpectrogramSTFTUpdate->show();
                if(gMW->m_gvSpectrogram->m_aAutoUpdate->isChecked())
                    gMW->m_gvSpectrogram->updateSTFTSettings();
            }
        }
    }
    else{
        QRect selview = mapFromScene(m_selection).boundingRect();

        if(gMW->ui->actionSelectionMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
                if(m_selection.width()>0 && m_selection.height()>0 && abs(selview.left()-event->x())<5 && event->y()>=selview.top() && event->y()<=selview.bottom())
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && m_selection.height()>0 && abs(selview.right()-event->x())<5 && selview.top()<=event->y() && selview.bottom()>=event->y())
                    setCursor(Qt::SplitHCursor);
                else if(m_selection.width()>0 && m_selection.height()>0 && abs(selview.top()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right())
                    setCursor(Qt::SplitVCursor);
                else if(m_selection.width()>0 && m_selection.height()>0 && abs(selview.bottom()-event->y())<5 && event->x()>=selview.left() && event->x()<=selview.right())
                    setCursor(Qt::SplitVCursor);
                else if(p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom())
                    setCursor(Qt::OpenHandCursor);
                else
                    setCursor(Qt::CrossCursor);
            }
        }
        else if(gMW->ui->actionEditMode->isChecked()){
            if(event->modifiers().testFlag(Qt::ShiftModifier)){
            }
            else if(event->modifiers().testFlag(Qt::ControlModifier)){
            }
            else{
            }
        }
    }

//    std::cout << "~QGVWaveform::mouseMoveEvent" << endl;
    QGraphicsView::mouseMoveEvent(event);
}

void QGVAmplitudeSpectrum::mouseReleaseEvent(QMouseEvent* event) {
//    std::cout << "QGVAmplitudeSpectrum::mouseReleaseEvent " << endl;

    QPointF p = mapToScene(event->pos());

    m_currentAction = CANothing;

    if(gMW->ui->actionSelectionMode->isChecked()) {
        if(event->modifiers().testFlag(Qt::ShiftModifier)) {
            setCursor(Qt::OpenHandCursor);
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)) {
            setCursor(Qt::OpenHandCursor);
        }
        else{
            if(p.x()>=m_selection.left() && p.x()<=m_selection.right() && p.y()>=m_selection.top() && p.y()<=m_selection.bottom())
                setCursor(Qt::OpenHandCursor);
            else
                setCursor(Qt::CrossCursor);
        }
    }
    else if(gMW->ui->actionEditMode->isChecked()) {
        if(event->modifiers().testFlag(Qt::ShiftModifier)){
        }
        else if(event->modifiers().testFlag(Qt::ControlModifier)) {
        }
        else{
            setCursor(Qt::SizeVerCursor);
        }
    }

    if(abs(m_selection.width())<=0 || abs(m_selection.height())<=0)
        selectionClear();
    else {
        m_aZoomOnSelection->setEnabled(true);
        m_aSelectionClear->setEnabled(true);
    }

    QGraphicsView::mouseReleaseEvent(event);
//    std::cout << "~QGVWaveform::mouseReleaseEvent " << endl;
}

void QGVAmplitudeSpectrum::keyPressEvent(QKeyEvent* event){
//    COUTD << "QGVAmplitudeSpectrum::keyPressEvent " << endl;

    if(event->key()==Qt::Key_Escape) {
        if(!hasSelection()) {
            if(!gMW->m_gvSpectrogram->hasSelection()
                && !gMW->m_gvWaveform->hasSelection())
                gMW->m_gvWaveform->playCursorSet(0.0, true);

            gMW->m_gvWaveform->selectionClear();
        }
        selectionClear();
    }
    if(event->key()==Qt::Key_S)
        selectionZoomOn();

    QGraphicsView::keyPressEvent(event);
}

void QGVAmplitudeSpectrum::selectionClear(bool forwardsync) {
    Q_UNUSED(forwardsync)

    m_giShownSelection->hide();
    m_giSelectionTxt->hide();
    m_selection = QRectF(0, 0, 0, 0);
    m_mouseSelection = QRectF(0, 0, 0, 0);
    m_giShownSelection->setRect(QRectF(0, 0, 0, 0));
    m_aZoomOnSelection->setEnabled(false);
    m_aSelectionClear->setEnabled(false);
    setCursor(Qt::CrossCursor);

    if(gMW->m_gvPhaseSpectrum)
        gMW->m_gvPhaseSpectrum->selectionClear();
    if(gMW->m_gvSpectrumGroupDelay)
        gMW->m_gvSpectrumGroupDelay->selectionClear();

    if(forwardsync){
        if(gMW->m_gvSpectrogram){
            if(gMW->m_gvSpectrogram->m_giShownSelection->isVisible()) {
                QRectF rect = gMW->m_gvSpectrogram->m_mouseSelection;
                if(std::abs(rect.left()-gMW->m_gvSpectrogram->m_scene->sceneRect().left())<std::numeric_limits<double>::epsilon()
                    && std::abs(rect.right()-gMW->m_gvSpectrogram->m_scene->sceneRect().right())<std::numeric_limits<double>::epsilon()){
                    gMW->m_gvSpectrogram->selectionClear();
                }
                else {
                    rect.setTop(0.0);
                    rect.setBottom(gFL->getFs()/2);
                    gMW->m_gvSpectrogram->selectionSet(rect, false);
                }
            }
        }
    }

    selectionSetTextInForm();

    m_scene->update();
}

void QGVAmplitudeSpectrum::selectionSetTextInForm() {

    QString str;

//    cout << "QGVAmplitudeSpectrum::selectionSetText: " << m_selection.height() << " " << gMW->m_gvPhaseSpectrum->m_selection.height() << endl;

    if (m_selection.height()==0 && gMW->m_gvPhaseSpectrum->m_selection.height()==0) {
        str = "No selection";
    }
    else {
        str += QString("");

        double left, right;
        if(m_selection.height()>0) {
            left = m_selection.left();
            right = m_selection.right();
        }
        else {
            left = gMW->m_gvPhaseSpectrum->m_selection.left();
            right = gMW->m_gvPhaseSpectrum->m_selection.right();
        }
        // TODO The line below cannot be avoided exept by reversing the y coordinate of the
        //      whole seen of the spectrogram, and I don't know how to do this :(
        if(std::abs(left)<1e-10) left=0.0;

        str += QString("[%1,%2]%3Hz").arg(left).arg(right).arg(right-left);

        if (gMW->m_gvAmplitudeSpectrum->isVisible() && m_selection.height()>0) {
            str += QString(" x [%4,%5]%6dB").arg(-m_selection.bottom()).arg(-m_selection.top()).arg(m_selection.height());
        }
//        if (gMW->m_gvPhaseSpectrum->isVisible() && gMW->m_gvPhaseSpectrum->m_selection.height()>0) {
//            str += QString(" x [%7,%8]%9rad").arg(-gMW->m_gvPhaseSpectrum->m_selection.bottom()).arg(-gMW->m_gvPhaseSpectrum->m_selection.top()).arg(gMW->m_gvPhaseSpectrum->m_selection.height());
//        }
    }

    gMW->ui->lblSpectrumSelectionTxt->setText(str);
}

void QGVAmplitudeSpectrum::selectionSet(QRectF selection, bool forwardsync) {

    // Order the selection to avoid negative width and negative height
    if(selection.right()<selection.left()){
        float tmp = selection.left();
        selection.setLeft(selection.right());
        selection.setRight(tmp);
    }
    if(selection.top()>selection.bottom()){
        float tmp = selection.top();
        selection.setTop(selection.bottom());
        selection.setBottom(tmp);
    }

    m_selection = m_mouseSelection = selection;

    if(m_selection.left()<0) m_selection.setLeft(0);
    if(m_selection.right()>gFL->getFs()/2.0) m_selection.setRight(gFL->getFs()/2.0);
    if(m_selection.top()<m_scene->sceneRect().top()) m_selection.setTop(m_scene->sceneRect().top());
    if(m_selection.bottom()>m_scene->sceneRect().bottom()) m_selection.setBottom(m_scene->sceneRect().bottom());

//    DEBUGSTRING << "QGVAmplitudeSpectrum::selectionSet " << m_selection << endl;

    m_giShownSelection->setRect(m_selection);
    m_giShownSelection->show();

    m_giSelectionTxt->setText(QString("%1Hz,%2dB").arg(m_selection.width()).arg(m_selection.height()));
//    m_giSelectionTxt->show();
    viewUpdateTexts();

    selectionSetTextInForm();

    m_aZoomOnSelection->setEnabled(m_selection.width()>0 || m_selection.height());
    m_aSelectionClear->setEnabled(m_selection.width()>0 || m_selection.height());

    if(forwardsync) {
        if(gMW->m_gvPhaseSpectrum){
            QRectF rect = gMW->m_gvPhaseSpectrum->m_mouseSelection;
            rect.setLeft(m_mouseSelection.left());
            rect.setRight(m_mouseSelection.right());
            if(rect.height()==0) {
                rect.setTop(gMW->m_gvPhaseSpectrum->m_scene->sceneRect().top());
                rect.setBottom(gMW->m_gvPhaseSpectrum->m_scene->sceneRect().bottom());
            }
            gMW->m_gvPhaseSpectrum->selectionSet(rect, false);
        }

        if(gMW->m_gvSpectrumGroupDelay){
            QRectF rect = gMW->m_gvSpectrumGroupDelay->m_mouseSelection;
            rect.setLeft(m_mouseSelection.left());
            rect.setRight(m_mouseSelection.right());
            if(rect.height()==0) {
                rect.setTop(gMW->m_gvSpectrumGroupDelay->m_scene->sceneRect().top());
                rect.setBottom(gMW->m_gvSpectrumGroupDelay->m_scene->sceneRect().bottom());
            }
            gMW->m_gvSpectrumGroupDelay->selectionSet(rect, false);
        }

        if(gMW->m_gvSpectrogram){
            QRectF rect = gMW->m_gvSpectrogram->m_mouseSelection;
            rect.setTop(gFL->getFs()/2-m_mouseSelection.right());
            rect.setBottom(gFL->getFs()/2-m_mouseSelection.left());
            if(!gMW->m_gvSpectrogram->m_giShownSelection->isVisible()) {
                rect.setLeft(gMW->m_gvSpectrogram->m_scene->sceneRect().left());
                rect.setRight(gMW->m_gvSpectrogram->m_scene->sceneRect().right());
            }
            gMW->m_gvSpectrogram->selectionSet(rect, false);
        }
    }

    selectionSetTextInForm();
}

void QGVAmplitudeSpectrum::viewUpdateTexts() {
    QTransform trans = transform();
    QTransform txttrans;
    txttrans.scale(1.0/trans.m11(), 1.0/trans.m22());

    // Cursor
    m_giCursorPositionXTxt->setTransform(txttrans);
    m_giCursorPositionYTxt->setTransform(txttrans);

    // Selection
    QRectF br = m_giSelectionTxt->boundingRect();
    m_giSelectionTxt->setTransform(txttrans);
    m_giSelectionTxt->setPos(m_selection.center()-QPointF(0.5*br.width()/trans.m11(), 0.5*br.height()/trans.m22()));
}

void QGVAmplitudeSpectrum::selectionZoomOn(){
    if(m_selection.width()>0 && m_selection.height()>0){
        QRectF zoomonrect = m_selection;
        if(gMW->m_dlgSettings->ui->cbViewsAddMarginsOnSelection->isChecked()) {
            zoomonrect.setTop(zoomonrect.top()-0.1*zoomonrect.height());
            zoomonrect.setBottom(zoomonrect.bottom()+0.1*zoomonrect.height());
            zoomonrect.setLeft(zoomonrect.left()-0.1*zoomonrect.width());
            zoomonrect.setRight(zoomonrect.right()+0.1*zoomonrect.width());
        }
        viewSet(zoomonrect);

        viewUpdateTexts();

        if(gMW->m_gvPhaseSpectrum)
            gMW->m_gvPhaseSpectrum->viewUpdateTexts();
        if(gMW->m_gvSpectrumGroupDelay)
            gMW->m_gvSpectrumGroupDelay->viewUpdateTexts();

        setMouseCursorPosition(QPointF(-1,0), false);
//        m_aZoomOnSelection->setEnabled(false);
    }
}

void QGVAmplitudeSpectrum::azoomin(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 *= 1.5;
    h22 *= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    setMouseCursorPosition(QPointF(-1,0), false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void QGVAmplitudeSpectrum::azoomout(){
    QTransform trans = transform();
    qreal h11 = trans.m11();
    qreal h22 = trans.m22();
    h11 /= 1.5;
    h22 /= 1.5;
    setTransformationAnchor(QGraphicsView::AnchorViewCenter);
    setTransform(QTransform(h11, trans.m12(), trans.m21(), h22, 0, 0));
    viewSet();

    setMouseCursorPosition(QPointF(-1,0), false);
    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}
void QGVAmplitudeSpectrum::aunzoom(){

    // Compute max and min among all visible files
    FFTTYPE ymin = std::numeric_limits<FFTTYPE>::infinity();
    FFTTYPE ymax = -std::numeric_limits<FFTTYPE>::infinity();
    for(unsigned int fi=0; fi<gFL->ftsnds.size(); fi++){
        if(!gFL->ftsnds[fi]->isVisible())
            continue;

        ymin = std::min(ymin, gFL->ftsnds[fi]->m_dft_min);
        ymax = std::max(ymax, gFL->ftsnds[fi]->m_dft_max);
    }
    ymin = sigproc::log2db*ymin-3;
    ymax = sigproc::log2db*ymax+3;

    QRectF rect = QRectF(0.0, -ymax, gFL->getFs()/2, (ymax-ymin));

    if(rect.bottom()>(-gMW->ui->sldAmplitudeSpectrumMin->value()))
        rect.setBottom(-gMW->ui->sldAmplitudeSpectrumMin->value());
    if(rect.top()<-ymax)
        rect.setTop(-10);

    viewSet(rect, false);

//    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
//    cout << "QGVAmplitudeSpectrum::aunzoom viewrect: " << viewrect.left() << "," << viewrect.right() << " X " << viewrect.top() << "," << viewrect.bottom() << endl;

    if(gMW->m_gvPhaseSpectrum)
        gMW->m_gvPhaseSpectrum->viewSet(QRectF(0.0, -M_PI, gFL->getFs()/2, 2*M_PI), false);
    if(gMW->m_gvSpectrumGroupDelay)
        gMW->m_gvSpectrumGroupDelay->viewSet(QRectF(0.0, sceneRect().top(), gFL->getFs()/2, sceneRect().height()), false);

//    cursorUpdate(QPointF(-1,0));
//    m_aZoomOnSelection->setEnabled(m_selection.width()>0 && m_selection.height()>0);
}

void QGVAmplitudeSpectrum::setMouseCursorPosition(QPointF p, bool forwardsync) {

    QFontMetrics qfm(gMW->m_dlgSettings->ui->lblGridFontSample->font());

    QLineF line;
    line.setP1(QPointF(p.x(), m_giCursorVert->line().y1()));
    line.setP2(QPointF(p.x(), m_giCursorVert->line().y2()));
    m_giCursorVert->setLine(line);
    line.setP1(QPointF(m_giCursorHoriz->line().x1(), p.y()));
    line.setP2(QPointF(m_giCursorHoriz->line().x2(), p.y()));
    m_giCursorHoriz->setLine(line);

    if(m_giCursorVert->line().x1()==-1){
        m_giCursorHoriz->hide();
        m_giCursorVert->hide();
        m_giCursorPositionXTxt->hide();
        m_giCursorPositionYTxt->hide();
    }
    else {
        QTransform trans = transform();
        QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();
        QTransform txttrans;
        txttrans.scale(1.0/trans.m11(),1.0/trans.m22());

        m_giCursorHoriz->show();
        m_giCursorHoriz->setLine(viewrect.right()-50/trans.m11(), m_giCursorHoriz->line().y1(), gFL->getFs()/2.0, m_giCursorHoriz->line().y1());
        m_giCursorVert->show();
        m_giCursorVert->setLine(m_giCursorVert->line().x1(), viewrect.top(), m_giCursorVert->line().x1(), viewrect.top()+(qfm.height())/trans.m22());
        m_giCursorPositionXTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        m_giCursorPositionYTxt->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
        m_giCursorPositionXTxt->show();
        m_giCursorPositionYTxt->show();

        m_giCursorPositionXTxt->setTransform(txttrans);
        m_giCursorPositionYTxt->setTransform(txttrans);
        QRectF br = m_giCursorPositionXTxt->boundingRect();
        qreal x = m_giCursorVert->line().x1()+1/trans.m11();
        x = min(x, viewrect.right()-br.width()/trans.m11());

        QString freqstr = QString("%1Hz").arg(m_giCursorVert->line().x1());
        if(gMW->m_dlgSettings->ui->cbViewsShowMusicNoteNames->isChecked())
            freqstr += "("+sigproc::h2n(sigproc::f2h(m_giCursorVert->line().x1()))+")";
        m_giCursorPositionXTxt->setText(freqstr);
        m_giCursorPositionXTxt->setPos(x, viewrect.top()-2/trans.m22());

        m_giCursorPositionYTxt->setText(QString("%1dB").arg(-m_giCursorHoriz->line().y1()));
        br = m_giCursorPositionYTxt->boundingRect();
        m_giCursorPositionYTxt->setPos(viewrect.right()-br.width()/trans.m11(), m_giCursorHoriz->line().y1()-br.height()/trans.m22());
    }

    if(forwardsync){
        if(gMW->m_gvSpectrogram)
            gMW->m_gvSpectrogram->setMouseCursorPosition(QPointF(0.0, gFL->getFs()/2-p.x()), false);
        if(gMW->m_gvPhaseSpectrum)
            gMW->m_gvPhaseSpectrum->setMouseCursorPosition(QPointF(p.x(), 0.0), false);
        if(gMW->m_gvSpectrumGroupDelay)
            gMW->m_gvSpectrumGroupDelay->setMouseCursorPosition(QPointF(p.x(), 0.0), false);
    }
}

void QGVAmplitudeSpectrum::drawBackground(QPainter* painter, const QRectF& rect){
//    COUTD << "QGVAmplitudeSpectrum::drawBackground " << rect.left() << " " << rect.right() << " " << rect.top() << " " << rect.bottom() << endl;

    double fs = gFL->getFs();

    // QGraphicsView::drawBackground(painter, rect);// TODO Need this ??

    // Draw grid
    if(m_aAmplitudeSpectrumShowGrid->isChecked())
        draw_grid(painter, rect);

    // Draw the used loudness curve
    if(m_aAmplitudeSpectrumShowLoudnessCurve->isChecked()) {
        QPen outlinePen(QColor(192, 192, 255));
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);
        painter->setOpacity(1);
        int dftlen = 4096;
        std::vector<std::complex<WAVTYPE> > elc(dftlen/2, 0.0);
        for(size_t u=0; u<elc.size(); ++u){
            elc[u] = sigproc::equalloudnesscurvesISO226(fs*double(u)/dftlen, 0);
            elc[u] = -elc[u]/sigproc::log2db;
        }
        draw_spectrum(painter, elc, fs, 1.0, rect);
    }

    // Draw the f0 and its harmonics
    for(size_t fi=0; fi<gFL->ftfzeros.size(); fi++){
        if(!gFL->ftfzeros[fi]->m_actionShow->isChecked()
            || gFL->ftfzeros[fi]->ts.size()==0)
            continue;

//            QPen outlinePen(gMW->ftfzeros[fi]->color);
//            outlinePen.setWidth(0);
//            painter->setPen(outlinePen);
//            painter->setBrush(QBrush(gMW->ftfzeros[fi]->color));

        double ct = 0.0; // The time where the f0 curve has to be sampled
        if(gMW->m_gvWaveform->hasSelection())
            ct = 0.5*(m_trgDFTParameters.nl+m_trgDFTParameters.nr)/fs;
        else
            ct = gMW->m_gvWaveform->getPlayCursorPosition();
        double cf0 = sigproc::nearest<double>(gFL->ftfzeros[fi]->ts, gFL->ftfzeros[fi]->f0s, ct, -1.0);

        if(cf0==-1)
            continue;

        // Draw the f0 vertical line
        QColor c = gFL->ftfzeros[fi]->getColor();
        c.setAlphaF(1.0);
        QPen outlinePen(c);
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);
        painter->drawLine(QLineF(cf0, -3000, cf0, 3000));

        // Update the f0 text
        // TODO Should be moved to setWindowRange (need to move the cf0 computation there too)
        gFL->ftfzeros[fi]->m_aspec_txt->setPos(cf0, 0.0);
        gFL->ftfzeros[fi]->m_aspec_txt->setText(QString("%1Hz").arg(cf0));

        // Draw harmonics up to Nyquist
        c.setAlphaF(0.5);
        outlinePen.setColor(c);
        painter->setPen(outlinePen);
        for(int h=2; h<int(0.5*fs/cf0)+1; h++)
            painter->drawLine(QLineF(h*cf0, -3000, h*cf0, 3000));
    }

    // Draw the spectra
    // TODO should draw spectra only if m_fft is not touching m_dft variables (it doesnt crash ??)
    if (gFL->ftsnds.size()==0) return;
    if (gMW->m_gvWaveform->m_selection.width()==0) return;

    // Draw the filter response
    if(m_filterresponse.size()>0) {
        QPen outlinePen(QColor(255, 192, 192));
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);
        painter->setOpacity(1.0);

        int dftlen = (int(m_filterresponse.size())-1)*2; // The dftlen of the filter response is a fixed one ! It is not the same as the other spectra
        int kmin = std::max(0, int(dftlen*rect.left()/fs));
        int kmax = std::min(dftlen/2, int(1+dftlen*rect.right()/fs));

        double prevx = fs*kmin/dftlen;
        double prevy = m_filterresponse[kmin];
        for(int k=kmin+1; k<=kmax; ++k){
            double x = fs*k/dftlen;
            double y = m_filterresponse[k];
            if(y<-1000000) y=-1000000;
            painter->drawLine(QLineF(prevx, -prevy, x, -y));
            prevx = x;
            prevy = y;
        }
    }

    // Draw the window's frequency response
    if(m_aAmplitudeSpectrumShowWindow->isChecked() && m_windft.size()>0) {
        QPen outlinePen(QColor(192, 192, 192));
        outlinePen.setWidth(0);
        painter->setPen(outlinePen);
        painter->setOpacity(1);

        draw_spectrum(painter, m_windft, fs, 1.0, rect);
    }

    FTSound* currsnd = gFL->getCurrentFTSound(true);

    for(size_t fi=0; fi<gFL->ftsnds.size(); fi++){
        if(!gMW->m_gvWaveform->m_aWaveformShowSelectedWaveformOnTop->isChecked() || gFL->ftsnds[fi]!=currsnd){
            if(gFL->ftsnds[fi]->m_actionShow->isChecked()){
                QPen outlinePen(gFL->ftsnds[fi]->getColor());
                outlinePen.setWidth(0);
                painter->setPen(outlinePen);
                painter->setBrush(QBrush(gFL->ftsnds[fi]->getColor()));
                painter->setOpacity(1);

                draw_spectrum(painter, gFL->ftsnds[fi]->m_dft, fs, 1.0, rect);
            }
        }
    }

    if(currsnd && gMW->m_gvWaveform->m_aWaveformShowSelectedWaveformOnTop->isChecked()){
        if(currsnd->m_actionShow->isChecked()){
            QPen outlinePen(currsnd->getColor());
            outlinePen.setWidth(0);
            painter->setPen(outlinePen);
            painter->setBrush(QBrush(currsnd->getColor()));
            painter->setOpacity(1);

            draw_spectrum(painter, currsnd->m_dft, fs, 1.0, rect);
        }
    }

//    COUTD << "QGVAmplitudeSpectrum::~drawBackground" << endl;
}

void QGVAmplitudeSpectrum::draw_spectrum(QPainter* painter, std::vector<std::complex<WAVTYPE> >& ldft, double fs, double ascale, const QRectF& rect) {
//    COUTD << "QGVAmplitudeSpectrum::draw_spectrum " << ldft.size() << endl;

    int dftlen = (int(ldft.size())-1)*2;
    if (dftlen<2)
        return;

    double lascale = std::log(ascale);
//    cout << "ascale=" << ascale << " lascale=" << lascale << endl;

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    int kmin = std::max(0, int(dftlen*rect.left()/fs));
    int kmax = std::min(dftlen/2, int(1+dftlen*rect.right()/fs));

    // Draw the sound's spectra
    double samppixdensity = (dftlen*(viewrect.right()-viewrect.left())/fs)/viewport()->rect().width();

    if(samppixdensity<=1.0) {
//        COUTD << "Spec: Draw lines between each bin" << endl;

        double prevx = fs*kmin/dftlen;
        std::complex<WAVTYPE>* yp = ldft.data();
        double prevy = sigproc::log2db*(lascale+yp[kmin].real());
        for(int k=kmin+1; k<=kmax; ++k){
            double x = fs*k/dftlen;
            double y = sigproc::log2db*(lascale+yp[k].real());
            if(y<-10*viewrect.bottom()) y=-10*viewrect.bottom();
            painter->drawLine(QLineF(prevx, -prevy, x, -y));
            prevx = x;
            prevy = y;
        }
    }
    else {
//        COUTD << "Spec: Plot only one line per pixel, in order to reduce computation time" << endl;

        painter->setWorldMatrixEnabled(false); // Work in pixel coordinates

        QRect pixrect = mapFromScene(rect).boundingRect();
        QRect fullpixrect = mapFromScene(viewrect).boundingRect();

        double s2p = -(fullpixrect.height()-1)/viewrect.height(); // Scene to pixel
        double p2s = viewrect.width()/fullpixrect.width(); // Pixel to scene
        double yzero = mapFromScene(QPointF(0,0)).y();
        double yinfmin = -viewrect.bottom()/sigproc::log2db; // Nothing seen below this

        std::complex<WAVTYPE>* yp = ldft.data();

        int ns = int(dftlen*(viewrect.left()+pixrect.left()*p2s)/fs);
        for(int i=pixrect.left(); i<=pixrect.right(); i++) {
            int ne = int(dftlen*(viewrect.left()+(i+1)*p2s)/fs);

            if(ns>=0 && ne<int(ldft.size())) {
                WAVTYPE ymin = std::numeric_limits<WAVTYPE>::infinity();
                WAVTYPE ymax = -std::numeric_limits<WAVTYPE>::infinity();
                std::complex<WAVTYPE>* ypp = yp+ns;
                WAVTYPE y;
                for(int n=ns; n<=ne; n++) {
                    y = lascale+ypp->real();
                    if(y<yinfmin) y = yinfmin-10;
                    ymin = std::min(ymin, y);
                    ymax = std::max(ymax, y);
                    ypp++;
                }
                ymin = sigproc::log2db*(ymin);
                ymax = sigproc::log2db*(ymax);
                ymin *= s2p;
                ymax *= s2p;
                ymin = int(ymin-1);
                ymax = int(ymax);
                if(ymin>fullpixrect.height()+1-yzero) ymin=fullpixrect.height()+1-yzero;
                if(ymax>fullpixrect.height()+1-yzero) ymax=fullpixrect.height()+1-yzero;
                painter->drawLine(QLineF(i, yzero+ymin, i, yzero+ymax));
            }

            ns = ne;
        }

        painter->setWorldMatrixEnabled(true); // Go back to scene coordinates
    }
}

void QGVAmplitudeSpectrum::draw_grid(QPainter* painter, const QRectF& rect) {
    // Prepare the pens and fonts
    // TODO put this in the constructor to limit the allocations in this function
    QPen gridPen(QColor(192,192,192)); //192
    gridPen.setWidth(0); // Cosmetic pen (width=1pixel whatever the transform)
    painter->setFont(gMW->m_dlgSettings->ui->lblGridFontSample->font());
    QFontMetrics qfm(gMW->m_dlgSettings->ui->lblGridFontSample->font());

    // Horizontal lines

    QRectF viewrect = mapToScene(viewport()->rect()).boundingRect();

    // Adapt the lines ordinates to the viewport
    double f = log10(float(viewrect.height()));
    int fi;
    if(f<0) fi=int(f-1);
    else fi = int(f);
    double lstep = pow(10.0, fi);
    int m=1;
    while(int(viewrect.height()/lstep)<3){
        lstep /= 2;
        m++;
    }

    // Draw the horizontal lines
    int mn=0;
    painter->setPen(gridPen);
    for(double l=int(viewrect.top()/lstep)*lstep; l<=rect.bottom(); l+=lstep){
//        if(mn%m==0) painter->setPen(gridPen);
//        else        painter->setPen(thinGridPen);
        painter->drawLine(QLineF(rect.left(), l, rect.right(), l));
        mn++;
    }

    // Write the ordinates of the horizontal lines
    painter->setPen(m_gridFontPen);
    QTransform trans = transform();
    for(double l=int(viewrect.top()/lstep)*lstep; l<=rect.bottom(); l+=lstep){
        painter->save();
        painter->translate(QPointF(viewrect.left(), l));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());

        QString txt = QString("%1dB").arg(-l);
        QRectF txtrect = painter->boundingRect(QRectF(), Qt::AlignLeft, txt);
        if(l<viewrect.bottom()-0.75*txtrect.height()/trans.m22())
            painter->drawStaticText(QPointF(0, -0.9*txtrect.height()), QStaticText(txt));
        painter->restore();
    }

    // Vertical lines

    // Adapt the lines absissa to the viewport
    f = std::log10(float(viewrect.width()));
    if(f<0) fi=int(f-1);
    else fi = int(f);
//    std::cout << viewrect.height() << " " << f << " " << fi << endl;
    lstep = pow(10.0, fi);
    m=1;
    while(int(viewrect.width()/lstep)<6){
        lstep /= 2;
        m++;
    }
//    std::cout << "lstep=" << lstep << endl;

    // Draw the vertical lines
    mn = 0;
    painter->setPen(gridPen);
    for(double l=int(viewrect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
//        if(mn%m==0) painter->setPen(gridPen);
//        else        painter->setPen(thinGridPen);
        painter->drawLine(QLineF(l, rect.top(), l, rect.bottom()));
        mn++;
    }

    // Write the absissa of the vertical lines
    painter->setPen(m_gridFontPen);
    for(double l=int(viewrect.left()/lstep)*lstep; l<=rect.right(); l+=lstep){
        painter->save();
        painter->translate(QPointF(l, viewrect.bottom()-(qfm.height()-2)/trans.m22()));
        painter->scale(1.0/trans.m11(), 1.0/trans.m22());
        painter->drawStaticText(QPointF(0, 0), QStaticText(QString("%1Hz").arg(l)));
        painter->restore();
    }
}

QGVAmplitudeSpectrum::~QGVAmplitudeSpectrum(){
    m_fftresizethread->m_mutex_resizing.lock();
    m_fftresizethread->m_mutex_resizing.unlock();
    m_fftresizethread->m_mutex_changingsizes.lock();
    m_fftresizethread->m_mutex_changingsizes.unlock();
    delete m_fftresizethread;
    delete m_fft;
    delete m_dlgSettings;
}
