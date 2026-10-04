#include "controller.h"
#include <QApplication>
#include <QAudioDevice>
#include <QtAudio>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMediaDevices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QStandardPaths>
#include <cmath>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#endif
#include <QSaveFile>
#include <QTextStream>
#include <QWindow>
#include <QtConcurrent>
#include <algorithm>
#include <cstring>
#include <utility>
namespace {
std::filesystem::path nativePath(const QString &s) {
#ifdef Q_OS_WIN
  return std::filesystem::path(s.toStdWString());
#else
  return std::filesystem::path(s.toStdString());
#endif
}
} // namespace
Controller::Controller(QObject *parent, bool restorePlayback, const QString &sessionPath)
    : QObject(parent), m_playlist(this) {
  m_sessionPath = sessionPath.isEmpty()
      ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/session.json"
      : sessionPath;
  QSettings s;
  QJsonObject snapshot;
  QFile saved(m_sessionPath);
  if (saved.open(QIODevice::ReadOnly)) {
    auto document = QJsonDocument::fromJson(saved.readAll());
    if (document.isObject() && document.object().value("version").toInt() == 1)
      snapshot = document.object();
  }
  auto setting = [&](const QString &key, const QVariant &fallback = QVariant()) {
    return snapshot.contains(key) ? snapshot.value(key).toVariant() : s.value(key, fallback);
  };
  if (restorePlayback && !snapshot.isEmpty()) {
    m_restoreTrack = snapshot.value("activeTrack").toString();
    m_restoreSelection = snapshot.value("selectedTrack").toString();
    m_restorePosition = snapshot.value("position").toDouble();
    if (!std::isfinite(m_restorePosition) || m_restorePosition < 0)
      m_restorePosition = 0;
    m_restorePlaying = snapshot.value("playing").toBool();
  }
  QStringList restoredPaths;
  if (snapshot.contains("playlist")) {
    for (const auto &path : snapshot.value("playlist").toArray())
      if (path.isString()) restoredPaths.append(path.toString());
  } else restoredPaths = s.value("playlist").toStringList();
  m_volume = setting("volume", 0.8).toDouble();
  if (!std::isfinite(m_volume)) m_volume = 0.8;
  m_volume = std::clamp(m_volume, 0.0, 1.0);
  // Old snapshots/settings stored a linear sink gain. Preserve their audible
  // level while migrating the displayed/saved slider to a perceptual scale.
  const bool storedVolume = snapshot.contains("volume") || s.contains("volume");
  const QString volumeScale = snapshot.contains("volume")
      ? snapshot.value("volumeScale").toString() : s.value("volumeScale").toString();
  if (storedVolume && volumeScale != "logarithmic")
    m_volume = QtAudio::convertVolume(float(m_volume), QtAudio::LinearVolumeScale,
                                      QtAudio::LogarithmicVolumeScale);
  m_loop = setting("loop", false).toBool();
  m_profile.rate = setting("rate", 48000).toInt();
  m_profile.clock = setting("clock", 1773400).toInt();
  m_profile.ym = setting("ym", true).toBool();
  m_profile.preamp = setting("preamp", 127).toInt();
  m_profile.filter = setting("filter", 1).toInt();
  m_profile.useFileTiming = setting("fileTiming", true).toBool();
  m_profile.interruptHz = setting("interrupt", 50).toDouble();
  const auto gains = setting("gains").toList();
  if (gains.size() == 6)
    for (int i = 0; i < 6; ++i)
      m_profile.gains[i] = gains[i].toInt();
  try {
    m_profile.validate();
  } catch (...) {
    m_profile = {};
  }
  connect(&m_loader, &QFutureWatcher<Result>::finished, this, [this] {
    auto r = m_loader.result();
    m_loading = false;
    if (m_pendingTrack >= 0) {
      int target = std::exchange(m_pendingTrack, -1);
      load(target, m_pendingAutoplay, m_pendingRestore);
      return;
    }
    if (m_loadJob->cancel.load())
      r.error = "Cancelled";
    if (!r.error.isEmpty()) {
      m_error = r.error == "Cancelled" ? QString() : r.error;
      m_status = r.error == "Cancelled" ? "Loading cancelled"
                                        : "Could not load module";
      emit changed();
      if (m_autoplay && m_currentTrack + 1 < m_paths.size())
        load(m_currentTrack + 1, true);
      return;
    }
    m_session = std::move(r.session);
    m_position = std::clamp(m_restore, 0.0, duration());
    m_playlist.setRendered(m_currentTrack, m_session->song, duration());
    if (m_position > 0)
      m_session->seek(uint64_t(m_position * m_session->profile.rate));
    updateAnalysis();
    m_status = QString("%1 · %2 Hz · %3 · %4")
                   .arg(QString::fromStdString(m_session->song.format))
                   .arg(m_profile.rate)
                   .arg(m_profile.ym ? "YM2149F" : "AY-3-8910")
                   .arg(m_profile.filter ? "FIR" : "Averager");
    m_error.clear();
    emit changed();
    emit positionChanged();
    if (m_autoplay)
      startPlayback();
    if (!m_restoreSelection.isEmpty()) {
      int selected = m_paths.indexOf(m_restoreSelection);
      if (selected >= 0) m_selected = selected;
      m_restoreSelection.clear();
      emit changed();
    }
    saveSession();
    emit loaded();
  });
  connect(&m_exporter, &QFutureWatcher<QString>::finished, this, [this] {
    QString e = m_exporter.result();
    if (e == "Cancelled") {
      m_error.clear();
      m_status = "Export cancelled";
    } else if (!e.isEmpty())
      m_error = e;
    else
      m_status = "WAV export completed from the beginning";
    emit changed();
    emit exported(e);
  });
  connect(&m_importer, &QFutureWatcher<QList<PlaylistEntry>>::finished, this,
          [this] {
            QList<PlaylistEntry> added;
            if (!m_importJob->cancel.load()) {
              for (const auto &entry : m_importer.result()) {
                if (m_knownPaths.contains(entry.path))
                  continue;
                m_knownPaths.insert(entry.path);
                m_paths.append(entry.path);
                added.append(entry);
              }
              m_playlist.append(added);
              if (m_selected < 0 && !m_paths.isEmpty())
                m_selected = 0;
              if (!added.isEmpty())
                m_importStatus = QString("%1 tracks added · %2 total")
                                     .arg(added.size())
                                     .arg(m_paths.size());
              saveSession();
            } else
              m_importStatus = "Import cancelled";
            m_importActive = false;
            emit changed();
            emit imported(added.size());
            if (!m_preparePath.isEmpty()) {
              int row = m_paths.indexOf(m_preparePath);
              if (row >= 0) {
                m_preparePath.clear();
                load(row, false);
              }
            }
            if (m_restoring) {
              m_restoring = false;
              int row = m_paths.indexOf(m_restoreTrack);
              if (row >= 0) load(row, m_restorePlaying, m_restorePosition);
              else {
                int selected = m_paths.indexOf(m_restoreSelection);
                if (selected >= 0) m_selected = selected;
                m_restoreSelection.clear();
              }
              m_restoreTrack.clear();
            }
            startImport();
          });
  connect(&m_timer, &QTimer::timeout, this, &Controller::tick);
  m_timer.start(10);
  connect(&m_visualTimer, &QTimer::timeout, this, &Controller::updateAnalysis);
  m_visualTimer.start(33);
  auto *media = new QMediaDevices(this);
  connect(media, &QMediaDevices::audioOutputsChanged, this, [this] {
    if (m_playing) {
      pause();
      m_error = "Audio devices changed. Press Play to reconnect.";
      emit changed();
    }
  });
  connect(&m_sessionTimer, &QTimer::timeout, this, &Controller::saveSession);
  m_sessionTimer.setTimerType(Qt::PreciseTimer);
  m_sessionTimer.start(10000);
  connect(qApp, &QCoreApplication::aboutToQuit, this, &Controller::saveSession);
  m_pendingFiles = restoredPaths;
  m_restoring = restorePlayback && !restoredPaths.isEmpty();
  startImport();
}
Controller::~Controller() {
  saveSession();
  if (m_loadJob)
    m_loadJob->cancel = true;
  if (m_exportJob)
    m_exportJob->cancel = true;
  if (m_importJob)
    m_importJob->cancel = true;
  m_importer.waitForFinished();
  m_loader.waitForFinished();
  m_exporter.waitForFinished();
  resetSink();
}
void Controller::saveSession() {
  // Never replace the previous complete playlist with a partially restored one.
  if (m_restoring) return;
  if (m_playing && m_sink)
    m_position = std::min(duration(), m_base + m_sink->processedUSecs() / 1000000.0);
  const int active = m_pendingTrack >= 0 ? m_pendingTrack : m_currentTrack;
  const double position = m_pendingTrack >= 0 ? m_pendingRestore
      : (m_loading ? m_restore : m_position);
  const bool resume = m_pendingTrack >= 0 ? m_pendingAutoplay
      : (m_loading ? m_autoplay : playing());
  QJsonObject snapshot{
      {"version", 1}, {"playlist", QJsonArray::fromStringList(m_paths)},
      {"activeTrack", m_paths.value(active)}, {"selectedTrack", m_paths.value(m_selected)},
      {"position", position}, {"playing", resume}, {"volume", m_volume}, {"loop", m_loop},
      {"volumeScale", "logarithmic"},
      {"rate", m_profile.rate}, {"clock", m_profile.clock}, {"ym", m_profile.ym},
      {"preamp", m_profile.preamp}, {"filter", m_profile.filter},
      {"interrupt", m_profile.interruptHz}, {"fileTiming", m_profile.useFileTiming}};
  QJsonArray gains;
  for (int g : m_profile.gains) gains.append(g);
  snapshot.insert("gains", gains);
  QDir().mkpath(QFileInfo(m_sessionPath).absolutePath());
  QSaveFile file(m_sessionPath);
  const auto data = QJsonDocument(snapshot).toJson(QJsonDocument::Compact);
  if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
    m_error = "Could not save playback session: " + file.errorString();
    emit changed();
    return;
  }
#ifdef Q_OS_UNIX
  // Persist the directory entry after the atomic rename, as well as file data.
  const auto directory = QFile::encodeName(QFileInfo(m_sessionPath).absolutePath());
  const int fd = ::open(directory.constData(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd >= 0) { ::fsync(fd); ::close(fd); }
#endif
  // Keep legacy settings available for older builds and desktop preferences.
  QSettings settings;
  for (auto it = snapshot.begin(); it != snapshot.end(); ++it)
    if (it.key() != "playlist") settings.setValue(it.key(), it.value().toVariant());
  settings.setValue("playlist", m_paths);
  settings.sync();
}
void Controller::resetSink() {
  m_audioWasActive = false;
  m_playing = false;
  m_resumeAfterSeek = false;
  m_pendingAudio.clear();
  m_output = nullptr;
  if (m_sink) {
    m_sink->disconnect(this);
    m_sink->reset();
    delete m_sink;
    m_sink = nullptr;
  }
}
void Controller::load(int i, bool autoplay, double restore) {
  if (i < 0 || i >= m_paths.size())
    return;
  if (busy()) {
    m_pendingTrack = i;
    m_pendingAutoplay = autoplay;
    m_pendingRestore = restore;
    m_loadJob->cancel = true;
    m_selected = i;
    emit changed();
    return;
  }
  resetSink();
  m_selected = m_currentTrack = i;
  m_session.reset();
  m_pendingAudio.clear();
  m_waveform.clear();
  m_position = 0;
  m_autoplay = autoplay;
  m_restore = restore;
  m_error.clear();
  m_status = "Opening module…";
  auto job = std::make_shared<Job>();
  m_loadJob = job;
  auto path = m_paths[i];
  auto profile = m_profile;
  m_loading = true;
  m_loader.setFuture(QtConcurrent::run([job, path, profile] {
    Result result;
    try {
      ay::StreamRenderer engine(nativePath(path), profile, false, true);
      if (job->cancel.load())
        throw std::runtime_error("Cancelled");
      result.session = std::make_shared<PlaybackSession>(std::move(engine));
    } catch (const std::exception &e) {
      result.error = QString::fromUtf8(e.what());
    }
    return result;
  }));
  emit changed();
  emit positionChanged();
}
void Controller::startImport() {
  if (importing() || (m_pendingFiles.isEmpty() && m_pendingFolders.isEmpty()))
    return;
  auto files = std::exchange(m_pendingFiles, {});
  auto folders = std::exchange(m_pendingFolders, {});
  auto profile = m_profile;
  auto job = std::make_shared<Job>();
  m_importJob = job;
  m_importStatus = "Adding tracks…";
  m_importActive = true;
  m_importer.setFuture(QtConcurrent::run([files, folders, profile,
                                          job]() mutable {
    QList<PlaylistEntry> entries;
    QSet<QString> seen;
    for (const auto &folder : folders) {
      QDirIterator it(folder,
                      {"*.pt3", "*.PT3", "*.psg", "*.PSG", "*.ym", "*.YM"},
                      QDir::Files, QDirIterator::Subdirectories);
      while (it.hasNext()) {
        if (job->cancel.load())
          return entries;
        files.append(it.next());
      }
    }
    for (const auto &path : files) {
      if (job->cancel.load())
        break;
      QString absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
      if (seen.contains(absolute))
        continue;
      seen.insert(absolute);
      entries.append(PlaylistModel::readMetadata(absolute, profile));
    }
    return entries;
  }));
  emit changed();
}
void Controller::openFiles(const QList<QUrl> &urls) {
  for (const auto &u : urls) {
    if (!u.isLocalFile())
      continue;
    QString p = QDir::cleanPath(QFileInfo(u.toLocalFile()).absoluteFilePath());
    if (p.endsWith(".m3u", Qt::CaseInsensitive) ||
        p.endsWith(".m3u8", Qt::CaseInsensitive)) {
      loadPlaylist(u);
      continue;
    }
    if (!m_knownPaths.contains(p))
      m_pendingFiles.append(p);
  }
  startImport();
}
void Controller::openPath(const QString &path) {
  m_restoreTrack.clear();
  m_restoreSelection.clear();
  auto p = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  int row = m_paths.indexOf(p);
  if (row >= 0) {
    load(row, false);
    return;
  }
  m_preparePath = p;
  openFiles({QUrl::fromLocalFile(p)});
}
void Controller::openFolder(const QUrl &url) {
  if (!url.isLocalFile())
    return;
  m_pendingFolders.append(url.toLocalFile());
  startImport();
}
void Controller::select(int row) {
  if (row < 0 || row >= m_paths.size())
    return;
  m_selected = row;
  emit changed();
}
void Controller::activate(int row) { load(row, true); }
void Controller::clearPlaylist() {
  if (busy())
    return;
  m_restoring = false;
  m_restoreTrack.clear();
  m_restoreSelection.clear();
  if (m_importJob)
    m_importJob->cancel = true;
  m_pendingFiles.clear();
  m_pendingFolders.clear();
  m_preparePath.clear();
  stop();
  m_session.reset();
  m_pendingAudio.clear();
  m_waveform.clear();
  m_scope.clear();
  m_selected = m_currentTrack = -1;
  m_paths.clear();
  m_knownPaths.clear();
  m_playlist.clear();
  m_status = "Open a module to begin";
  m_error.clear();
  saveSession();
  emit changed();
}
void Controller::browse(const QString &purpose) {
  auto *dialog = new QFileDialog(nullptr);
  dialog->setAttribute(Qt::WA_DeleteOnClose);
  dialog->setWindowModality(Qt::ApplicationModal);
  dialog->setSupportedSchemes({"file"});
  dialog->setDirectory(
      QSettings().value("browseDirectory", QDir::homePath()).toString());
  if (purpose == "files") {
    dialog->setWindowTitle("Add music to playlist");
    dialog->setFileMode(QFileDialog::ExistingFiles);
    dialog->setNameFilters(
        {"AY modules / logs (*.pt3 *.PT3 *.psg *.PSG *.ym *.YM)",
         "Playlists (*.m3u *.m3u8)", "All files (*)"});
  } else if (purpose == "folder" || purpose == "batch") {
    dialog->setWindowTitle(purpose == "folder"
                               ? "Add music from folder and subfolders"
                               : "Export playlist to folder");
    dialog->setFileMode(QFileDialog::Directory);
    dialog->setOption(QFileDialog::ShowDirsOnly);
  } else if (purpose == "playlistOpen") {
    dialog->setWindowTitle("Open playlist");
    dialog->setFileMode(QFileDialog::ExistingFile);
    dialog->setNameFilter("M3U playlists (*.m3u *.m3u8)");
  } else if (purpose == "playlistSave" || purpose == "wav") {
    bool wav = purpose == "wav";
    dialog->setWindowTitle(wav ? "Export full song from beginning"
                               : "Save playlist");
    dialog->setAcceptMode(QFileDialog::AcceptSave);
    dialog->setFileMode(QFileDialog::AnyFile);
    dialog->setDefaultSuffix(wav ? "wav" : "m3u8");
    dialog->setNameFilter(wav ? "PCM WAV (*.wav)" : "M3U playlist (*.m3u8)");
  } else {
    delete dialog;
    return;
  }
  // Native dialogs remain enabled. QApplication loads the desktop theme's
  // dialog helper, including KDE's multi-file picker and Ctrl+A selection.
  connect(dialog, &QFileDialog::filesSelected, this,
          [this, purpose](const QStringList &files) {
            if (files.isEmpty())
              return;
            QSettings().setValue("browseDirectory",
                                 QFileInfo(files.first()).isDir()
                                     ? files.first()
                                     : QFileInfo(files.first()).absolutePath());
            QList<QUrl> urls;
            for (const auto &file : files)
              urls.append(QUrl::fromLocalFile(file));
            if (purpose == "files")
              openFiles(urls);
            else if (purpose == "folder")
              openFolder(urls.first());
            else if (purpose == "batch")
              exportPlaylistWav(urls.first());
            else if (purpose == "wav")
              exportWav(urls.first());
            else if (purpose == "playlistOpen")
              loadPlaylist(urls.first());
            else if (purpose == "playlistSave")
              savePlaylist(urls.first());
          });
  dialog->show();
}
void Controller::play() {
  if (busy())
    return;
  if (m_selected >= 0 && (!m_session || m_selected != m_currentTrack)) {
    load(m_selected, true);
    return;
  }
  startPlayback();
}
void Controller::startPlayback() {
  if (!m_session)
    return;
  if (m_playing)
    return;
  if (m_position >= duration()) {
    m_position = 0;
    m_session->seek(0);
  }
  if (m_sink && m_sink->state() == QtAudio::SuspendedState) {
    m_sink->resume();
    m_resumeAfterSeek = false;
    m_playing = true;
    emit changed();
    return;
  }
  if (!m_session->ready()) {
    m_resumeAfterSeek = true;
    emit changed();
    return;
  }
  resetSink();
  QAudioFormat f;
  f.setSampleRate(m_profile.rate);
  f.setChannelCount(2);
  f.setSampleFormat(QAudioFormat::Int16);
  auto device = QMediaDevices::defaultAudioOutput();
  if (device.isNull()) {
    m_error = "No audio output device is available";
    emit changed();
    return;
  }
  if (!device.isFormatSupported(f)) {
    m_error = "Audio device does not support the requested PCM format. Choose "
              "another sample rate in Mixer.";
    emit changed();
    return;
  }
  m_device = device.description() +
             QString(" · %1 Hz / 16-bit stereo").arg(f.sampleRate());
  m_sink = new QAudioSink(device, f, this);
  m_sink->setBufferSize(m_profile.rate * 4 / 10);
  m_sink->setVolume(QtAudio::convertVolume(float(m_volume),
      QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale));
  connect(m_sink, &QAudioSink::stateChanged, this,
          [this](QtAudio::State state) {
            if (state == QtAudio::ActiveState)
              m_audioWasActive = true;
            if (state == QtAudio::IdleState && m_audioWasActive && m_playing &&
                m_written < qint64(m_session->frames * 4)) {
              ++m_underruns;
              emit changed();
            }
            if (state == QtAudio::StoppedState && m_sink &&
                m_sink->error() != QtAudio::NoError) {
              m_playing = false;
              m_error = "Audio output failed (error " +
                        QString::number(m_sink->error()) + ")";
              emit changed();
            }
          });
  m_base = m_position;
  m_written = qint64(m_position * m_profile.rate) * 4;
  m_output = m_sink->start();
  if (!m_output) {
    m_error = "Could not start audio output";
    resetSink();
    emit changed();
    return;
  }
  m_playing = true;
  m_error.clear();
  tick();
  emit changed();
}
void Controller::pause() {
  if (m_resumeAfterSeek) {
    m_resumeAfterSeek = false;
    emit changed();
    return;
  }
  if (m_playing && m_sink) {
    tick();
    m_sink->suspend();
    m_playing = false;
  } else
    startPlayback();
  emit changed();
}
void Controller::stop() {
  if (busy()) {
    m_loadJob->cancel = true;
    m_pendingTrack = -1;
    m_autoplay = false;
    m_restore = 0;
  }
  resetSink();
  m_position = 0;
  if (m_session)
    m_session->seek(0);
  m_peakLeft = m_peakRight = 0;
  emit changed();
  emit positionChanged();
}
void Controller::seek(double seconds) {
  if (!m_session)
    return;
  bool resume = playing();
  resetSink();
  m_position = std::clamp(seconds, 0.0, duration());
  m_session->seek(uint64_t(m_position * m_session->profile.rate));
  updateAnalysis();
  emit positionChanged();
  emit changed();
  if (resume)
    startPlayback();
}
void Controller::next(int direction) {
  if (m_paths.isEmpty())
    return;
  int base = m_pendingTrack >= 0
                 ? m_pendingTrack
                 : (m_currentTrack >= 0 ? m_currentTrack : m_selected);
  int i = (base + direction + m_paths.size()) % m_paths.size();
  load(i, true);
}
void Controller::setVolume(double v) {
  if (!std::isfinite(v)) return;
  m_volume = std::clamp(v, 0.0, 1.0);
  if (m_sink)
    m_sink->setVolume(QtAudio::convertVolume(float(m_volume),
        QtAudio::LogarithmicVolumeScale, QtAudio::LinearVolumeScale));
  emit changed();
}
void Controller::tick() {
  if (m_session && !m_session->error().isEmpty()) {
    m_error = m_session->error();
    resetSink();
    emit changed();
  }
  if (m_resumeAfterSeek && m_session && m_session->ready())
    startPlayback();
  if (m_playing && m_sink && m_output && m_session) {
    qint64 remaining = qint64(m_session->frames * 4) - m_written;
    auto count =
        std::min({qint64(m_sink->bytesFree()), remaining, qint64(19200)});
    if (m_pendingAudio.isEmpty())
      count -= count % 4;
    if (count > 0) {
      if (m_pendingAudio.isEmpty())
        m_pendingAudio = m_session->take(count / 4);
      if (!m_pendingAudio.isEmpty()) {
        qint64 n =
            m_output->write(m_pendingAudio.constData(),
                            std::min<qint64>(count, m_pendingAudio.size()));
        if (n > 0) {
          m_written += n;
          m_pendingAudio.remove(0, n);
        } else if (n < 0) {
          m_error = "Audio write failed";
          resetSink();
          emit changed();
          return;
        }
      }
    }
    m_position =
        std::min(duration(), m_base + m_sink->processedUSecs() / 1000000.0);
    if (m_written >= qint64(m_session->frames * 4) &&
        m_sink->state() == QtAudio::IdleState) {
      m_position = duration();
      resetSink();
      if (m_loop) {
        m_position = 0;
        m_session->seek(0);
        startPlayback();
      } else if (m_currentTrack + 1 < m_paths.size())
        load(m_currentTrack + 1, true);
      emit changed();
    }
  }
  emit positionChanged();
}
void Controller::remove(int i) {
  if (busy() || i < 0 || i >= m_paths.size())
    return;
  if (i == m_currentTrack) {
    stop();
    m_session.reset();
    m_pendingAudio.clear();
    m_waveform.clear();
    m_currentTrack = -1;
  } else if (i < m_currentTrack)
    --m_currentTrack;
  m_knownPaths.remove(m_paths[i]);
  m_paths.removeAt(i);
  m_playlist.remove(i);
  if (i < m_selected)
    --m_selected;
  else if (i == m_selected)
    m_selected = m_paths.isEmpty() ? -1 : std::min(i, int(m_paths.size() - 1));
  saveSession();
  emit changed();
}
void Controller::move(int from, int to) {
  if (busy() || from < 0 || to < 0 || from >= m_paths.size() ||
      to >= m_paths.size())
    return;
  m_paths.move(from, to);
  m_playlist.move(from, to);
  auto adjust = [from, to](int &row) {
    if (row == from)
      row = to;
    else if (from < row && to >= row)
      --row;
    else if (from > row && to <= row)
      ++row;
  };
  adjust(m_selected);
  adjust(m_currentTrack);
  saveSession();
  emit changed();
}
void Controller::exportWav(const QUrl &url) {
  if (m_currentTrack < 0 || !m_session || busy() || exporting() ||
      !url.isLocalFile())
    return;
  auto path = m_paths[m_currentTrack];
  auto destination = url.toLocalFile();
  auto profile = m_profile;
  if (QFileInfo::exists(destination)) {
    m_error = "Export destination already exists. Choose a new filename.";
    emit changed();
    return;
  }
  auto job = std::make_shared<Job>();
  m_exportJob = job;
  m_exporter.setFuture(QtConcurrent::run([job, path, destination, profile] {
    try {
      auto r = ay::renderFile(nativePath(path), profile, [job](double p) {
        job->progress = p;
        return !job->cancel.load();
      });
      if (job->cancel.load())
        return QString("Cancelled");
      ay::writeWav(nativePath(destination), r);
      return QString();
    } catch (const std::exception &e) {
      return QString::fromUtf8(e.what());
    }
  }));
  emit changed();
}
void Controller::cancel() {
  m_pendingTrack = -1;
  m_autoplay = false;
  if (m_loadJob)
    m_loadJob->cancel = true;
  if (m_exportJob)
    m_exportJob->cancel = true;
  if (m_importJob)
    m_importJob->cancel = true;
  m_pendingFiles.clear();
  m_pendingFolders.clear();
  m_preparePath.clear();
}
void Controller::applySettings(bool ym, int clock, double interrupt, int rate,
                               int preamp, bool filter,
                               const QVariantList &gains, bool fileTiming) {
  if (busy()) {
    m_error = "Wait for loading to finish before changing mixer settings.";
    emit changed();
    return;
  }
  ay::Profile p = m_profile;
  p.useFileTiming = fileTiming;
  p.ym = ym;
  p.clock = clock;
  p.interruptHz = interrupt;
  p.rate = rate;
  p.preamp = preamp;
  p.filter = filter ? 1 : 0;
  if (gains.size() != 6) {
    m_error = "Six channel coefficients are required";
    emit changed();
    return;
  }
  for (int i = 0; i < 6; ++i)
    p.gains[i] = gains[i].toInt();
  try {
    p.validate();
  } catch (const std::exception &e) {
    m_error = e.what();
    emit changed();
    return;
  }
  bool resume = playing();
  double pos = m_position;
  m_profile = p;
  saveSession();
  if (m_currentTrack >= 0)
    load(m_currentTrack, resume, pos);
  emit changed();
}
void Controller::savePlaylist(const QUrl &url) {
  QSaveFile f(url.toLocalFile());
  if (!f.open(QIODevice::WriteOnly)) {
    m_error = f.errorString();
    emit changed();
    return;
  }
  QTextStream s(&f);
  s << "#EXTM3U\n";
  for (const auto &p : m_paths)
    s << p << '\n';
  s.flush();
  if (!f.commit())
    m_error = f.errorString();
  emit changed();
}
void Controller::loadPlaylist(const QUrl &url) {
  QFile f(url.toLocalFile());
  if (!f.open(QIODevice::ReadOnly)) {
    m_error = f.errorString();
    emit changed();
    return;
  }
  QTextStream s(&f);
  auto dir = QFileInfo(f).absoluteDir();
  QList<QUrl> urls;
  while (!s.atEnd()) {
    auto line = s.readLine().trimmed();
    if (line.isEmpty() || line.startsWith('#'))
      continue;
    auto path =
        QFileInfo(line).isAbsolute() ? line : dir.absoluteFilePath(line);
    urls.append(QUrl::fromLocalFile(path));
  }
  for (const auto &url : urls) {
    auto p = QDir::cleanPath(QFileInfo(url.toLocalFile()).absoluteFilePath());
    if (!m_knownPaths.contains(p))
      m_pendingFiles.append(p);
  }
  startImport();
}

void Controller::exportPlaylistWav(const QUrl &folder) {
  if (busy() || exporting() || m_paths.isEmpty() || !folder.isLocalFile())
    return;
  auto paths = m_paths;
  auto destination = folder.toLocalFile();
  auto profile = m_profile;
  auto job = std::make_shared<Job>();
  m_exportJob = job;
  m_exporter.setFuture(QtConcurrent::run([job, paths, destination, profile] {
    QStringList failures;
    int completed = 0;
    for (int i = 0; i < paths.size(); ++i) {
      if (job->cancel.load())
        return QString("Cancelled");
      QString name = QFileInfo(paths[i]).completeBaseName();
      QString target = QDir(destination).filePath(name + ".wav");
      int suffix = 2;
      while (QFileInfo::exists(target) && suffix < 10000)
        target = QDir(destination)
                     .filePath(name + "-" + QString::number(suffix++) + ".wav");
      try {
        auto r = ay::renderFile(nativePath(paths[i]), profile,
                                [job, i, total = paths.size()](double p) {
                                  job->progress = (i + p) / total;
                                  return !job->cancel.load();
                                });
        ay::writeWav(nativePath(target), r);
        ++completed;
      } catch (const std::exception &e) {
        if (job->cancel.load())
          return QString("Cancelled");
        failures.append(QFileInfo(paths[i]).fileName() + ": " +
                        QString::fromUtf8(e.what()));
      }
    }
    if (failures.isEmpty())
      return QString();
    return QString("Batch export: %1 completed, %2 failed.\n%3")
        .arg(completed)
        .arg(failures.size())
        .arg(failures.join('\n'));
  }));
  m_status = "Exporting playlist from the beginning…";
  emit changed();
}

void Controller::updateAnalysis() {
  if (!m_session)
    return;
  auto analysis =
      m_session->analysis(uint64_t(m_position * m_session->profile.rate));
  m_peakLeft = analysis.left;
  m_peakRight = analysis.right;
  m_scope.clear();
  m_channelScopes.clear();
  for (int v = 0; v < m_session->voiceCount; ++v) {
    QVariantList points;
    points.reserve(analysis.scopes[v].size());
    for (float point : analysis.scopes[v])
      points.append(point);
    m_channelScopes.append(QVariant(points));
    if (v == 0)
      m_scope = points;
  }
  m_waveform = m_session->waveform();
  emit visualChanged();
}
