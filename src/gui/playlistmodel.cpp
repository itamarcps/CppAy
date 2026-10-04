#include "playlistmodel.h"
#include "pt3.h"
#include <QFile>
#include <QFileInfo>

QVariant PlaylistModel::data(const QModelIndex &index, int role) const {
  auto *e = entry(index.row());
  if (!index.isValid() || !e)
    return {};
  switch (role) {
  case Qt::DisplayRole:
  case Title:
    return e->title;
  case Path:
    return e->path;
  case Author:
    return e->author;
  case Format:
    return e->format;
  case Duration:
    return e->seconds;
  case Error:
    return e->metadataError;
  default:
    return {};
  }
}
QHash<int, QByteArray> PlaylistModel::roleNames() const {
  return {{Path, "filePath"},          {Title, "trackTitle"},
          {Author, "trackAuthor"},     {Format, "trackFormat"},
          {Duration, "trackDuration"}, {Error, "metadataError"}};
}
void PlaylistModel::append(const QList<PlaylistEntry> &entries) {
  if (entries.isEmpty())
    return;
  beginInsertRows({}, m_entries.size(), m_entries.size() + entries.size() - 1);
  m_entries.append(entries);
  endInsertRows();
  emit countChanged();
}
void PlaylistModel::remove(int row) {
  beginRemoveRows({}, row, row);
  m_entries.removeAt(row);
  endRemoveRows();
  emit countChanged();
}
void PlaylistModel::move(int from, int to) {
  if (from == to)
    return;
  beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
  m_entries.move(from, to);
  endMoveRows();
}
void PlaylistModel::clear() {
  if (m_entries.isEmpty())
    return;
  beginResetModel();
  m_entries.clear();
  endResetModel();
  emit countChanged();
}
void PlaylistModel::setRendered(int row, const ay::Song &song, double seconds) {
  if (row < 0 || row >= m_entries.size())
    return;
  auto &e = m_entries[row];
  auto text = QString::fromLocal8Bit(song.title.c_str()).trimmed();
  if (!text.isEmpty())
    e.title = text;
  e.author = QString::fromLocal8Bit(song.author.c_str()).trimmed();
  e.format = QString::fromStdString(song.format);
  e.seconds = seconds;
  e.metadataError.clear();
  emit dataChanged(index(row), index(row));
}
PlaylistEntry PlaylistModel::readMetadata(const QString &path,
                                          const ay::Profile &profile) {
  PlaylistEntry e;
  e.path = path;
  e.title = QFileInfo(path).completeBaseName();
  e.format = QFileInfo(path).suffix().toUpper();
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    e.metadataError = file.errorString();
    return e;
  }
  auto header = file.read(202);
  if (header.size() >= 202 &&
      (header.startsWith("ProTracker") || header.startsWith("Vortex"))) {
    auto text = [](QByteArray b) {
      int end = b.indexOf('\0');
      if (end >= 0)
        b.truncate(end);
      return QString::fromLocal8Bit(b).trimmed();
    };
    auto name = text(header.mid(30, 32));
    if (!name.isEmpty())
      e.title = name;
    e.author = text(header.mid(66, 32));
    const bool turbo =
        header[13] >= '7' && header[13] <= '9' && header[98] != 32;
    e.format = turbo ? "PT3 TS" : "PT3";
    // Structural duration scan only: never synthesize PCM during import.
    if (file.size() > 65536) {
      e.metadataError = "PT3 exceeds 64 KiB";
      return e;
    }
    file.seek(0);
    auto bytes = file.readAll();
    try {
      ay::Pt3 player(std::vector<uint8_t>(bytes.begin(), bytes.end()));
      auto interrupts = ay::pt3Duration(player, turbo);
      e.seconds = interrupts / profile.interruptHz;
    } catch (const std::exception &error) {
      e.metadataError = QString::fromUtf8(error.what());
    }
  } else if (header.startsWith("YM3!") || header.startsWith("YM3b")) {
    bool loop = header.startsWith("YM3b");
    auto bytes = file.size() - (loop ? 8 : 4);
    e.format = loop ? "YM3b" : "YM3";
    if (bytes > 0 && bytes % 14 == 0)
      e.seconds = (bytes / 14) / profile.interruptHz;
  } else if (header.startsWith(QByteArray("PSG\x1a", 4))) {
    e.format = "PSG";
  }
  return e;
}
