// SPDX-FileCopyrightText: 2024 KPhotoAlbum Contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef GIFDISPLAY_H
#define GIFDISPLAY_H

#include "AbstractDisplay.h"

#include <QLabel>
#include <QMovie>
#include <QSize>

namespace Viewer
{

class GifDisplay : public AbstractDisplay
{
    Q_OBJECT

public:
    explicit GifDisplay(QWidget *parent = nullptr);

public Q_SLOTS:
    void stop() override;
    void rotate(const DB::ImageInfoPtr &info) override;

protected:
    bool setImageImpl(DB::ImageInfoPtr info, bool forward) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void updateScaledSize();

    QLabel *m_label;
    QMovie *m_movie;
    QSize m_nativeSize;
};

} // namespace Viewer

#endif // GIFDISPLAY_H

// vi:expandtab:tabstop=4 shiftwidth=4:
