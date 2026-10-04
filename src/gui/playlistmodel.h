#pragma once
#include "engine.h"
#include <QAbstractListModel>
#include <QStringList>
#include <atomic>

struct PlaylistEntry {
  QString path, title, author, format, metadataError;
  double seconds = -1;
};

// Only the GUI thread mutates this model. Import workers return plain entries.
class PlaylistModel : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
public:
  enum Role { Path = Qt::UserRole + 1, Title, Author, Format, Duration, Error };
  explicit PlaylistModel(QObject *parent = nullptr)
      : QAbstractListModel(parent) {}
  int rowCount(const QModelIndex &parent = {}) const override {
    return parent.isValid() ? 0 : m_entries.size();
  }
  QVariant data(const QModelIndex &, int role) const override;
  QHash<int, QByteArray> roleNames() const override;
  void append(const QList<PlaylistEntry> &);
  void remove(int);
  void move(int, int);
  void clear();
  void setRendered(int, const ay::Song &, double);
  const PlaylistEntry *entry(int row) const {
    return row >= 0 && row < m_entries.size() ? &m_entries[row] : nullptr;
  }
  static PlaylistEntry readMetadata(const QString &, const ay::Profile &);
signals:
  void countChanged();

private:
  QList<PlaylistEntry> m_entries;
};
