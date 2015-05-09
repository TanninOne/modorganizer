    #include "modflagicondelegate.h"
#include <QList>


EModFlag ModFlagIconDelegate::m_ConflictFlags[4] = { EModFlag::CONFLICT_MIXED
                                                   , EModFlag::CONFLICT_OVERWRITE
                                                   , EModFlag::CONFLICT_OVERWRITTEN
                                                   , EModFlag::CONFLICT_REDUNDANT };

ModFlagIconDelegate::ModFlagIconDelegate(QObject *parent)
  : IconDelegate(parent)
{
}

QList<QString> ModFlagIconDelegate::getIcons(const QModelIndex &index) const
{
  QList<QString> result;
  QVariant modid = index.data(Qt::UserRole + 1);
  if (modid.isValid()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modid.toInt());
    std::set<EModFlag> flags = info->flags();

    { // insert conflict icon first to provide nicer alignment
      auto iter = std::find_first_of(flags.begin(), flags.end(), m_ConflictFlags, m_ConflictFlags + 4);
      if (iter != flags.end()) {
        result.append(getFlagIcon(*iter));
        flags.erase(iter);
      } else {
        result.append(QString());
      }
    }

    for (EModFlag flag : flags) {
      result.append(getFlagIcon(flag));
    }
  }
  return result;
}

QString ModFlagIconDelegate::getFlagIcon(EModFlag flag) const
{
  switch (flag) {
    case EModFlag::BACKUP: return ":/MO/gui/emblem_backup";
    case EModFlag::INVALID: return ":/MO/gui/problem";
    case EModFlag::NOTENDORSED: return ":/MO/gui/emblem_notendorsed";
    case EModFlag::NOTES: return ":/MO/gui/emblem_notes";
    case EModFlag::CONFLICT_OVERWRITE: return ":/MO/gui/emblem_conflict_overwrite";
    case EModFlag::CONFLICT_OVERWRITTEN: return ":/MO/gui/emblem_conflict_overwritten";
    case EModFlag::CONFLICT_MIXED: return ":/MO/gui/emblem_conflict_mixed";
    case EModFlag::CONFLICT_REDUNDANT: return ":MO/gui/emblem_conflict_redundant";
    default: return QString();
  }
}

size_t ModFlagIconDelegate::getNumIcons(const QModelIndex &index) const
{
  unsigned int modIdx = index.data(Qt::UserRole + 1).toInt();
  if (modIdx < ModInfo::getNumMods()) {
    ModInfo::Ptr info = ModInfo::getByIndex(modIdx);
    std::set<EModFlag> flags = info->flags();
    int count = flags.size();
    if (std::find_first_of(flags.begin(), flags.end(), m_ConflictFlags, m_ConflictFlags + 4) == flags.end()) {
      ++count;
    }
    return count;
  } else {
    return 0;
  }
}


QSize ModFlagIconDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &modelIndex) const
{
  int count = getNumIcons(modelIndex);
  unsigned int index = modelIndex.data(Qt::UserRole + 1).toInt();
  QSize result;
  if (index < ModInfo::getNumMods()) {
    result = QSize(count * 40, 20);
  } else {
    result = QSize(1, 20);
  }
  if (option.rect.width() > 0) {
    result.setWidth(std::min(option.rect.width(), result.width()));
  }
  return result;
}

