    #include "mainwindow.h"
    #include "ui_mainwindow.h"
    #include <QDebug>

    #define CAN_RECEIVE_MESSAGE_ID 0x21
    #define CAN_SEND_MESSAGE_ID 0x02

    MainWindow::MainWindow(QWidget *parent)
        : QMainWindow(parent)
        , ui(new Ui::MainWindow)
    {
        ui->setupUi(this);

        // Begin in distance mode
        modeLumiereActive = false;
        ui->opticalValueTitle->setText("Valeur Actuelle : Distance");
        ui->btnToggleCapteur->setText("Passer en mode Lumière");

        if (can_port.open("can0") == scpp::STATUS_OK) {

            canNotifier = new QSocketNotifier(can_port.socketFd(), QSocketNotifier::Read, this);
            connect(canNotifier, &QSocketNotifier::activated, this, &MainWindow::updateData);
        }
    }

    MainWindow::~MainWindow()
    {
        delete ui;
    }

    // =========================================================
    // L'ACTION DU BOUTON
    // =========================================================
    void MainWindow::on_btnToggleCapteur_clicked()
    {
        // Change active mode
        modeLumiereActive = !modeLumiereActive;

        // Prepare CAN payload to send to the board
        scpp::CanFrame commande;

        commande.id = CAN_SEND_MESSAGE_ID;
        commande.len = 1;

        // Update UI
        if (modeLumiereActive) {
            ui->opticalValueTitle->setText("Valeur Actuelle : Luminosité");
            ui->btnToggleCapteur->setText("Passer en mode Distance");
            commande.data[0] = 0x01; // 1 = Activate light measuring
        } else {
            ui->opticalValueTitle->setText("Valeur Actuelle : Distance");
            ui->btnToggleCapteur->setText("Passer en mode Lumière");
            commande.data[0] = 0x00; // 0 = Activate distance measuring
        }

        // Erase old value while waiting for the new one
        ui->opticalValueLabel->setText("--");

        // Write on the CAN bus
        if (can_port.write(commande) != scpp::STATUS_OK) {
            qDebug() << "Erreur : Impossible d'envoyer la commande au STM32 !";
        } else {
            qDebug() << "Commande envoyée avec succès !";
        }
    }

    // =========================================================
    // LOOP Reading
    // =========================================================
    void MainWindow::updateData()
    {
        scpp::CanFrame frame;

        if (can_port.read(frame) == scpp::STATUS_OK) {
            if (frame.id == CAN_RECEIVE_MESSAGE_ID) {

                // Update Optical Sensor Value and Title

                int opticalValue = (frame.data[1] << 8) | frame.data[2];

                if (frame.data[0] == 0) {
                    ui->opticalValueTitle->setText("Valeur Actuelle : Distance");
                    ui->opticalValueLabel->setText(QString("%1 mm").arg(opticalValue));

                    ui->btnToggleCapteur->setText("Passer en mode Lumière");
                    modeLumiereActive = false;
                }
                else if (frame.data[0] == 1) {
                    ui->opticalValueTitle->setText("Valeur Actuelle : Luminosité");
                    ui->opticalValueLabel->setText(QString("%1 lux").arg(opticalValue));

                    ui->btnToggleCapteur->setText("Passer en mode Distance");
                    modeLumiereActive = true;
                }

                // Update Ambient Sensor Values

                int pressure = (frame.data[3] << 8) | frame.data[4];
                ui->pressureLabel->setText(QString("Pression : %1 hPa").arg(pressure));

                int temp = (frame.data[5] << 8) | frame.data[6];
                ui->temperatureLabel->setText(QString("Témperature : %1 °C").arg(temp));

                int umid = frame.data[7];
                ui->umidityLabel->setText(QString("Humidité : %1 %").arg(umid));
            }
        }
    }
