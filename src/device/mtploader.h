/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2019, Jonas Kvinge <jonas@jkvinge.net>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MTPLOADER_H
#define MTPLOADER_H

#include "config.h"

#include <memory>
#include <stdbool.h>

#include <QObject>
#include <QThread>
#include <QString>
#include <QUrl>

class TaskManager;
class CollectionBackend;
class ConnectedDevice;
class MtpDevice;
class MtpConnection;

class MtpLoader : public QObject {
  Q_OBJECT

 public:
  MtpLoader(const QUrl &url, TaskManager *task_manager, CollectionBackend *backend, std::shared_ptr<ConnectedDevice> device);
  ~MtpLoader();

  bool Init();

 public slots:
  void LoadDatabase();

 signals:
  void Error(const QString &message);
  void TaskStarted(int task_id);
  void LoadFinished(bool success);

 private:
  bool TryLoad();

 private:
  QUrl url_;
  TaskManager *task_manager_;
  CollectionBackend *backend_;
  std::shared_ptr<ConnectedDevice> device_;
  std::shared_ptr<MtpConnection> connection_;
  QThread *original_thread_;

};

#endif  // MTPLOADER_H
