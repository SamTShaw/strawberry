/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 * Copyright 2013, Jonas Kvinge <jonas@strawbs.net>
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

#ifndef ABOUT_H
#define ABOUT_H

#include "config.h"

#include <stdbool.h>

#include <QObject>
#include <QWidget>
#include <QDialog>
#include <QList>
#include <QString>

#include "ui_about.h"

class About : public QDialog {
  Q_OBJECT

 public:
  About(QWidget *parent = nullptr);

  struct Person {
    Person(const QString &n, const QString &e = QString()) : name(n), email(e) {}
    bool operator<(const Person &other) const { return name < other.name; }
    QString name;
    QString email;
  };

 private:
  QString MainHtml() const;
  QString ContributorsHtml() const;
  QString PersonToHtml(const Person& person) const;

 private:
  Ui::About ui_;

  QList<Person> strawberry_authors_;
  QList<Person> strawberry_constributors_;
  QList<Person> strawberry_thanks_;
  QList<Person> clementine_authors_;
  QList<Person> clementine_constributors_;
};

#endif  // ABOUT_H
