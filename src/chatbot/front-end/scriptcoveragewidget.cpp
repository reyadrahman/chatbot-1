/*
 * Copyright (C) 2012 Andres Pagliano, Gabriel Miretti, Gonzalo Buteler,
 * Nestor Bustamante, Pablo Perez de Angelis
 *
 * This file is part of LVK Chatbot.
 *
 * LVK Chatbot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * LVK Chatbot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with LVK Chatbot.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "front-end/scriptcoveragewidget.h"
#include "ui_scriptcoveragewidget.h"

#include <QMenu>
#include <QMessageBox>

enum ScriptsTableColumns
{
    ScriptNameCol,
    ScriptCoverageCol,
    ScriptsTableTotalColumns
};

enum ScriptRole
{
    ScriptIdRole = Qt::UserRole
};


const QString HTML_SCRIPT_START     = "<html><header><style></style></head><body>";
const QString HTML_SCRIPT_LINE      = "<a href=\"%6,%7\" style=\"%1\"><b>%2:</b> %3<br/>"
                                       "<b>%4:</b> %5</a><br/>";
const QString HTML_SCRIPT_LINE_OK   = HTML_SCRIPT_LINE.arg("text-decoration:none;color:#000000");
const QString HTML_SCRIPT_LINE_ERR1 = HTML_SCRIPT_LINE.arg("text-decoration:none;color:#772b77");
const QString HTML_SCRIPT_LINE_ERR2 = HTML_SCRIPT_LINE.arg("text-decoration:none;color:#ff0000");
const QString HTML_SCRIPT_END       = "</body></html>";

//--------------------------------------------------------------------------------------------------
// Helpers
//--------------------------------------------------------------------------------------------------

namespace
{

// coverage string format
inline QString covFormat(float cov)
{
    return  QString::number((int)cov) + "%";
}

} // namespace


//--------------------------------------------------------------------------------------------------
// ScriptCoverageWidget
//--------------------------------------------------------------------------------------------------

Lvk::FE::ScriptCoverageWidget::ScriptCoverageWidget(QWidget *parent)
    : QWidget(parent),
      ui(new Ui::ScriptCoverageWidget),
      m_detective(tr("Detective")),
      m_root(0),
      m_collapseDef(false), // Collapse rule defintion column when it is not used
      m_categoryVisible(false)
{
    ui->setupUi(this);

    m_sizes = QList<int>() << (width()*2/10) << (width()*4/10) << (width()*4/10);

    ui->splitter->setSizes(m_sizes);

    clear();
    setupTables();
    connectSignals();
}

//--------------------------------------------------------------------------------------------------

Lvk::FE::ScriptCoverageWidget::~ScriptCoverageWidget()
{
    delete ui;
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::setupTables()
{
    // Date-Contact table
    ui->scriptsTable->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->scriptsTable->setRowCount(0);
    ui->scriptsTable->setColumnCount(ScriptsTableTotalColumns);
    ui->scriptsTable->setSortingEnabled(true);
    ui->scriptsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->scriptsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->scriptsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->scriptsTable->setAlternatingRowColors(true);
    ui->scriptsTable->horizontalHeader()->setStretchLastSection(true);
    ui->scriptsTable->verticalHeader()->hide();
    ui->scriptsTable->setColumnWidth(ScriptNameCol, 120);
    ui->scriptsTable->setHorizontalHeaderLabels(QStringList()
                                                << tr("File")
                                                << tr("Coverage"));
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::connectSignals()
{
    connect(ui->scriptsTable,
            SIGNAL(customContextMenuRequested(QPoint)),
            SLOT(onCustomMenu(QPoint)));

    connect(ui->scriptsTable->selectionModel(),
            SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
            SLOT(onScriptRowChanged(QModelIndex,QModelIndex)));

    connect(ui->scriptView,
            SIGNAL(anchorClicked(QUrl)),
            SLOT(onAnchorClicked(QUrl)));

    connect(ui->showRuleDefButton,
            SIGNAL(clicked()),
            SLOT(onShowRuleDefClicked()));

    connect(ui->splitter,
            SIGNAL(splitterMoved(int, int)),
            SLOT(onSplitterMoved(int, int)));
}

//--------------------------------------------------------------------------------------------------

QList<int> Lvk::FE::ScriptCoverageWidget::splitterSizes() const
{
    return m_collapseDef ? m_sizes : ui->splitter->sizes();
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::setSplitterSizes(const QList<int> &sizes)
{
    if (m_collapseDef) {
        m_sizes = sizes;
    } else {
        ui->splitter->setSizes(sizes);
    }

    showRuleUsedColumn(ui->ruleGroupBox->isVisible());
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::onSplitterMoved(int, int)
{
    m_sizes[0] = ui->splitter->sizes()[0];
    m_sizes[1] = ui->splitter->sizes()[1];

    if (ui->splitter->sizes()[2] > 0) {
        m_sizes[2] = ui->splitter->sizes()[2];
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::onCustomMenu(const QPoint &pos)
{
    QTableWidgetItem *item = ui->scriptsTable->itemAt(pos);

    if (item) {
        QPoint gpos = ui->scriptsTable->viewport()->mapToGlobal(pos);

        QMenu menu;
        menu.addAction(tr("Remove"));

        QAction* selItem = menu.exec(gpos);
        if (selItem) {
            QString filename = ui->scriptsTable->item(item->row(), ScriptNameCol)->text();
            QString title = tr("Remove script");
            QString msg = tr("Are you sure you want to remove the script '%1'?").arg(filename);

            int selButton = QMessageBox::question(this, title, msg, QMessageBox::Ok,
                                                  QMessageBox::Cancel);

            if (selButton == QMessageBox::Ok) {
                ui->scriptsTable->removeRow(item->row());
                emit scriptRemoved(filename);
            }
        }
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::setAnalyzedScripts(const Clue::AnalyzedList &scripts,
                                                       const Lvk::BE::Rule *root)
{
    clear();

    m_scripts = scripts;
    m_root = root; // CHECK deep copy? !!!

    float globalCov = 0.0;
    int i = 0;

    ui->scriptsTable->setSortingEnabled(false);

    foreach (const Clue::AnalyzedScript &s, scripts) {
        addScriptRow(i++, s.filename, s.coverage);
        globalCov += s.coverage;
    }

    ui->scriptsTable->setSortingEnabled(true);

    if (scripts.size() > 0) {
        globalCov /= scripts.size();
        ui->coverageLabel->setText(tr("Global coverage: ") + covFormat(globalCov));
    } else {
        ui->coverageLabel->clear();
    }
}

//--------------------------------------------------------------------------------------------------

int Lvk::FE::ScriptCoverageWidget::currentScript()
{
    return ui->scriptsTable->currentRow();
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::setCurrentScript(int i)
{
    ui->scriptsTable->setCurrentCell(i, 0);
}

void Lvk::FE::ScriptCoverageWidget::setCategoryVisible(bool visible)
{
    m_categoryVisible = visible;

    if (!ui->categoryLabel->text().isEmpty()) {
        ui->categoryGroupBox->setVisible(visible);
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::addScriptRow(int i, const QString &filename, float coverage)
{
    int r = ui->scriptsTable->rowCount();
    ui->scriptsTable->insertRow(r);
    ui->scriptsTable->setItem(r, ScriptNameCol,     new QTableWidgetItem(filename));
    ui->scriptsTable->setItem(r, ScriptCoverageCol, new QTableWidgetItem(covFormat(coverage)));
    ui->scriptsTable->item(r, ScriptNameCol)->setData(ScriptIdRole, QVariant(i));
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::clear()
{
    m_root = 0;
    m_scripts.clear();

    ui->scriptsTable->clearContents();
    ui->scriptsTable->setRowCount(0);
    ui->coverageLabel->clear();
    ui->ruleView->clear();
    ui->scriptView->setHtml(tr("(No script selected)"));
    ui->categoryLabel->clear();
    ui->categoryGroupBox->setVisible(false);

    showRuleUsedColumn(false);
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::onScriptRowChanged(const QModelIndex &current,
                                                       const QModelIndex &/*previous*/)
{
    QTableWidgetItem *item = ui->scriptsTable->item(current.row(), ScriptNameCol);

    if (item) {
        showScript(item->data(ScriptIdRole).toInt());
    } else {
        ui->scriptView->setHtml(tr("(No script selected)"));
    }

    showRuleUsedColumn(false);
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::onAnchorClicked(const QUrl &url)
{
   QStringList indexes = url.toString().split(",");
   int i = indexes[0].toUInt();
   int j = indexes[1].toUInt();

   showRuleUsed(i, j);
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::onShowRuleDefClicked()
{
    emit showRule(ui->ruleView->ruleId());
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::showScript(int i)
{
    qDebug() << "ScriptCoverageWidget: Showing script #" << i;

    if (i < 0 || i >= m_scripts.size()) {
        ui->scriptView->setHtml(tr("Error: Wrong script index"));
        return;
    }
    if (m_scripts[i].isEmpty()) {
        ui->scriptView->setHtml(tr("(Empty script)"));
        return;
    }

    QString html = HTML_SCRIPT_START;
    QString linePattern;

    for (int j = 0; j < m_scripts[i].size(); ++j) {
        const Clue::AnalyzedLine &line = m_scripts[i][j];
        const QString &character = m_scripts[i].character;

        if (line.outputIdx != -1) {
            linePattern = HTML_SCRIPT_LINE_OK;
        } else if (line.ruleId != 0) {
            linePattern = HTML_SCRIPT_LINE_ERR1;
        } else {
            linePattern = HTML_SCRIPT_LINE_ERR2;
        }

        html += linePattern.arg(m_detective, line.question, character, line.answer).arg(i).arg(j);
    }

    html += HTML_SCRIPT_END;

    ui->scriptView->setHtml(html);
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::showRuleUsed(int i, int j)
{
    if (i >= m_scripts.size() || j >= m_scripts[i].size()) {
        qCritical() << "ScriptCoverageWidget: Invalid indices" << i << j;
        return;
    }

    Clue::AnalyzedLine line = m_scripts[i][j];

    const BE::Rule *rule = findRule(line.ruleId);

    ui->ruleView->setRule(rule, line.inputIdx);

    showRuleUsedColumn(true);
    showCurrentCategory(line.topic);

    switch (line.status) {
    case Clue::NoAnswerFound:
    case Clue::MismatchExpectedAnswer:
        showHint(line.expHint);
        break;
    case Clue::MatchForbiddenAnswer:
        showHint(line.forbidHint);
        break;
    case Clue::AnswerOk:
    case Clue::NotAnalyzed:
    default:
        showHint("");
        break;
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::showHint(const QString &hint)
{
    if (!hint.isEmpty()) {
        ui->hintLabel->setText(tr("Hint: ") + hint);
        ui->lightBulb->setVisible(true);
    } else {
        ui->hintLabel->clear();
        ui->lightBulb->setVisible(false);
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::showRuleUsedColumn(bool show)
{
    if (m_collapseDef) {
        ui->splitter->setSizes(show ? m_sizes : m_sizes.mid(0, 2));
    }

    ui->ruleGroupBox->setVisible(show);
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::ScriptCoverageWidget::showCurrentCategory(const QString topic)
{
    // TODO refactor duplicated code from MainWindow
    if (m_categoryVisible) {
        quint64 catId = topic.toULongLong(); // FIXME breaking AppFacade encapsulation!
        const BE::Rule *category = findRule(catId);
        ui->categoryLabel->setText(category ? category->name() : tr("(none)"));
        ui->categoryGroupBox->setVisible(true);
    } else {
        ui->categoryLabel->clear();
        ui->categoryGroupBox->setVisible(false);
    }
}

//--------------------------------------------------------------------------------------------------

const Lvk::BE::Rule *Lvk::FE::ScriptCoverageWidget::findRule(quint64 ruleId)
{
    if (!m_root) {
        return 0;
    }

    for (BE::Rule::const_iterator it = m_root->begin(); it != m_root->end(); ++it) {
        if (ruleId != 0) {
            if ((*it)->id() == ruleId) {
                return *it;
            }
        } else {
            if ((*it)->type() == BE::Rule::EvasiveRule) {
                return *it;
            }
        }
    }

    return 0;
}

