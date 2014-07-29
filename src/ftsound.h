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

#ifndef FTSOUND_H
#define FTSOUND_H

#include <deque>
#include <vector>
#include <complex>

#include <QString>
#include <QColor>
#include <QAudioFormat>
#include <QAction>

#include "filetype.h"

#define WAVTYPE double

class CFFTW3;

class FTSound : public QIODevice, public FileType
{
    Q_OBJECT

    void load(const QString& _fileName);

    QAudioFormat m_outputaudioformat; // Temporary copy for readData

    void setSamplingRate(double _fs); // Used by implementations of load

public:
    FTSound(const QString& _fileName, QObject* parent);
    static QString getAudioFileReadingDescription();

    std::vector<WAVTYPE> wav;
    std::vector<WAVTYPE> wavfiltered;
    std::vector<WAVTYPE>* wavtoplay;
    WAVTYPE m_wavmaxamp;
    double fs; // [Hz] Sampling frequency of this specific wav file
    static double fs_common;  // [Hz] Sampling frequency of the sound player
    static WAVTYPE s_play_power;
    static std::deque<WAVTYPE> s_play_power_values;
    double getDuration() const {return wav.size()/fs;}
    virtual double getLastSampleTime() const;
    virtual void fillContextMenu(QMenu& contextmenu, WMainWindow* mainwindow);

    // Spectrum
    std::vector<std::complex<WAVTYPE> > m_dft;

    // QIODevice
    double setPlay(const QAudioFormat& format, double tstart=0.0, double tstop=0.0, double fstart=0.0, double fstop=0.0);
    void stop();
    qint64 readData(char *data, qint64 maxlen);
    qint64 writeData(const char *data, qint64 len);
//    qint64 bytesAvailable() const;
    qint64 m_start; // [sample index]
    qint64 m_pos;   // [sample index]
    qint64 m_end;   // [sample index]

    qreal m_ampscale; // [linear]
    qint64 m_delay;   // [sample index]

    // Visualization
    QAction* m_actionInvPolarity;
    QAction* m_actionResetAmpScale;
    QAction* m_actionResetDelay;

    ~FTSound();
};

#endif // FTSOUND_H