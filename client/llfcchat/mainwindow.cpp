#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "resetdialog.h"
#include "tcpmgr.h"
#include <QLayout>
#include <QMessageBox>
#include <QSize>

namespace {
const QSize kAuthDefaultSize(420, 640);
const QSize kAuthMinimumSize(360, 560);
const QSize kChatMinimumSize(1050, 900);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    _login_dlg(nullptr),
    _reg_dlg(nullptr),
    _reset_dlg(nullptr),
    _chat_dlg(nullptr)
{
    ui->setupUi(this);
    //创建一个CentralWidget, 并将其设置为MainWindow的中心部件
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    setCentralWidget(_login_dlg);
    applyAuthWindowSize(true);

    //连接登录界面注册信号
    connect(_login_dlg, &LoginDialog::switchRegister, this, &MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg, &LoginDialog::switchReset, this, &MainWindow::SlotSwitchReset);
    //连接创建聊天界面信号
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_swich_chatdlg, this, &MainWindow::SlotSwitchChat);
    connect(TcpMgr::GetInstance().get(),&TcpMgr::sig_notify_offline, this, &MainWindow::SlotNotifyOffline);


    //测试用
    //emit TcpMgr::GetInstance()->sig_swich_chatdlg();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::applyAuthWindowSize(bool reset_to_default)
{
    setMinimumSize(kAuthMinimumSize);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    if(reset_to_default){
        resize(kAuthDefaultSize);
    }
}

void MainWindow::SlotSwitchReg()
{
    _reg_dlg = new RegisterDialog(this);
    _reg_dlg->hide();

    _reg_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);

     //连接注册界面返回登录信号
    connect(_reg_dlg, &RegisterDialog::sigSwitchLogin, this, &MainWindow::SlotSwitchLogin);
    if(_login_dlg){
        _login_dlg->hide();
    }
    setCentralWidget(_reg_dlg);
    _login_dlg = nullptr;
    applyAuthWindowSize(false);
    _reg_dlg->show();
}

//从注册界面返回登录界面
void MainWindow::SlotSwitchLogin()
{
    //创建一个CentralWidget, 并将其设置为MainWindow的中心部件
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    if(_reg_dlg){
        _reg_dlg->hide();
    }
    setCentralWidget(_login_dlg);
    _reg_dlg = nullptr;
    applyAuthWindowSize(false);

    _login_dlg->show();
    //连接登录界面注册信号
    connect(_login_dlg, &LoginDialog::switchRegister, this, &MainWindow::SlotSwitchReg);
    //连接登录界面忘记密码信号
    connect(_login_dlg, &LoginDialog::switchReset, this, &MainWindow::SlotSwitchReset);
}

void MainWindow::SlotSwitchReset()
{
    //创建一个CentralWidget, 并将其设置为MainWindow的中心部件
    _reset_dlg = new ResetDialog(this);
    _reset_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    if(_login_dlg){
        _login_dlg->hide();
    }
    setCentralWidget(_reset_dlg);
    _login_dlg = nullptr;
    applyAuthWindowSize(false);

    _reset_dlg->show();
    //注册返回登录信号和槽函数
    connect(_reset_dlg, &ResetDialog::switchLogin, this, &MainWindow::SlotSwitchLogin2);
}

//从重置界面返回登录界面
void MainWindow::SlotSwitchLogin2()
{
    //创建一个CentralWidget, 并将其设置为MainWindow的中心部件
    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    if(_reset_dlg){
        _reset_dlg->hide();
    }
    setCentralWidget(_login_dlg);
    _reset_dlg = nullptr;
    applyAuthWindowSize(false);

    _login_dlg->show();
    //连接登录界面忘记密码信号
    connect(_login_dlg, &LoginDialog::switchReset, this, &MainWindow::SlotSwitchReset);
    //连接登录界面注册信号
    connect(_login_dlg, &LoginDialog::switchRegister, this, &MainWindow::SlotSwitchReg);
}

void MainWindow::SlotSwitchChat()
{
    _chat_dlg = new ChatDialog();
    _chat_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    if(_login_dlg){
        _login_dlg->hide();
    }
    setCentralWidget(_chat_dlg);
    _login_dlg = nullptr;
    _chat_dlg->show();
    this->setMinimumSize(kChatMinimumSize);
    this->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
    QSize chat_size = size().expandedTo(kChatMinimumSize);
    if(chat_size != size()){
        resize(chat_size);
    }
}

void MainWindow::SlotNotifyOffline()
{
    QMessageBox::warning(this, tr("提示"), tr("账号已在其他设备登录，当前连接已断开。"));

    _login_dlg = new LoginDialog(this);
    _login_dlg->setWindowFlags(Qt::CustomizeWindowHint|Qt::FramelessWindowHint);
    if(_chat_dlg){
        _chat_dlg->hide();
    }
    setCentralWidget(_login_dlg);
    _chat_dlg = nullptr;
    _reg_dlg = nullptr;
    _reset_dlg = nullptr;
    applyAuthWindowSize(true);
    _login_dlg->show();

    connect(_login_dlg, &LoginDialog::switchRegister, this, &MainWindow::SlotSwitchReg);
    connect(_login_dlg, &LoginDialog::switchReset, this, &MainWindow::SlotSwitchReset);
}
