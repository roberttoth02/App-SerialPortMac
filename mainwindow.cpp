#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>


// serial port
#include <boost/asio.hpp>
#include <boost/asio/serial_port.hpp>
#include <boost/thread.hpp>

#ifndef NDEBUG
#include <iostream>
#endif

class Serial {

public:

    typedef enum {
        PASSTHROUGH = 0,
        READ_INTEGER = 1,
        READ_FLOATINGPOINT = 2
    } TransferMode;

    Serial(const char *dev_name, unsigned int baud, unsigned int dataBits, int parity, int stopbits,
           TransferMode mode, lsl::stream_outlet *outlet) : m_io(),  m_port(m_io, dev_name), m_mode(mode), m_outlet(outlet)
    {
#ifndef NDEBUG
        std::cerr << " Opened Serial: " << dev_name << std::endl;
#endif
        // transfer options to m_port
        m_port.set_option( boost::asio::serial_port_base::baud_rate( baud ) );
        m_port.set_option( boost::asio::serial_port_base::character_size( dataBits ) );
        m_port.set_option( boost::asio::serial_port_base::parity( (boost::asio::serial_port_base::parity::type) parity ) );
        m_port.set_option( boost::asio::serial_port_base::stop_bits((boost::asio::serial_port_base::stop_bits::type) stopbits) );

        // start reading
        read_some();

        // run the IO service as a separate thread, so the main thread can do others
        boost::thread t(boost::bind(&boost::asio::io_service::run, &m_io));
    }

    void close() // call the do_close function via the io service in the other thread
    {
        m_io.post(boost::bind(&Serial::do_close, this, boost::system::error_code()));
    }


private:

    char read_msg_[512];

    boost::asio::io_service m_io;
    boost::asio::serial_port m_port;
    TransferMode m_mode;
    lsl::stream_outlet *m_outlet;

    void handler(  const boost::system::error_code& error, size_t bytes_transferred)
    {
        if (!error) {

            if (m_outlet !=nullptr && bytes_transferred > 0) {

                // read completed, so process the data
                read_msg_[bytes_transferred]=0;
               // std::cout << bytes_transferred << " bytes: " << read_msg_ << std::endl;

                // in PASSTHROUGH MODE, just forward buffer read to the outlet
                if (m_mode == PASSTHROUGH) {
                    m_outlet->push_sample(read_msg_);
                }
                // else READ the message read in serial and try to convert to INTEGER or FLOATINGPOINT numbers
                else {

                    static QRegularExpression rx_integers("^[+-]?\\d+"); // integers only
                    static QRegularExpression rx_numbers("^[+-]?((\\d*\\.?\\d+)|(\\d+\\.?\\d*))"); // any numbers with a dot

                    QRegularExpressionMatch matches;
                    if (m_mode == READ_INTEGER) {
                        matches = rx_integers.match(read_msg_);
                        if (matches.hasMatch()) {
                            bool ok = false;
                            int32_t value = matches.captured(0).toInt(&ok);
                            if (ok)
                                m_outlet->push_sample(&value);
                        }
                    }
                    else {
                        matches = rx_numbers.match(read_msg_);
                        if (matches.hasMatch()) {
                            bool ok = false;
                            double value = matches.captured(0).toDouble(&ok);
                            if (ok)
                                m_outlet->push_sample(&value);
                        }
                    }


                }
            }

            read_some(); // start waiting for another asynchronous read again
        }
        else
            do_close(error);
    }

    void read_some()
    {
        m_port.async_read_some(boost::asio::buffer(read_msg_,512),
                               boost::bind(&Serial::handler,  this,
                               boost::asio::placeholders::error,
                               boost::asio::placeholders::bytes_transferred)
                               );
    }

    void do_close(const boost::system::error_code& error)
    {
        if (error == boost::asio::error::operation_aborted)
            return; // ignore it because the connection cancelled the timer
#ifndef NDEBUG
        if (error)
            std::cerr << "Error: " << error.message() << std::endl; // show the error message
        else
            std::cout << "Error: Connection did not succeed.\n";
#endif
        m_port.close();
    }
};


MainWindow::MainWindow(QWidget *parent, const std::string &config_file) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Parse startup config file
    load_config(config_file);

    // Setup macOS-specific menu handling
    QMenu *appMenu = menuBar()->addMenu(tr("Application"));
    QAction *quitAction = appMenu->addAction(tr("Quit"));
    quitAction->setShortcut(QKeySequence::Quit); 
    QObject::connect(quitAction, &QAction::triggered, QCoreApplication::quit);

    // Make GUI connections
    QObject::connect(ui->actionQuit, &QAction::triggered, QCoreApplication::quit);
    QObject::connect(ui->linkButton, &QPushButton::clicked, this, &MainWindow::on_link);
    QObject::connect(this, &MainWindow::error, this, &MainWindow::on_link);
    QObject::connect(ui->actionLoad_Configuration, &QAction::triggered, this, &MainWindow::load_config_dialog);
    QObject::connect(ui->actionSave_Configuration, &QAction::triggered, this, &MainWindow::save_config_dialog);
}


void MainWindow::load_config_dialog() {
    QString sel = QFileDialog::getOpenFileName(this,"Load Configuration File","","Configuration Files (*.cfg)");
    if (!sel.isEmpty())
        load_config(sel.toStdString());
}

void MainWindow::save_config_dialog() {
    QString sel = QFileDialog::getSaveFileName(this,"Save Configuration File","","Configuration Files (*.cfg)");
    if (!sel.isEmpty())
        save_config(sel.toStdString());
}

void MainWindow::closeEvent(QCloseEvent *ev) {
    if (reader_thread_)
        ev->ignore();
    save_config(_filename);
}

void MainWindow::load_config(const std::string &filename) {
    using boost::property_tree::ptree;
    ptree pt;

    if (filename.empty()) {
        return;
    }

    // parse file
    try {
        read_xml(filename, pt);
    } catch(std::exception &e) {
        QMessageBox::information(this,"Error",(std::string("Cannot read config file: ")+= e.what()).c_str(),QMessageBox::Ok);
        return;
    }

    // get config values
    try {
        ui->comPort->setText(pt.get<std::string>("coresettings.comport","/dev/ttyS0").c_str());
        ui->baudRate->setValue(pt.get<int>("coresettings.baudrate",9600));
        ui->datamode->setCurrentIndex(pt.get<int>("coresettings.datamode",0));
        ui->samplingRate->setValue(pt.get<int>("streamsettings.samplingrate",0));
        ui->chunkSize->setValue(pt.get<int>("streamsettings.chunksize",32));
        ui->streamName->setText(pt.get<std::string>("streamsettings.streamname","SerialPort").c_str());
        ui->dataBits->setCurrentIndex(pt.get<int>("miscsettings.databits",4));
        ui->parity->setCurrentIndex(pt.get<int>("miscsettings.parity",0));
        ui->stopBits->setCurrentIndex(pt.get<int>("miscsettings.stopbits",0));
        ui->readIntervalTimeout->setValue(pt.get<int>("timeoutsettings.readintervaltimeout",500));
        ui->readTotalTimeoutConstant->setValue(pt.get<int>("timeoutsettings.readtotaltimeoutconstant",50));
        ui->readTotalTimeoutMultiplier->setValue(pt.get<int>("timeoutsettings.readtotaltimeoutmultiplier",10));
    } catch(std::exception &) {
        QMessageBox::information(this,"Error in Config File","Could not read out config parameters.",QMessageBox::Ok);
        return;
    }

    _filename = filename;
}

void MainWindow::save_config(const std::string &filename) {
    using boost::property_tree::ptree;
    ptree pt;

    if (filename.empty()) {
        return;
    }

    // transfer UI content into property tree
    try {
        pt.put("coresettings.comport",ui->comPort->text().toStdString());
        pt.put("coresettings.baudrate",ui->baudRate->value());
        pt.put("coresettings.datamode",ui->datamode->currentIndex());
        pt.put("streamsettings.samplingrate",ui->samplingRate->value());
        pt.put("streamsettings.chunksize",ui->chunkSize->value());
        pt.put("streamsettings.streamname",ui->streamName->text().toStdString());
        pt.put("miscsettings.databits",ui->dataBits->currentIndex());
        pt.put("miscsettings.parity",ui->parity->currentIndex());
        pt.put("miscsettings.stopbits",ui->stopBits->currentIndex());
        pt.put("timeoutsettings.readintervaltimeout",ui->readIntervalTimeout->value());
        pt.put("timeoutsettings.readtotaltimeoutconstant",ui->readTotalTimeoutConstant->value());
        pt.put("timeoutsettings.readtotaltimeoutmultiplier",ui->readTotalTimeoutMultiplier->value());
    } catch(std::exception &e) {
        QMessageBox::critical(this,"Error",(std::string("Could not prepare settings for saving: ")+=e.what()).c_str(),QMessageBox::Ok);
    }

    // write to disk
    try {
        write_xml(filename, pt);
    } catch(std::exception &e) {
        QMessageBox::critical(this,"Error",(std::string("Could not write to config file: ")+=e.what()).c_str(),QMessageBox::Ok);
    }

    _filename = filename;
}


// start/stop the cognionics connection
void MainWindow::on_link()
{
    if (reader_thread_) {
        // === perform unlink action ===
        try {
            shutdown_ = true;
            reader_thread_->join();
            reader_thread_.reset();
        } catch(std::exception &e) {
            //QMessageBox::critical(this,"Error",(std::string("Could not stop the background processing: ")+=e.what()).c_str(),QMessageBox::Ok); //@
            return;
        }

        // indicate that we are now successfully unlinked
        ui->linkButton->setText("Link");
        ui->linkButton->setStyleSheet("background-color: rgb(94, 186, 125);");
    }
    else {
        // === perform link action ===
        try {
            // get the UI parameters...
            std::string  comPort = ui->comPort->text().toStdString();
            int baudRate = ui->baudRate->value();
            int readmode = ui->datamode->currentIndex();
            int samplingRate = ui->samplingRate->value();
            int chunkSize = ui->chunkSize->value();
            std::string streamName = ui->streamName->text().toStdString();
            int dataBits = ui->dataBits->currentIndex()+4;
            int parity = ui->parity->currentIndex();
            int stopBits = ui->stopBits->currentIndex();
            int readIntervalTimeout = ui->readIntervalTimeout->value();
            int readTotalTimeoutConstant = ui->readTotalTimeoutConstant->value();
            int readTotalTimeoutMultiplier = ui->readTotalTimeoutMultiplier->value();

            // start reading
            shutdown_ = false;
            reader_thread_ = std::make_unique<std::thread>(&MainWindow::read_thread, this, comPort,
                                                           baudRate, dataBits,  parity, stopBits, readmode,
                                                           samplingRate, chunkSize, streamName);
        }
        catch(std::exception &e) {
            //QMessageBox::critical(this,"Error",(std::string("Error during initialization: ")+=e.what()).c_str(),QMessageBox::Ok); //@
            return;
        }

        // done, all successful
        ui->linkButton->setText("Unlink");
        ui->linkButton->setStyleSheet("background-color: rgb(185, 49, 53);");
    }
}


// background data reader thread
void MainWindow::read_thread(const std::string &comPort, unsigned int baudRate, unsigned int dataBits, int parity, int stopbits, int readmode,
                             int samplingRate, int chunkSize, const std::string &streamName)
{
    try {

        lsl::channel_format_t format = lsl::cf_int16;
        if (readmode == Serial::READ_FLOATINGPOINT)
            format = lsl::cf_float32;
        else if (readmode == Serial::READ_INTEGER)
            format = lsl::cf_int32;

        // create streaminfo
        lsl::stream_info info(streamName,"Raw", 1, samplingRate, format, std::string("SerialPort_") + streamName);

        // append some meta-data
        lsl::xml_element channels = info.desc().append_child("channels");
        channels.append_child("channel")
                .append_child_value("label","Channel1")
                .append_child_value("type","Raw")
                .append_child_value("unit", (readmode == Serial::READ_FLOATINGPOINT) ? "float" : "integer");
        info.desc().append_child("acquisition")
            .append_child("hardware")
            .append_child_value("com_port", comPort)
            .append_child_value("baud_rate",std::to_string(baudRate));

        // make a new outlet
        lsl::stream_outlet outlet(info, chunkSize);

        // Creates a Serial port receiver : this enters transmission loop to outlet
        Serial receiver(comPort.c_str(), baudRate, dataBits, parity, stopbits, (Serial::TransferMode) readmode, &outlet);

        // keep serial alive until shutdown
        while (!shutdown_) {

            /// TMP simulate fps
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    catch(std::exception &e) {
        // any other error
        //QMessageBox::critical(nullptr,"Error",(std::string("Error during operation : ")+=e.what()).c_str(),QMessageBox::Ok); //@
        // indicate that we are now unlinked
        //emit error(); //@
    }

}

MainWindow::~MainWindow() {
    delete ui;
}
