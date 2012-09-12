#include "welcomewidget.h"
#include "ui_welcomewidget.h"
#include "front-end/filefilters.h"
#include "front-end/mainwindow.h"
#include "common/settings.h"
#include "common/settingskeys.h"

#include <QtDebug>
#include <QFileDialog>
#include <QStyle>
#include <QDesktopWidget>

//--------------------------------------------------------------------------------------------------
// WelcomeWidget
//--------------------------------------------------------------------------------------------------

Lvk::FE::WelcomeWidget::WelcomeWidget(QWidget *parent) :
    QWidget(parent), ui(new Ui::WelcomeWidget), m_mw(new MainWindow())
{
    ui->setupUi(this);

    connect(ui->createChatbotButton,   SIGNAL(clicked()), SLOT(onCreateChatbot()));
    connect(ui->openChatbotButton,     SIGNAL(clicked()), SLOT(onOpenChatbot()));
    connect(ui->openLastChatbotButton, SIGNAL(clicked()), SLOT(onOpenLastChatbot()));

    Lvk::Cmn::Settings settings;
    m_lastFilename = settings.value(SETTING_LAST_FILE, QString()).toString();

    if (m_lastFilename.isEmpty()) {
        ui->openLastChatbotButton->setVisible(false);
    }

    setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, size(),
                                    qApp->desktop()->availableGeometry()));
}

//--------------------------------------------------------------------------------------------------

Lvk::FE::WelcomeWidget::~WelcomeWidget()
{
    delete ui;
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::WelcomeWidget::onOpenChatbot()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Open File"), "",
                                                    FileFilters::chatbotFilter());
    if (!filename.isEmpty()) {
        m_mw->openFile(filename);
        m_mw->show();
        close();
    }
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::WelcomeWidget::onCreateChatbot()
{
    m_mw->newFile();
    m_mw->show();
    close();
}

//--------------------------------------------------------------------------------------------------

void Lvk::FE::WelcomeWidget::onOpenLastChatbot()
{
    m_mw->openFile(m_lastFilename);
    m_mw->show();
    close();
}
