#include "chatpage.h"
#include "ui_chatpage.h"
#include <QStyleOption>
#include <QPainter>
#include "ChatItemBase.h"
#include "TextBubble.h"
#include "PictureBubble.h"
#include "applyfrienditem.h"
#include "usermgr.h"
#include <QJsonArray>
#include <QJsonObject>
#include "tcpmgr.h"
#include <QUuid>
#include <QCryptographicHash>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>

ChatPage::ChatPage(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ChatPage)
{
    ui->setupUi(this);
    //设置按钮样式
    ui->receive_btn->SetState("normal","hover","press");
    ui->send_btn->SetState("normal","hover","press");

    //设置图标样式
    ui->emo_lb->SetState("normal","hover","press","normal","hover","press");
    ui->file_lb->SetState("normal","hover","press","normal","hover","press");
    connect(ui->file_lb, &ClickedLabel::clicked, this, &ChatPage::on_file_lb_clicked);

}

ChatPage::~ChatPage()
{
    delete ui;
}

void ChatPage::SetUserInfo(std::shared_ptr<UserInfo> user_info)
{
    _user_info = user_info;
    //设置ui界面
    ui->title_lb->setText(_user_info->_name);
    ui->chat_data_list->removeAllItem();
    for(auto & msg : user_info->_chat_msgs){
        AppendChatMsg(msg);
    }
}

void ChatPage::AppendChatMsg(std::shared_ptr<TextChatData> msg)
{
    auto self_info = UserMgr::GetInstance()->GetUserInfo();
    ChatRole role;
    //todo... 添加聊天显示
    if (msg->_from_uid == self_info->_uid) {
        role = ChatRole::Self;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        
        pChatItem->setUserName(self_info->_name);
        pChatItem->setUserIcon(QPixmap(self_info->_icon));
        QWidget* pBubble = nullptr;
        pBubble = new TextBubble(role, msg->_msg_content);
        pChatItem->setWidget(pBubble);
        ui->chat_data_list->appendChatItem(pChatItem);
    }
    else {
        role = ChatRole::Other;
        ChatItemBase* pChatItem = new ChatItemBase(role);
        auto friend_info = UserMgr::GetInstance()->GetFriendById(msg->_from_uid);
        if (friend_info == nullptr) {
            return;
        }
        pChatItem->setUserName(friend_info->_name);
        pChatItem->setUserIcon(QPixmap(friend_info->_icon));
        QWidget* pBubble = nullptr;
        pBubble = new TextBubble(role, msg->_msg_content);
        pChatItem->setWidget(pBubble);
        ui->chat_data_list->appendChatItem(pChatItem);
    }


}

void ChatPage::paintEvent(QPaintEvent *event)
{
    QStyleOption opt;
    opt.init(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
}

void ChatPage::on_send_btn_clicked()
{
    if (_user_info == nullptr) {
        qDebug() << "friend_info is empty";
        return;
    }

    auto user_info = UserMgr::GetInstance()->GetUserInfo();
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Self;
    QString userName = user_info->_name;
    QString userIcon = user_info->_icon;

    const QVector<MsgInfo>& msgList = pTextEdit->getMsgList();
    QJsonObject textObj;
    QJsonArray textArray;
    int txt_size = 0;

    for(int i=0; i<msgList.size(); ++i)
    {
        QString type = msgList[i].msgFlag;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        pChatItem->setUserIcon(QPixmap(userIcon));
        QWidget *pBubble = nullptr;

        if(type == "text")
        {   
            //消息内容长度不合规就跳过
            if(msgList[i].content.length() > 1024){
                delete pChatItem;
                continue;
            }

            //生成唯一id
            QUuid uuid = QUuid::createUuid();
            //转为字符串
            QString uuidString = uuid.toString();

            pBubble = new TextBubble(role, msgList[i].content);
            if(txt_size + msgList[i].content.length()> 1024){
                textObj["fromuid"] = user_info->_uid;
                textObj["touid"] = _user_info->_uid;
                textObj["text_array"] = textArray;
                QJsonDocument doc(textObj);
                QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
                //发送并清空之前累计的文本列表
                txt_size = 0;
                textArray = QJsonArray();
                textObj = QJsonObject();
                //发送tcp请求给chat server
                emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
            }

            //将bubble和uid绑定，以后可以等网络返回消息后设置是否送达
            //_bubble_map[uuidString] = pBubble;
            txt_size += msgList[i].content.length();
            QJsonObject obj;
            QByteArray utf8Message = msgList[i].content.toUtf8();
            obj["content"] = QString::fromUtf8(utf8Message);
            obj["msgid"] = uuidString;
            textArray.append(obj);
            auto txt_msg = std::make_shared<TextChatData>(uuidString, obj["content"].toString(),
                user_info->_uid, _user_info->_uid);
            emit sig_append_send_chat_msg(txt_msg);
        }
        else if(type == "image")
        {
             pBubble = new PictureBubble(QPixmap(msgList[i].content) , role);
        }
        else if(type == "file")
        {
            pBubble = new PictureBubble(msgList[i].pixmap, role);
            sendFileMsg(msgList[i].content, user_info->_uid, _user_info->_uid);
        }
        //发送消息
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            ui->chat_data_list->appendChatItem(pChatItem);
        }
        else {
            delete pChatItem;
        }

    }

    if(!textArray.isEmpty()){
        qDebug() << "textArray is " << textArray ;
        //发送给服务器
        textObj["text_array"] = textArray;
        textObj["fromuid"] = user_info->_uid;
        textObj["touid"] = _user_info->_uid;
        QJsonDocument doc(textObj);
        QByteArray jsonData = doc.toJson(QJsonDocument::Compact);
        //发送并清空之前累计的文本列表
        txt_size = 0;
        textArray = QJsonArray();
        textObj = QJsonObject();
        //发送tcp请求给chat server
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_TEXT_CHAT_MSG_REQ, jsonData);
    }
}

void ChatPage::on_receive_btn_clicked()
{
    auto pTextEdit = ui->chatEdit;
    ChatRole role = ChatRole::Other;
    QString userName = _user_info->_name;
    QString userIcon = _user_info->_icon;

    const QVector<MsgInfo>& msgList = pTextEdit->getMsgList();
    for(int i=0; i<msgList.size(); ++i)
    {
        QString type = msgList[i].msgFlag;
        ChatItemBase *pChatItem = new ChatItemBase(role);
        pChatItem->setUserName(userName);
        pChatItem->setUserIcon(QPixmap(userIcon));
        QWidget *pBubble = nullptr;
        if(type == "text")
        {
            pBubble = new TextBubble(role, msgList[i].content);
        }
        else if(type == "image")
        {
            pBubble = new PictureBubble(QPixmap(msgList[i].content) , role);
        }
        else if(type == "file")
        {
            pBubble = new PictureBubble(msgList[i].pixmap, role);
        }
        if(pBubble != nullptr)
        {
            pChatItem->setWidget(pBubble);
            ui->chat_data_list->appendChatItem(pChatItem);
        }
    }
}

void ChatPage::clearItems()
{
    ui->chat_data_list->removeAllItem();
}

void ChatPage::on_file_lb_clicked(QString, ClickLbState)
{
    QString file_path = QFileDialog::getOpenFileName(this, tr("Select file"));
    ui->file_lb->ResetNormalState();
    if(file_path.isEmpty()){
        return;
    }

    ui->chatEdit->insertFileFromUrl(QStringList() << file_path);
}

bool ChatPage::sendFileMsg(const QString& file_path, int from_uid, int to_uid)
{
    QFileInfo file_info(file_path);
    if(!file_info.exists() || !file_info.isFile()){
        QMessageBox::warning(this, tr("Tips"), tr("File does not exist."));
        return false;
    }

    if(file_info.size() > MAX_SEND_FILE_SIZE){
        QMessageBox::warning(this, tr("Tips"), tr("The file must be smaller than 4GB."));
        return false;
    }

    QFile file(file_path);
    if(!file.open(QIODevice::ReadOnly)){
        QMessageBox::warning(this, tr("Tips"), tr("Could not open file."));
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Md5);
    if(!hash.addData(&file)){
        QMessageBox::warning(this, tr("Tips"), tr("Could not read file."));
        return false;
    }

    QString md5 = hash.result().toHex();
    file.seek(0);

    qint64 total_size = file_info.size();
    qint64 trans_size = 0;
    int seq = 0;
    int last_seq = static_cast<int>((total_size + MAX_FILE_CHUNK_LEN - 1) / MAX_FILE_CHUNK_LEN);

    if(total_size == 0){
        QJsonObject jsonObj;
        jsonObj["fromuid"] = from_uid;
        jsonObj["touid"] = to_uid;
        jsonObj["md5"] = md5;
        jsonObj["name"] = file_info.fileName();
        jsonObj["seq"] = 1;
        jsonObj["total_size"] = QString::number(total_size);
        jsonObj["trans_size"] = QString::number(trans_size);
        jsonObj["last"] = 1;
        jsonObj["data"] = "";

        QJsonDocument doc(jsonObj);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_UPLOAD_FILE_REQ,
                                                  doc.toJson(QJsonDocument::Compact));
        return true;
    }

    while(!file.atEnd()){
        QByteArray chunk = file.read(MAX_FILE_CHUNK_LEN);
        if(chunk.isEmpty() && !file.atEnd()){
            QMessageBox::warning(this, tr("Tips"), tr("Could not read file chunk."));
            return false;
        }

        ++seq;
        trans_size += chunk.size();

        QJsonObject jsonObj;
        jsonObj["fromuid"] = from_uid;
        jsonObj["touid"] = to_uid;
        jsonObj["md5"] = md5;
        jsonObj["name"] = file_info.fileName();
        jsonObj["seq"] = seq;
        jsonObj["total_size"] = QString::number(total_size);
        jsonObj["trans_size"] = QString::number(trans_size);
        jsonObj["last"] = (seq == last_seq) ? 1 : 0;
        QByteArray encoded = chunk.toBase64();
        jsonObj["data"] = QString::fromLatin1(encoded.constData(), encoded.size());

        QJsonDocument doc(jsonObj);
        emit TcpMgr::GetInstance()->sig_send_data(ReqId::ID_UPLOAD_FILE_REQ,
                                                  doc.toJson(QJsonDocument::Compact));
    }

    return true;
}
