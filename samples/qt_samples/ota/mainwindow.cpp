#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    thread = new TencentIotCSDK();
    ui->progressBar->setRange(0,100);
    ui->progressBar->setValue(0);
    qRegisterMetaType<uint8_t>("uint8_t");
    qRegisterMetaType<uint16_t>("uint16_t");
    qRegisterMetaType<QVector<int>>("QVector<int>");
    qRegisterMetaType<QVector<double>>("QVector<double>");
    qRegisterMetaType<QVector<QString>>("QVector<QString>");
    connect(thread, &TencentIotCSDK::send_file_progress, this, &MainWindow::ota_update_progress);
    connect(thread, &TencentIotCSDK::send_ota_success, this, &MainWindow::ota_update_success);
    connect(thread, &TencentIotCSDK::send_ota_debug_info, this, &MainWindow::handler_debug_data);
    ui->textBrowser->document()->setMaximumBlockCount(1024);
    ui->textBrowser->setOpenLinks(false);
    ui->textBrowser->setOpenExternalLinks(false);
    this->setWindowTitle("Tencent Iot Explorer OTA测试工具");
}

void MainWindow::ota_update_progress(int progress)
{
    ui->progressBar->setValue(progress);
}

void MainWindow::handler_debug_data(QString debug)
{
    ui->textBrowser->append(debug);
}

void MainWindow::ota_update_success()
{
    ui->progressBar->setValue(100);
    QMessageBox::information(NULL, "信息", "固件下载成功!",QMessageBox::Yes);
    ui->start_ota_update->setEnabled(true);
    ui->progressBar->setValue(0);
}

MainWindow::~MainWindow()
{
    delete thread;
    delete ui;
}


void MainWindow::on_start_ota_update_clicked()
{
    ui->start_ota_update->setEnabled(false);
    thread->start();
}
