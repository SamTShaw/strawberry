/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
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

#include <QtGlobal>
#include <QWidget>
#include <QVariant>
#include <QString>
#include <QStringBuilder>
#include <QSize>
#include <QLabel>
#include <QSettings>
#include <QtEvents>

#include "core/timeconstants.h"
#include "core/utilities.h"
#include "trackslider.h"
#include "ui_trackslider.h"
#include "clickablelabel.h"
#include "tracksliderslider.h"

#ifdef HAVE_MOODBAR
#  include "moodbar/moodbarproxystyle.h"
#endif

const char* TrackSlider::kSettingsGroup = "MainWindow";

TrackSlider::TrackSlider(QWidget* parent)
    : QWidget(parent),
      ui_(new Ui_TrackSlider),
#ifdef HAVE_MOODBAR
      moodbar_style_(nullptr),
#endif
      setting_value_(false),
      show_remaining_time_(true),
    slider_maximum_value_(0)
{
  ui_->setupUi(this);

  UpdateLabelWidth();

  // Load settings
  QSettings s;
  s.beginGroup(kSettingsGroup);
  show_remaining_time_ = s.value("show_remaining_time").toBool();

  connect(ui_->slider, SIGNAL(sliderMoved(int)), SIGNAL(ValueChanged(int)));
  connect(ui_->slider, SIGNAL(valueChanged(int)), SLOT(ValueMaybeChanged(int)));
  connect(ui_->remaining, SIGNAL(Clicked()), SLOT(ToggleTimeDisplay()));
  connect(ui_->slider, SIGNAL(SeekForward()), SIGNAL(SeekForward()));
  connect(ui_->slider, SIGNAL(SeekBackward()), SIGNAL(SeekBackward()));
  connect(ui_->slider, SIGNAL(Previous()), SIGNAL(Previous()));
  connect(ui_->slider, SIGNAL(Next()), SIGNAL(Next()));

}

TrackSlider::~TrackSlider() {
  delete ui_;
#ifdef HAVE_MOODBAR
  if (moodbar_style_) moodbar_style_->deleteLater();
#endif
}

void TrackSlider::SetApplication(Application* app) {
#ifdef HAVE_MOODBAR
  if (!moodbar_style_) moodbar_style_ = new MoodbarProxyStyle(app, ui_->slider);
#endif
}

void TrackSlider::UpdateLabelWidth() {
  // We set the label's minimum size so it won't resize itself when the user is dragging the slider.
  UpdateLabelWidth(ui_->elapsed, "0:00:00");
  UpdateLabelWidth(ui_->remaining, "-0:00:00");
}

void TrackSlider::UpdateLabelWidth(QLabel* label, const QString& text) {

  QString old_text = label->text();
  label->setText(text);
  label->setMinimumWidth(0);
  int width = label->sizeHint().width();
  label->setText(old_text);

  label->setMinimumWidth(width);

}

QSize TrackSlider::sizeHint() const {

  int width = 500;
  width += ui_->elapsed->sizeHint().width();
  width += ui_->remaining->sizeHint().width();

  int height = qMax(ui_->slider->sizeHint().height(), ui_->elapsed->sizeHint().height());

  return QSize(width, height);

}

void TrackSlider::SetValue(int elapsed, int total) {

  setting_value_ = true; // This is so we don't emit from QAbstractSlider::valueChanged
  ui_->slider->setMaximum(total);
  if (!ui_->slider->isSliderDown()) {
    ui_->slider->setValue(elapsed);
  }

  setting_value_ = false;

  UpdateTimes(elapsed / kMsecPerSec);

}

void TrackSlider::UpdateTimes(int elapsed) {

  ui_->elapsed->setText(Utilities::PrettyTime(elapsed));
  // Update normally if showing remaining time
  if (show_remaining_time_) {
    ui_->remaining->setText("-" + Utilities::PrettyTime((ui_->slider->maximum() / kMsecPerSec) - elapsed));
  }
  else {
    // Check if slider maximum value is changed before updating
    if (slider_maximum_value_ != ui_->slider->maximum() || !ui_->slider->isEnabled()) {
      slider_maximum_value_ = ui_->slider->maximum();
      ui_->remaining->setText(Utilities::PrettyTime((ui_->slider->maximum() / kMsecPerSec)));
    }
  }
  setEnabled(true);

}

void TrackSlider::SetStopped() {

  setEnabled(false);
  ui_->elapsed->setText("0:00:00");
  ui_->remaining->setText("0:00:00");

  setting_value_ = true;
  ui_->slider->setValue(0);
  slider_maximum_value_ = 0;
  setting_value_ = false;

}

void TrackSlider::SetCanSeek(bool can_seek) {
  ui_->slider->setEnabled(can_seek);
}

void TrackSlider::Seek(int gap) {
  if (ui_->slider->isEnabled())
    ui_->slider->setValue(ui_->slider->value() + gap * kMsecPerSec);
}

void TrackSlider::ValueMaybeChanged(int value) {
  if (setting_value_) return;

  UpdateTimes(value / kMsecPerSec);
  emit ValueChangedSeconds(value / kMsecPerSec);
}

bool TrackSlider::event(QEvent* e) {

  switch (e->type()) {
    case QEvent::ApplicationFontChange:
    case QEvent::StyleChange:
      UpdateLabelWidth();
      break;
    default:
      break;
  }
  return false;

}

void TrackSlider::ToggleTimeDisplay() {

  show_remaining_time_ = !show_remaining_time_;
  if (!show_remaining_time_) {
    // We set the value to -1 because the label must be updated
    slider_maximum_value_ = -1;
  }
  UpdateTimes(ui_->slider->value() / kMsecPerSec);

  // Save this setting
  QSettings s;
  s.beginGroup(kSettingsGroup);
  s.setValue("show_remaining_time", show_remaining_time_);

}
