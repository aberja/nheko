// SPDX-FileCopyrightText: 2021 Nheko Contributors
// SPDX-FileCopyrightText: 2022 Nheko Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Manager.h"

#include <QRegularExpression>
#include <QTextDocumentFragment>

#include "Cache.h"
#include "EventAccessors.h"
#include "MxcImageProvider.h"
#include "Utils.h"

#include <mtx/responses/notifications.hpp>

#include <variant>

static QString
formatNotification(const mtx::responses::Notification &notification)
{
    return utils::stripReplyFallbacks(notification.event, {}, {}).quoted_body;
}

void
NotificationsManager::postNotification(const mtx::responses::Notification &notification,
                                       const QImage &icon)
{
    Q_UNUSED(icon)

    const auto room_name = QString::fromStdString(cache::singleRoomInfo(notification.room_id).name);
    const auto sender =
      cache::displayName(QString::fromStdString(notification.room_id),
                         QString::fromStdString(mtx::accessors::sender(notification.event)));

    const auto room_id  = QString::fromStdString(notification.room_id);
    const auto event_id = QString::fromStdString(mtx::accessors::event_id(notification.event));

    const auto isEncrypted = std::get_if<mtx::events::EncryptedEvent<mtx::events::msg::Encrypted>>(
                               &notification.event) != nullptr;
    const auto isReply = utils::isReply(notification.event);

    // Putting these here to pass along since I'm not sure how
    // our translate step interacts with .mm files
    const auto respondStr  = QObject::tr("Respond");
    const auto sendStr     = QObject::tr("Send");
    const auto placeholder = QObject::tr("Write a message...");

    auto playSound = false;

    if (std::find(notification.actions.begin(),
                  notification.actions.end(),
                  mtx::pushrules::actions::Action{mtx::pushrules::actions::set_tweak_sound{
                    .value = "default"}}) != notification.actions.end()) {
        playSound = true;
    }
    if (isEncrypted) {
        // TODO: decrypt this message if the decryption setting is on in the UserSettings
        const QString messageInfo = (isReply ? tr("%1 replied with an encrypted message")
                                             : tr("%1 sent an encrypted message"))
                                      .arg(sender);
        objCxxPostNotification(room_name,
                               room_id,
                               event_id,
                               messageInfo,
                               "",
                               "",
                               respondStr,
                               sendStr,
                               placeholder,
                               playSound);
    } else {
        const QString messageInfo =
          (isReply ? tr("%1 replied to a message") : tr("%1 sent a message")).arg(sender);
        if (mtx::accessors::msg_type(notification.event) == mtx::events::MessageType::Image)
            MxcImageProvider::download(
              QString::fromStdString(mtx::accessors::url(notification.event)).remove("mxc://"),
              QSize(200, 80),
              [this,
               notification,
               room_name,
               room_id,
               event_id,
               messageInfo,
               respondStr,
               sendStr,
               placeholder,
               playSound](QString, QSize, QImage, QString imgPath) {
                  objCxxPostNotification(room_name,
                                         room_id,
                                         event_id,
                                         messageInfo,
                                         formatNotification(notification),
                                         imgPath,
                                         respondStr,
                                         sendStr,
                                         placeholder,
                                         playSound);
              });
        else
            objCxxPostNotification(room_name,
                                   room_id,
                                   event_id,
                                   messageInfo,
                                   formatNotification(notification),
                                   "",
                                   respondStr,
                                   sendStr,
                                   placeholder,
                                   playSound);
    }
}
