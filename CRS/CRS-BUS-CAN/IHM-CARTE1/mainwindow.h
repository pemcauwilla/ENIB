#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSocketNotifier>
#include "socketcan_cpp.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void updateData();
    void on_btnToggleCapteur_clicked();

private:
    Ui::MainWindow *ui;

    scpp::SocketCan can_port;

    bool modeLumiereActive;

    QSocketNotifier* canNotifier;
};
#endif // MAINWINDOW_H
