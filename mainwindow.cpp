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

    char read_msg_[512];

    boost::asio::io_service m_io;
    boost::asio::serial_port m_port;
    lsl::stream_outlet *m_outlet;

private:

    void handler(  const boost::system::error_code& error, size_t bytes_transferred)
    {
        if (!error)
        {  // read completed, so process the data
            read_msg_[bytes_transferred]=0;
#ifndef NDEBUG
            std::cout << bytes_transferred << " bytes: " << read_msg_ << std::endl;
#endif
            // forward data to outlet
            if (m_outlet!=nullptr && bytes_transferred) {
                m_outlet->push_sample(read_msg_);
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
#ifndef NDEBUG
        if (error == boost::asio::error::operation_aborted)
            return; // ignore it because the connection cancelled the timer
        if (error)
            std::cerr << "Error: " << error.message() << std::endl; // show the error message
        else
            std::cout << "Error: Connection did not succeed.\n";
#endif
        m_port.close();
    }
public:

    void close() // call the do_close function via the io service in the other thread
    {
        m_io.post(boost::bind(&Serial::do_close, this, boost::system::error_code()));
    }

    Serial(const char *dev_name, unsigned int baud, lsl::stream_outlet *outlet) : m_io(),
        m_port(m_io, dev_name), m_outlet(outlet)
    {
//        m_port.set_option( boost::asio::serial_port_base::parity() );	// default none
//        m_port.set_option( boost::asio::serial_port_base::character_size( 8 ) );
//        m_port.set_option( boost::asio::serial_port_base::stop_bits() );	// default one
        m_port.set_option( boost::asio::serial_port_base::baud_rate( baud ) );

        read_some();

        // run the IO service as a separate thread, so the main thread can do others
        boost::thread t(boost::bind(&boost::asio::io_service::run, &m_io));
    }

};


MainWindow::MainWindow(QWidget *parent, const std::string &config_file) :
QMainWindow(parent),
ui(new Ui::MainWindow)
{
	ui->setupUi(this);

	// parse startup config file
	load_config(config_file);

	// make GUI connections
	QObject::connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(close()));
    QObject::connect(ui->linkButton, SIGNAL(clicked()), this, SLOT(on_link()));
    QObject::connect(this, SIGNAL(error()), this, SLOT(on_link()));
	QObject::connect(ui->actionLoad_Configuration, SIGNAL(triggered()), this, SLOT(load_config_dialog()));
	QObject::connect(ui->actionSave_Configuration, SIGNAL(triggered()), this, SLOT(save_config_dialog()));
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
}

void MainWindow::load_config(const std::string &filename) {
	using boost::property_tree::ptree;
	ptree pt;

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
		ui->baudRate->setValue(pt.get<int>("coresettings.baudrate",57600));
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
}

void MainWindow::save_config(const std::string &filename) {
	using boost::property_tree::ptree;
	ptree pt;

	// transfer UI content into property tree
	try {
        pt.put("coresettings.comport",ui->comPort->text().toStdString());
		pt.put("coresettings.baudrate",ui->baudRate->value());
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
			QMessageBox::critical(this,"Error",(std::string("Could not stop the background processing: ")+=e.what()).c_str(),QMessageBox::Ok);
			return;
		}

		// indicate that we are now successfully unlinked
		ui->linkButton->setText("Link");
    }
    else {
//		// === perform link action ===

        try {
            // get the UI parameters...
            std::string  comPort = ui->comPort->text().toStdString();
            int baudRate = ui->baudRate->value();
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
            reader_thread_ = std::make_unique<std::thread>(&MainWindow::read_thread, this, comPort, baudRate, samplingRate, chunkSize, streamName);

        }
        catch(std::exception &e) {
            QMessageBox::critical(this,"Error",(std::string("Error during initialization: ")+=e.what()).c_str(),QMessageBox::Ok);
            return;
        }

		// done, all successful
		ui->linkButton->setText("Unlink");
	}
}


// background data reader thread
void MainWindow::read_thread(const std::string &comPort, int baudRate,
                             int samplingRate, int chunkSize, const std::string &streamName)
{
    try {

		// create streaminfo
		lsl::stream_info info(streamName,"Raw",1,samplingRate,lsl::cf_int16,std::string("SerialPort_") + streamName);
		// append some meta-data
		lsl::xml_element channels = info.desc().append_child("channels");
		channels.append_child("channel")
			.append_child_value("label","Channel1")
			.append_child_value("type","Raw")
			.append_child_value("unit","integer");
		info.desc().append_child("acquisition")
			.append_child("hardware")
            .append_child_value("com_port", comPort)
            .append_child_value("baud_rate",std::to_string(baudRate));

		// make a new outlet
		lsl::stream_outlet outlet(info,chunkSize);

        // enter transmission loop
        Serial receiver(comPort.c_str(), (unsigned int) baudRate, &outlet);

		while (!shutdown_) {

//          /// TMP SIMULATE DATA
//            ///            sample = (short) (rand() * 255.f);
//          sample = sample + 1;
//          sample = sample > 300 ? 0: sample;
//			// transmit it
//			outlet.push_sample(&sample);

            /// TMP simulate fps
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
	catch(std::exception &e) {
		// any other error
        QMessageBox::critical(this,"Error",(std::string("Error during operation : ")+=e.what()).c_str(),QMessageBox::Ok);
        // indicate that we are now unlinked
        emit error();
	}

//	CloseHandle(hPort);
}

MainWindow::~MainWindow() {
	delete ui;
}
