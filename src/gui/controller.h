#pragma once
#include "engine.h"
#include "playbacksession.h"
#include "playlistmodel.h"
#include <QAudioSink>
#include <QFutureWatcher>
#include <QObject>
#include <QSet>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <atomic>
#include <memory>
class Controller : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString title READ title NOTIFY changed)
  Q_PROPERTY(QString author READ author NOTIFY changed)
  Q_PROPERTY(QString status READ status NOTIFY changed)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString device READ device NOTIFY changed)
  Q_PROPERTY(int underruns READ underruns NOTIFY changed)
  Q_PROPERTY(double duration READ duration NOTIFY changed)
  Q_PROPERTY(double position READ position NOTIFY positionChanged)
  Q_PROPERTY(bool playing READ playing NOTIFY changed)
  Q_PROPERTY(bool busy READ busy NOTIFY changed)
  Q_PROPERTY(bool exporting READ exporting NOTIFY changed)
  Q_PROPERTY(double progress READ progress NOTIFY positionChanged)
  Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY changed)
  Q_PROPERTY(bool loop READ loop WRITE setLoop NOTIFY changed)
  Q_PROPERTY(QStringList playlist READ playlist NOTIFY changed)
  Q_PROPERTY(int selected READ selected NOTIFY changed)
  Q_PROPERTY(int currentTrack READ currentTrack NOTIFY changed)
  Q_PROPERTY(QAbstractItemModel *playlistModel READ playlistModel CONSTANT)
  Q_PROPERTY(bool importing READ importing NOTIFY changed)
  Q_PROPERTY(QString importStatus READ importStatus NOTIFY changed)
  Q_PROPERTY(QVariantList waveform READ waveform NOTIFY visualChanged)
  Q_PROPERTY(QVariantList scope READ scope NOTIFY visualChanged)
  Q_PROPERTY(QVariantList channelScopes READ channelScopes NOTIFY visualChanged)
  Q_PROPERTY(int voiceCount READ voiceCount NOTIFY changed)
  Q_PROPERTY(double peakLeft READ peakLeft NOTIFY visualChanged)
  Q_PROPERTY(double peakRight READ peakRight NOTIFY visualChanged)
  Q_PROPERTY(int clock READ clock NOTIFY changed)
  Q_PROPERTY(double interruptHz READ interruptHz NOTIFY changed)
  Q_PROPERTY(int preamp READ preamp NOTIFY changed)
  Q_PROPERTY(QVariantList gains READ gains NOTIFY changed)
  Q_PROPERTY(bool fileTiming READ fileTiming NOTIFY changed)
  Q_PROPERTY(int rate READ rate NOTIFY changed)
  Q_PROPERTY(bool ym READ ym NOTIFY changed)
  Q_PROPERTY(bool filtered READ filtered NOTIFY changed)
  struct Job {
    std::atomic_bool cancel = false;
    std::atomic<double> progress = 0;
  };
  struct Result {
    std::shared_ptr<PlaybackSession> session;
    QString error;
  };
  std::shared_ptr<PlaybackSession> m_session;
  QByteArray m_pendingAudio;
  QAudioSink *m_sink = nullptr;
  QIODevice *m_output = nullptr;
  QFutureWatcher<Result> m_loader;
  QFutureWatcher<QString> m_exporter;
  QFutureWatcher<QList<PlaylistEntry>> m_importer;
  PlaylistModel m_playlist;
  QSet<QString> m_knownPaths;
  QStringList m_pendingFiles, m_pendingFolders;
  QString m_preparePath, m_importStatus;
  std::shared_ptr<Job> m_importJob;
  std::shared_ptr<Job> m_loadJob, m_exportJob;
  QTimer m_timer, m_visualTimer, m_sessionTimer;
  QString m_sessionPath, m_restoreTrack, m_restoreSelection;
  double m_restorePosition = 0;
  bool m_restorePlaying = false, m_restoring = false;
  ay::Profile m_profile;
  QStringList m_paths;
  int m_selected = -1, m_currentTrack = -1, m_pendingTrack = -1;
  bool m_pendingAutoplay = false;
  double m_pendingRestore = 0;
  bool m_importActive = false, m_loading = false;
  QString m_status = "Open a module to begin", m_error, m_device;
  double m_position = 0, m_base = 0, m_volume = 0.8, m_restore = 0;
  qint64 m_written = 0;
  int m_underruns = 0;
  bool m_audioWasActive = false;
  bool m_playing = false, m_loop = false, m_autoplay = false,
       m_resumeAfterSeek = false;
  QVariantList m_waveform, m_scope, m_channelScopes;
  double m_peakLeft = 0, m_peakRight = 0;
  void updateAnalysis();
  void resetSink();
  void tick();
  void load(int, bool, double = 0);
  void saveSession();
  void startPlayback();
  void startImport();

public:
  explicit Controller(QObject *parent = nullptr, bool restorePlayback = true,
                      const QString &sessionPath = {});
  ~Controller();
  QString title() const {
    return m_session ? QString::fromLocal8Bit(m_session->song.title.c_str())
                     : (m_playlist.entry(m_selected)
                            ? m_playlist.entry(m_selected)->title
                            : "C++Ay");
  }
  QString author() const {
    return m_session ? QString::fromLocal8Bit(m_session->song.author.c_str())
                     : (m_playlist.entry(m_selected)
                            ? m_playlist.entry(m_selected)->author
                            : "Native AY / YM playback");
  }
  QString status() const { return m_status; }
  QString error() const { return m_error; }
  QString device() const { return m_device; }
  int underruns() const { return m_underruns; }
  double duration() const {
    return m_session ? double(m_session->frames) / m_session->profile.rate : 0;
  }
  double position() const { return m_position; }
  bool playing() const { return m_playing || m_resumeAfterSeek; }
  bool busy() const { return m_loading; }
  bool exporting() const { return m_exporter.isRunning(); }
  double progress() const {
    return busy() && m_loadJob          ? m_loadJob->progress.load()
           : exporting() && m_exportJob ? m_exportJob->progress.load()
                                        : 0;
  }
  // Perceptual slider position; convert only when applying to the audio sink.
  double volume() const { return m_volume; }
  void setVolume(double);
  bool loop() const { return m_loop; }
  void setLoop(bool l) {
    m_loop = l;
    emit changed();
  }
  QStringList playlist() const { return m_paths; }
  int selected() const { return m_selected; }
  int currentTrack() const { return m_currentTrack; }
  QAbstractItemModel *playlistModel() { return &m_playlist; }
  bool importing() const { return m_importActive; }
  QString importStatus() const { return m_importStatus; }
  QVariantList waveform() const { return m_waveform; }
  QVariantList scope() const { return m_scope; }
  QVariantList channelScopes() const { return m_channelScopes; }
  int voiceCount() const { return m_session ? m_session->voiceCount : 3; }
  double peakLeft() const { return m_peakLeft; }
  double peakRight() const { return m_peakRight; }
  int clock() const { return m_profile.clock; }
  double interruptHz() const {
    return m_session ? m_session->profile.interruptHz : m_profile.interruptHz;
  }
  bool fileTiming() const { return m_profile.useFileTiming; }
  int preamp() const { return m_profile.preamp; }
  QVariantList gains() const {
    QVariantList v;
    for (int g : m_profile.gains)
      v.append(g);
    return v;
  }
  int rate() const { return m_profile.rate; }
  bool ym() const { return m_profile.ym; }
  bool filtered() const { return m_profile.filter != 0; }
  Q_INVOKABLE void openFiles(const QList<QUrl> &);
  Q_INVOKABLE void openPath(const QString &);
  Q_INVOKABLE void openFolder(const QUrl &);
  Q_INVOKABLE void select(int);
  Q_INVOKABLE void activate(int);
  Q_INVOKABLE void clearPlaylist();
  Q_INVOKABLE void browse(const QString &purpose);
  Q_INVOKABLE void play();
  Q_INVOKABLE void pause();
  Q_INVOKABLE void stop();
  Q_INVOKABLE void seek(double);
  Q_INVOKABLE void next(int direction = 1);
  Q_INVOKABLE void remove(int);
  Q_INVOKABLE void move(int, int);
  Q_INVOKABLE void exportWav(const QUrl &);
  Q_INVOKABLE void exportPlaylistWav(const QUrl &);
  Q_INVOKABLE void cancel();
  Q_INVOKABLE void applySettings(bool ym, int clock, double interrupt, int rate,
                                 int preamp, bool filter,
                                 const QVariantList &gains,
                                 bool fileTiming = true);
  Q_INVOKABLE void savePlaylist(const QUrl &);
  Q_INVOKABLE void loadPlaylist(const QUrl &);
  Q_INVOKABLE void clearError() {
    m_error.clear();
    emit changed();
  }
signals:
  void changed();
  void positionChanged();
  void visualChanged();
  void loaded();
  void imported(int count);
  void exported(const QString &);
};
