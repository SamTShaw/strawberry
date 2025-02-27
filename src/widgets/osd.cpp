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

#include "config.h"

#ifdef HAVE_DBUS
# include <dbus/notification.h>
#endif

#include <QObject>
#include <QCoreApplication>
#include <QList>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QRegExp>
#include <QImage>
#include <QSettings>
#ifdef HAVE_DBUS
#  include <QDBusPendingCall>
#endif

#include "osd.h"
#include "osdpretty.h"

#include "core/application.h"
#include "core/logging.h"
#include "core/systemtrayicon.h"
#include "covermanager/currentalbumcoverloader.h"

const char *OSD::kSettingsGroup = "OSD";

OSD::OSD(SystemTrayIcon *tray_icon, Application *app, QObject *parent)
    : QObject(parent),
      app_(app),
      tray_icon_(tray_icon),
      app_name_(QCoreApplication::applicationName()),
      timeout_msec_(5000),
      behaviour_(Native),
      show_on_volume_change_(false),
      show_art_(true),
      show_on_play_mode_change_(true),
      show_on_pause_(true),
      show_on_resume_(false),
      use_custom_text_(false),
      custom_text1_(QString()),
      custom_text2_(QString()),
      preview_mode_(false),
      force_show_next_(false),
      ignore_next_stopped_(false),
      pretty_popup_(new OSDPretty(OSDPretty::Mode_Popup))
  {

  connect(app_->current_albumcover_loader(), SIGNAL(ThumbnailLoaded(Song, QUrl, QImage)), SLOT(AlbumCoverLoaded(Song, QUrl, QImage)));

  ReloadSettings();
  Init();

  app_name_[0] = app_name_[0].toUpper();

}

OSD::~OSD() {
  delete pretty_popup_;
}

void OSD::ReloadSettings() {

  QSettings s;
  s.beginGroup(kSettingsGroup);
  behaviour_ = OSD::Behaviour(s.value("Behaviour", Native).toInt());
  timeout_msec_ = s.value("Timeout", 5000).toInt();
  show_on_volume_change_ = s.value("ShowOnVolumeChange", false).toBool();
  show_art_ = s.value("ShowArt", true).toBool();
  show_on_play_mode_change_ = s.value("ShowOnPlayModeChange", true).toBool();
  show_on_pause_ = s.value("ShowOnPausePlayback", true).toBool();
  show_on_resume_ = s.value("ShowOnResumePlayback", false).toBool();
  use_custom_text_ = s.value(("CustomTextEnabled"), false).toBool();
  custom_text1_ = s.value("CustomText1").toString();
  custom_text2_ = s.value("CustomText2").toString();
  s.endGroup();

  if (!SupportsNativeNotifications() && behaviour_ == Native)
    behaviour_ = Pretty;
  if (!SupportsTrayPopups() && behaviour_ == TrayPopup)
    behaviour_ = Disabled;

  ReloadPrettyOSDSettings();

}

// Reload just Pretty OSD settings, not everything
void OSD::ReloadPrettyOSDSettings() {
  pretty_popup_->set_popup_duration(timeout_msec_);
  pretty_popup_->ReloadSettings();
}

void OSD::ReshowCurrentSong() {
  force_show_next_ = true;
  AlbumCoverLoaded(last_song_, last_image_uri_, last_image_);
}

void OSD::AlbumCoverLoaded(const Song &song, const QUrl &cover_url, const QImage &image) {

  // Don't change tray icon details if it's a preview
  if (!preview_mode_ && tray_icon_)
    tray_icon_->SetNowPlaying(song, cover_url);

  last_song_ = song;
  last_image_ = image;
  last_image_uri_ = cover_url;

  QStringList message_parts;
  QString summary;
  if (!use_custom_text_) {
    summary = song.PrettyTitle();
    if (!song.artist().isEmpty())
      summary = QString("%1 - %2").arg(song.artist(), summary);
    if (!song.album().isEmpty())
      message_parts << song.album();
    if (song.disc() > 0)
      message_parts << tr("disc %1").arg(song.disc());
    if (song.track() > 0)
      message_parts << tr("track %1").arg(song.track());
  }
  else {
    QRegExp variable_replacer("[%][a-z]+[%]");
    summary = custom_text1_;
    QString message(custom_text2_);

    // Replace the first line
    int pos = 0;
    variable_replacer.indexIn(custom_text1_);
    while ((pos = variable_replacer.indexIn(custom_text1_, pos)) != -1) {
      QStringList captured = variable_replacer.capturedTexts();
      summary.replace(captured[0], ReplaceVariable(captured[0], song));
      pos += variable_replacer.matchedLength();
    }

    // Replace the second line
    pos = 0;
    variable_replacer.indexIn(custom_text2_);
    while ((pos = variable_replacer.indexIn(custom_text2_, pos)) != -1) {
      QStringList captured = variable_replacer.capturedTexts();
      message.replace(captured[0], ReplaceVariable(captured[0], song));
      pos += variable_replacer.matchedLength();
    }
    message_parts << message;
  }

  if (show_art_) {
    ShowMessage(summary, message_parts.join(", "), "notification-audio-play", image);
  }
  else {
    ShowMessage(summary, message_parts.join(", "), "notification-audio-play", QImage());
  }

  // Reload the saved settings if they were changed for preview
  if (preview_mode_) {
    ReloadSettings();
    preview_mode_ = false;
  }
}

void OSD::Paused() {
  if (show_on_pause_) {
    ShowMessage(app_name_, tr("Paused"));
  }
}

void OSD::Resumed() {
  if (show_on_resume_) {
    AlbumCoverLoaded(last_song_, last_image_uri_, last_image_);
  }
}

void OSD::Stopped() {
  if (tray_icon_) tray_icon_->ClearNowPlaying();
  if (ignore_next_stopped_) {
    ignore_next_stopped_ = false;
    return;
  }

  ShowMessage(app_name_, tr("Stopped"));
}

void OSD::StopAfterToggle(bool stop) {
  ShowMessage(app_name_, tr("Stop playing after track: %1").arg(stop ? tr("On") : tr("Off")));
}

void OSD::PlaylistFinished() {
  // We get a PlaylistFinished followed by a Stopped from the player
  ignore_next_stopped_ = true;

  ShowMessage(app_name_, tr("Playlist finished"));
}

void OSD::VolumeChanged(int value) {
  if (!show_on_volume_change_) return;

  ShowMessage(app_name_, tr("Volume %1%").arg(value));
}

void OSD::ShowMessage(const QString &summary, const QString &message, const QString icon, const QImage &image) {

  if (pretty_popup_->toggle_mode()) {
    pretty_popup_->ShowMessage(summary, message, image);
  }
  else {
    switch (behaviour_) {
      case Native:
        if (image.isNull()) {
          ShowMessageNative(summary, message, icon, QImage());
        }
        else {
          ShowMessageNative(summary, message, QString(), image);
        }
        break;

#ifndef Q_OS_MACOS
      case TrayPopup:
        if (tray_icon_) tray_icon_->ShowPopup(summary, message, timeout_msec_);
        break;
#endif

      case Disabled:
        if (!force_show_next_) break;
        force_show_next_ = false;
      // fallthrough
      case Pretty:
        pretty_popup_->ShowMessage(summary, message, image);
        break;

      default:
        break;
    }
  }

}

#if !defined(HAVE_X11) && defined(HAVE_DBUS)
void OSD::CallFinished(QDBusPendingCallWatcher*) {}
#endif

void OSD::ShuffleModeChanged(PlaylistSequence::ShuffleMode mode) {
  if (show_on_play_mode_change_) {
    QString current_mode = QString();
    switch (mode) {
      case PlaylistSequence::Shuffle_Off:         current_mode = tr("Don't shuffle");   break;
      case PlaylistSequence::Shuffle_All:         current_mode = tr("Shuffle all");     break;
      case PlaylistSequence::Shuffle_InsideAlbum: current_mode = tr("Shuffle tracks in this album"); break;
      case PlaylistSequence::Shuffle_Albums:      current_mode = tr("Shuffle albums");  break;
    }
    ShowMessage(app_name_, current_mode);
  }
}

void OSD::RepeatModeChanged(PlaylistSequence::RepeatMode mode) {
  if (show_on_play_mode_change_) {
    QString current_mode = QString();
    switch (mode) {
      case PlaylistSequence::Repeat_Off:      current_mode = tr("Don't repeat");   break;
      case PlaylistSequence::Repeat_Track:    current_mode = tr("Repeat track");   break;
      case PlaylistSequence::Repeat_Album:    current_mode = tr("Repeat album"); break;
      case PlaylistSequence::Repeat_Playlist: current_mode = tr("Repeat playlist"); break;
      case PlaylistSequence::Repeat_OneByOne: current_mode = tr("Stop after every track"); break;
      case PlaylistSequence::Repeat_Intro: current_mode = tr("Intro tracks"); break;
    }
    ShowMessage(app_name_, current_mode);
  }
}

QString OSD::ReplaceVariable(const QString &variable, const Song &song) {

  QString return_value;
  if (variable == "%artist%") {
    return song.artist();
  }
  else if (variable == "%album%") {
    return song.album();
  }
  else if (variable == "%title%") {
    return song.PrettyTitle();
  }
  else if (variable == "%albumartist%") {
    return song.effective_albumartist();
  }
  else if (variable == "%year%") {
    return song.PrettyYear();
  }
  else if (variable == "%composer%") {
    return song.composer();
  }
  else if (variable == "%performer%") {
    return song.performer();
  }
  else if (variable == "%grouping%") {
    return song.grouping();
  }
  else if (variable == "%length%") {
    return song.PrettyLength();
  }
  else if (variable == "%disc%") {
    return return_value.setNum(song.disc());
  }
  else if (variable == "%track%") {
    return return_value.setNum(song.track());
  }
  else if (variable == "%genre%") {
    return song.genre();
  }
  else if (variable == "%playcount%") {
    return return_value.setNum(song.playcount());
  }
  else if (variable == "%skipcount%") {
    return return_value.setNum(song.skipcount());
  }
  else if (variable == "%filename%") {
    return song.basefilename();
  }
  else if (variable == "%newline%") {
    // We need different strings depending on notification type
    switch (behaviour_) {
      case Native:
#ifdef Q_OS_MACOS
        return "\n";
#endif
#ifdef Q_OS_LINUX
        return "<br/>";
#endif
#ifdef Q_OS_WIN32
        // Other OS don't support native notifications
        qLog(Debug) << "New line not supported by this notification type under Windows";
        return "";
#endif
      case TrayPopup:
        qLog(Debug) << "New line not supported by this notification type";
        return "";
      case Pretty:
      default:
        // When notifications are disabled, we force the PrettyOSD
        return "<br/>";
    }
  }

  //if the variable is not recognized, just return it
  return variable;
}

void OSD::ShowPreview(const Behaviour type, const QString &line1, const QString &line2, const Song &song) {

  behaviour_ = type;
  custom_text1_ = line1;
  custom_text2_ = line2;
  if (!use_custom_text_) use_custom_text_ = true;

  // We want to reload the settings, but we can't do this here because the cover art loading is asynch
  preview_mode_ = true;
  AlbumCoverLoaded(song, QUrl(), QImage());

}

void OSD::SetPrettyOSDToggleMode(bool toggle) {
  pretty_popup_->set_toggle_mode(toggle);
}

