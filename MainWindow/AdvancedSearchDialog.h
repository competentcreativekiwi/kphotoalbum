// SPDX-FileCopyrightText: 2024 KPhotoAlbum Contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef ADVANCEDSEARCHDIALOG_H
#define ADVANCEDSEARCHDIALOG_H

#include <kpabase/FileNameList.h>

#include <QDialog>
#include <QMap>

class QLineEdit;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QTimer;

namespace MainWindow
{

/**
 * @brief Non-modal dialog providing include/exclude text search and per-tag
 * include/exclude/neutral filtering. Emits searchResultsChanged() on every
 * input change (live filtering).
 *
 * Tag state per item:
 *   0  = neutral  (ignore this tag)
 *  +1  = include  (image must have this tag)
 *  -1  = exclude  (image must NOT have this tag)
 *
 * Logic:
 *   - All include-text words must match (AND).
 *   - Any exclude-text word causes rejection (OR exclusion).
 *   - All +tags must be present (AND, even within the same category).
 *   - All -tags must be absent (AND).
 *   - Exclude beats include on conflict.
 */
class AdvancedSearchDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AdvancedSearchDialog(QWidget *parent = nullptr);

Q_SIGNALS:
    void searchResultsChanged(const DB::FileNameList &files);
    void dialogClosed();

protected:
    void closeEvent(QCloseEvent *event) override;

private Q_SLOTS:
    void onSearchChanged();
    void onTagItemClicked(QTreeWidgetItem *item, int column);
    void onRestoreLastSearch();
    void onClearAll();

private:
    void buildTagTree();
    DB::FileNameList computeResults() const;

    // Cycle neutral → include → exclude → neutral and refresh display
    void cycleTagState(QTreeWidgetItem *item);
    void applyTagStateAppearance(QTreeWidgetItem *item);

    // Persist / restore the full dialog state for "Restore Last Search"
    struct SearchState {
        QString includeText;
        QString excludeText;
        // category name → (tag name → state)
        QMap<QString, QMap<QString, int>> tagStates;
    };
    void saveCurrentState();
    void applyState(const SearchState &state);

    QLineEdit *m_includeEdit;
    QLineEdit *m_excludeEdit;
    QTreeWidget *m_tagTree;
    QPushButton *m_restoreButton;
    QPushButton *m_clearButton;
    QTimer *m_debounceTimer;

    SearchState m_lastSearch;
    bool m_hasLastSearch = false;

    // Prevent recursive onSearchChanged calls while applying a saved state
    bool m_applyingState = false;
};

} // namespace MainWindow

#endif // ADVANCEDSEARCHDIALOG_H

// vi:expandtab:tabstop=4 shiftwidth=4:
