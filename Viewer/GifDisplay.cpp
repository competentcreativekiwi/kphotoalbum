// SPDX-FileCopyrightText: 2024 KPhotoAlbum Contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "GifDisplay.h"

#include <DB/ImageInfo.h>

#include <QResizeEvent>
#include <QSize>
#include <QVBoxLayout>

Viewer::GifDisplay::GifDisplay(QWidget *parent)
    : AbstractDisplay(parent)
    , m_label(new QLabel(this))
    , m_movie(nullptr)
    , m_nativeSize()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);

    m_label->setAlignment(Qt::AlignCenter);
    m_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    setStyleSheet(QStringLiteral("background: black"));
}

bool Viewer::GifDisplay::setImageImpl(DB::ImageInfoPtr info, bool /*forward*/)
{
    stop();

    const QString path = info->fileName().absolute();
    m_movie = new QMovie(path, QByteArray(), this);

    if (!m_movie->isValid()) {
        delete m_movie;
        m_movie = nullptr;
        return false;
    }

    // Jump to first frame to read the native dimensions before scaling
    m_movie->jumpToFrame(0);
    m_nativeSize = m_movie->currentPixmap().size();
    updateScaledSize();

    m_label->setMovie(m_movie);
    m_movie->start();
    return true;
}

void Viewer::GifDisplay::updateScaledSize()
{
    if (!m_movie || m_nativeSize.isEmpty())
        return;
    m_movie->setScaledSize(m_nativeSize.scaled(m_label->size(), Qt::KeepAspectRatio));
}

void Viewer::GifDisplay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateScaledSize();
}

void Viewer::GifDisplay::stop()
{
    if (m_movie) {
        m_movie->stop();
        m_label->setMovie(nullptr);
        delete m_movie;
        m_movie = nullptr;
    }
}

void Viewer::GifDisplay::rotate(const DB::ImageInfoPtr & /*info*/)
{
    // Rotation of animated GIFs is not supported
}

#include "moc_GifDisplay.cpp"

// vi:expandtab:tabstop=4 shiftwidth=4:
