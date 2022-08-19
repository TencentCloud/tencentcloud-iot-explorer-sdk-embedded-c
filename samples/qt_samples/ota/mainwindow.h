#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDebug>
#include <QThread>
#include <QMessageBox>
#include <QMainWindow>
#include "tencentiotcsdk.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_start_ota_update_clicked();

    void ota_update_success();
    void ota_update_progress(int);
    void handler_debug_data(QString);
private:
    Ui::MainWindow *ui;
    TencentIotCSDK *thread;
};
#endif // MAINWINDOW_H
