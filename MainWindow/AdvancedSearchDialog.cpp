// SPDX-FileCopyrightText: 2024 KPhotoAlbum Contributors
//
// SPDX-License-Identifier: GPL-2.0-or-later

#include "AdvancedSearchDialog.h"

#include <DB/CategoryCollection.h>
#include <DB/ImageDB.h>
#include <DB/ImageInfoList.h>
#include <DB/search/ImageSearchInfo.h>
#include <kpabase/FileNameList.h>

#include <KLocalizedString>
#include <QApplication>
#include <QCloseEvent>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

// Qt::UserRole stores the tag state: 0=neutral, 1=include, -1=exclude
static const int StateRole = Qt::UserRole;

MainWindow::AdvancedSearchDialog::AdvancedSearchDialog(QWidget *parent)
    : QDialog(parent)
    , m_debounceTimer(new QTimer(this))
{
    setWindowTitle(i18nc("@title:window", "Advanced Search"));
    setMinimumSize(400, 550);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(8);

    // ── Include text ──────────────────────────────────────────────────────────
    auto *includeGroup = new QGroupBox(i18nc("@title:group", "Include (all words must match)"), this);
    auto *includeLayout = new QVBoxLayout(includeGroup);
    m_includeEdit = new QLineEdit(this);
    m_includeEdit->setPlaceholderText(i18nc("@label:textbox", "Type words to include…"));
    m_includeEdit->setClearButtonEnabled(true);
    includeLayout->addWidget(m_includeEdit);
    root->addWidget(includeGroup);

    // ── Exclude text ──────────────────────────────────────────────────────────
    auto *excludeGroup = new QGroupBox(i18nc("@title:group", "Exclude (any word = excluded)"), this);
    auto *excludeLayout = new QVBoxLayout(excludeGroup);
    m_excludeEdit = new QLineEdit(this);
    m_excludeEdit->setPlaceholderText(i18nc("@label:textbox", "Type words to exclude…"));
    m_excludeEdit->setClearButtonEnabled(true);
    excludeLayout->addWidget(m_excludeEdit);
    root->addWidget(excludeGroup);

    // ── Tag tree ──────────────────────────────────────────────────────────────
    auto *tagGroup = new QGroupBox(i18nc("@title:group", "Tags"), this);
    auto *tagLayout = new QVBoxLayout(tagGroup);

    auto *legend = new QLabel(
        i18nc("@info legend for tag states",
              "<b>+</b> = must have tag &nbsp;&nbsp; <b>−</b> = must not have tag &nbsp;&nbsp; <i>(blank)</i> = ignore"),
        this);
    legend->setTextFormat(Qt::RichText);
    tagLayout->addWidget(legend);

    m_tagTree = new QTreeWidget(this);
    m_tagTree->setColumnCount(2);
    m_tagTree->setHeaderLabels({ i18nc("@title:column tag state", "State"),
                                 i18nc("@title:column tag name", "Tag") });
    m_tagTree->setColumnWidth(0, 50);
    m_tagTree->setRootIsDecorated(true);
    m_tagTree->setSortingEnabled(false);
    m_tagTree->setAlternatingRowColors(true);
    tagLayout->addWidget(m_tagTree);
    root->addWidget(tagGroup, 1); // stretch tag group

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;
    m_restoreButton = new QPushButton(i18nc("@action:button", "Restore Last Search"), this);
    m_restoreButton->setEnabled(false);
    m_clearButton = new QPushButton(i18nc("@action:button", "Clear All"), this);
    btnRow->addWidget(m_restoreButton);
    btnRow->addStretch();
    btnRow->addWidget(m_clearButton);
    root->addLayout(btnRow);

    // ── Connections ───────────────────────────────────────────────────────────
    m_debounceTimer->setSingleShot(true);
    m_debounceTimer->setInterval(150); // ms — avoid querying DB on every keystroke

    connect(m_includeEdit, &QLineEdit::textChanged, m_debounceTimer, qOverload<>(&QTimer::start));
    connect(m_excludeEdit, &QLineEdit::textChanged, m_debounceTimer, qOverload<>(&QTimer::start));
    connect(m_debounceTimer, &QTimer::timeout, this, &AdvancedSearchDialog::onSearchChanged);
    connect(m_tagTree, &QTreeWidget::itemClicked, this, &AdvancedSearchDialog::onTagItemClicked);
    connect(m_restoreButton, &QPushButton::clicked, this, &AdvancedSearchDialog::onRestoreLastSearch);
    connect(m_clearButton, &QPushButton::clicked, this, &AdvancedSearchDialog::onClearAll);

    buildTagTree();
}

// ── Tag tree ──────────────────────────────────────────────────────────────────

void MainWindow::AdvancedSearchDialog::buildTagTree()
{
    m_tagTree->clear();

    const auto categories = DB::ImageDB::instance()->categoryCollection()->categories();
    for (const auto &category : categories) {
        if (category->isSpecialCategory())
            continue;

        auto *catItem = new QTreeWidgetItem(m_tagTree);
        catItem->setText(1, category->name());
        QFont bold = catItem->font(1);
        bold.setBold(true);
        catItem->setFont(1, bold);
        catItem->setFlags(Qt::ItemIsEnabled); // category rows are not clickable for state
        catItem->setExpanded(true);

        const QStringList tags = category->itemsInclCategories();
        for (const QString &tag : tags) {
            auto *tagItem = new QTreeWidgetItem(catItem);
            tagItem->setData(0, StateRole, 0); // neutral
            tagItem->setText(1, tag);
            tagItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            applyTagStateAppearance(tagItem);
        }
    }
}

void MainWindow::AdvancedSearchDialog::cycleTagState(QTreeWidgetItem *item)
{
    const int current = item->data(0, StateRole).toInt();
    // neutral(0) → include(+1) → exclude(-1) → neutral(0)
    const int next = (current == 0) ? 1 : (current == 1) ? -1 : 0;
    item->setData(0, StateRole, next);
    applyTagStateAppearance(item);
}

void MainWindow::AdvancedSearchDialog::applyTagStateAppearance(QTreeWidgetItem *item)
{
    const int state = item->data(0, StateRole).toInt();
    if (state == 1) {
        item->setText(0, QStringLiteral("+"));
        item->setForeground(0, QColor(Qt::darkGreen));
        item->setForeground(1, QColor(Qt::darkGreen));
    } else if (state == -1) {
        item->setText(0, QStringLiteral("−"));
        item->setForeground(0, QColor(Qt::red));
        item->setForeground(1, QColor(Qt::red));
    } else {
        item->setText(0, QString());
        item->setForeground(0, QApplication::palette().color(QPalette::Text));
        item->setForeground(1, QApplication::palette().color(QPalette::Text));
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::AdvancedSearchDialog::onTagItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    // Ignore category header rows (they have no StateRole)
    if (!item->parent())
        return;
    cycleTagState(item);
    onSearchChanged();
}

void MainWindow::AdvancedSearchDialog::onSearchChanged()
{
    if (m_applyingState)
        return;
    Q_EMIT searchResultsChanged(computeResults());
}

void MainWindow::AdvancedSearchDialog::onRestoreLastSearch()
{
    if (!m_hasLastSearch)
        return;
    applyState(m_lastSearch);
}

void MainWindow::AdvancedSearchDialog::onClearAll()
{
    m_applyingState = true;
    m_includeEdit->clear();
    m_excludeEdit->clear();

    // Reset all tag states to neutral
    for (int i = 0; i < m_tagTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *catItem = m_tagTree->topLevelItem(i);
        for (int j = 0; j < catItem->childCount(); ++j) {
            QTreeWidgetItem *tagItem = catItem->child(j);
            tagItem->setData(0, StateRole, 0);
            applyTagStateAppearance(tagItem);
        }
    }
    m_applyingState = false;
    onSearchChanged();
}

// ── State save / restore ──────────────────────────────────────────────────────

void MainWindow::AdvancedSearchDialog::saveCurrentState()
{
    m_lastSearch.includeText = m_includeEdit->text();
    m_lastSearch.excludeText = m_excludeEdit->text();
    m_lastSearch.tagStates.clear();

    for (int i = 0; i < m_tagTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *catItem = m_tagTree->topLevelItem(i);
        const QString category = catItem->text(1);
        for (int j = 0; j < catItem->childCount(); ++j) {
            QTreeWidgetItem *tagItem = catItem->child(j);
            const int state = tagItem->data(0, StateRole).toInt();
            if (state != 0)
                m_lastSearch.tagStates[category][tagItem->text(1)] = state;
        }
    }
    m_hasLastSearch = true;
    m_restoreButton->setEnabled(true);
}

void MainWindow::AdvancedSearchDialog::applyState(const SearchState &state)
{
    m_applyingState = true;
    m_includeEdit->setText(state.includeText);
    m_excludeEdit->setText(state.excludeText);

    for (int i = 0; i < m_tagTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *catItem = m_tagTree->topLevelItem(i);
        const QString category = catItem->text(1);
        for (int j = 0; j < catItem->childCount(); ++j) {
            QTreeWidgetItem *tagItem = catItem->child(j);
            const QString tag = tagItem->text(1);
            const int s = state.tagStates.value(category).value(tag, 0);
            tagItem->setData(0, StateRole, s);
            applyTagStateAppearance(tagItem);
        }
    }
    m_applyingState = false;
    onSearchChanged();
}

// ── Core search computation ───────────────────────────────────────────────────

DB::FileNameList MainWindow::AdvancedSearchDialog::computeResults() const
{
    // ── 1. Tag filter via ImageSearchInfo ─────────────────────────────────────
    DB::ImageSearchInfo tagInfo;
    bool hasTagConditions = false;

    for (int i = 0; i < m_tagTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *catItem = m_tagTree->topLevelItem(i);
        const QString category = catItem->text(1);

        QStringList parts;
        for (int j = 0; j < catItem->childCount(); ++j) {
            QTreeWidgetItem *tagItem = catItem->child(j);
            const int state = tagItem->data(0, StateRole).toInt();
            if (state == 1)
                parts << tagItem->text(1);
            else if (state == -1)
                parts << (QLatin1Char('!') + tagItem->text(1));
        }
        if (!parts.isEmpty()) {
            tagInfo.setCategoryMatchText(category, parts.join(QStringLiteral(" & ")));
            hasTagConditions = true;
        }
    }

    // Get initial set — all images if no tag conditions
    DB::FileNameList results;
    if (hasTagConditions) {
        results = DB::ImageDB::instance()->search(tagInfo).files();
    } else {
        results = DB::ImageDB::instance()->search(DB::ImageSearchInfo()).files();
    }

    // ── 2. Include text (each word must match — AND) ───────────────────────────
    const QStringList includeWords = m_includeEdit->text().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString &word : includeWords) {
        DB::ImageSearchInfo wordInfo;
        wordInfo.setFreeformMatchText(QRegularExpression::escape(word));
        const DB::FileNameList wordMatches = DB::ImageDB::instance()->search(wordInfo).files();
        const QSet<DB::FileName> wordSet(wordMatches.begin(), wordMatches.end());

        DB::FileNameList filtered;
        filtered.reserve(results.size());
        for (const DB::FileName &f : qAsConst(results)) {
            if (wordSet.contains(f))
                filtered << f;
        }
        results = filtered;
    }

    // ── 3. Exclude text (any word = exclude — OR exclusion) ───────────────────
    const QString excludeText = m_excludeEdit->text().trimmed();
    if (!excludeText.isEmpty()) {
        QStringList excludeWords = excludeText.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        QStringList escaped;
        escaped.reserve(excludeWords.size());
        for (const QString &w : qAsConst(excludeWords))
            escaped << QRegularExpression::escape(w);

        DB::ImageSearchInfo excludeInfo;
        excludeInfo.setFreeformMatchText(escaped.join(QLatin1Char('|')));
        const DB::FileNameList excludeMatches = DB::ImageDB::instance()->search(excludeInfo).files();
        const QSet<DB::FileName> excludeSet(excludeMatches.begin(), excludeMatches.end());

        DB::FileNameList filtered;
        filtered.reserve(results.size());
        for (const DB::FileName &f : qAsConst(results)) {
            if (!excludeSet.contains(f))
                filtered << f;
        }
        results = filtered;
    }

    return results;
}

// ── Dialog lifecycle ──────────────────────────────────────────────────────────

void MainWindow::AdvancedSearchDialog::closeEvent(QCloseEvent *event)
{
    saveCurrentState();
    Q_EMIT dialogClosed();
    QDialog::closeEvent(event);
}

#include "moc_AdvancedSearchDialog.cpp"

// vi:expandtab:tabstop=4 shiftwidth=4:
