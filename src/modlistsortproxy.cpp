/*
Copyright (C) 2012 Sebastian Herbord. All rights reserved.

This file is part of Mod Organizer.

Mod Organizer is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Mod Organizer is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Mod Organizer.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "modlistsortproxy.h"
#include "modinfo.h"
#include "profile.h"
#include "messagedialog.h"
#include <QMenu>
#include <QCheckBox>
#include <QWidgetAction>
#include <QApplication>
#include <QMimeData>
#include <QDebug>
#include <QTreeView>
#include <functional>


using namespace MOBase::ModFeature;
using MOBase::VersionInfo;


ModListSortProxy::ModListSortProxy(Profile* profile, QObject *parent)
  : QSortFilterProxyModel(parent)
  , m_Profile(profile)
  , m_CategoryFilter()
  , m_CurrentFilter()
  , m_FilterActive(false)
  , m_FilterMode(FILTER_AND)
{
  m_EnabledColumns.set(ModList::COL_FLAGS);
  m_EnabledColumns.set(ModList::COL_NAME);
  m_EnabledColumns.set(ModList::COL_VERSION);
  m_EnabledColumns.set(ModList::COL_PRIORITY);
  setDynamicSortFilter(true); // this seems to work without dynamicsortfilter
                              // but I don't know why. This should be necessary
}

void ModListSortProxy::setProfile(Profile *profile)
{
  m_Profile = profile;
}

void ModListSortProxy::updateFilterActive()
{
  m_FilterActive = ((m_CategoryFilter.size() > 0)
                    || (m_ContentFilter.size() > 0)
                    || !m_CurrentFilter.isEmpty());
  emit filterActive(m_FilterActive);
}

void ModListSortProxy::setCategoryFilter(const std::vector<int> &categories)
{
  m_CategoryFilter = categories;
  updateFilterActive();
  invalidate();
}

void ModListSortProxy::setContentFilter(const std::vector<int> &content)
{
  m_ContentFilter = content;
  updateFilterActive();
  invalidate();
}

Qt::ItemFlags ModListSortProxy::flags(const QModelIndex &modelIndex) const
{
  Qt::ItemFlags flags = sourceModel()->flags(mapToSource(modelIndex));

  return flags;
}

void ModListSortProxy::enableAllVisible()
{
  if (m_Profile == nullptr) return;

  for (int i = 0; i < this->rowCount(); ++i) {
    int modID = mapToSource(index(i, 0)).data(Qt::UserRole + 1).toInt();
    m_Profile->setModEnabled(modID, true);
  }
  invalidate();
}

void ModListSortProxy::disableAllVisible()
{
  if (m_Profile == nullptr) return;

  for (int i = 0; i < this->rowCount(); ++i) {
    int modID = mapToSource(index(i, 0)).data(Qt::UserRole + 1).toInt();
    m_Profile->setModEnabled(modID, false);
  }
  invalidate();
}

QString getFactoryName(int categoryId)
{
  if (categoryId < 0) {
    return "(unset)";
  } else {
    CategoryFactory &categories = CategoryFactory::instance();
    return categories.getCategoryName(categories.getCategoryIndex(categoryId));
  }
}

template <typename T, typename U>
T getValue(U *mod, const std::function<T()> &getter, const T &def)
{
  if (mod == nullptr) {
    return def;
  } else {
    return getter();
  }
}

bool ModListSortProxy::lessThan(const QModelIndex &left,
                                const QModelIndex &right) const
{
  bool lOk, rOk;
  int leftIndex  = left.data(Qt::UserRole + 1).toInt(&lOk);
  int rightIndex = right.data(Qt::UserRole + 1).toInt(&rOk);
  if (!lOk || !rOk) {
    return false;
  }

  ModInfo::Ptr leftMod = ModInfo::getByIndex(leftIndex);
  ModInfo::Ptr rightMod = ModInfo::getByIndex(rightIndex);

  bool lt = false;

  {
    QModelIndex leftPrioIdx = left.sibling(left.row(), ModList::COL_PRIORITY);
    QVariant leftPrio = leftPrioIdx.data();
    if (!leftPrio.isValid()) leftPrio = left.data(Qt::UserRole);
    QModelIndex rightPrioIdx = right.sibling(right.row(), ModList::COL_PRIORITY);
    QVariant rightPrio = rightPrioIdx.data();
    if (!rightPrio.isValid()) rightPrio = right.data(Qt::UserRole);

    lt = leftPrio.toInt() < rightPrio.toInt();
  }

  switch (left.column()) {
    case ModList::COL_FLAGS: {
      if (leftMod->flags().size() != rightMod->flags().size())
        lt = leftMod->flags().size() < rightMod->flags().size();
    } break;
    case ModList::COL_CONTENT: {
      std::vector<ModInfo::EContent> lContent = leftMod->getContents();
      std::vector<ModInfo::EContent> rContent = rightMod->getContents();
      if (lContent.size() != rContent.size()) {
        lt = lContent.size() < rContent.size();
      }

      int lValue = 0;
      int rValue = 0;
      for (ModInfo::EContent content : lContent) {
        lValue += 2 << (unsigned int)content;
      }
      for (ModInfo::EContent content : rContent) {
        rValue += 2 << (unsigned int)content;
      }

      lt = lValue < rValue;
    } break;
    case ModList::COL_NAME: {
      int comp = QString::compare(leftMod->name(), rightMod->name(), Qt::CaseInsensitive);
      if (comp != 0)
        lt = comp < 0;
    } break;
    case ModList::COL_CATEGORY: {
      int leftCategory = -1;
      int rightCategory = -1;
      Categorized *leftCategorized = leftMod->feature<Categorized>();
      Categorized *rightCategorized = rightMod->feature<Categorized>();
      if (leftCategorized != nullptr) {
        leftCategory = leftCategorized->primary();
      }
      if (rightCategorized != nullptr) {
        rightCategory = rightCategorized->primary();
      }
      lt = getFactoryName(leftCategory) < getFactoryName(rightCategory);
    } break;
    case ModList::COL_MODID: {
      int leftModId = -1;
      int rightModId = -1;
      QString leftModIdStr, rightModIdStr;

      Repository *leftRepo = leftMod->feature<Repository>();
      Repository *rightRepo = rightMod->feature<Repository>();
      bool leftIsNumeric = true;
      bool rightIsNumeric = true;
      if (leftRepo != nullptr) {
        leftModIdStr = leftRepo->modId();
        leftModId = leftModIdStr.toInt(&leftIsNumeric);
      }
      if (rightRepo != nullptr) {
        rightModIdStr = rightRepo->modId();
        rightModId = rightModIdStr.toInt(&rightIsNumeric);
      }
      if (leftIsNumeric && rightIsNumeric) {
        lt = leftModId < rightModId;
      } else {
        lt = leftModIdStr < rightModIdStr;
      }
    } break;
    case ModList::COL_VERSION: {
      VersionInfo leftVer, rightVer;
      Versioned *leftVersioned = leftMod->feature<Versioned>();
      Versioned *rightVersioned = rightMod->feature<Versioned>();

      if (leftVersioned != nullptr) {
        leftVer = leftVersioned->get();
      }
      if (rightVersioned != nullptr) {
        rightVer = rightVersioned->get();
      }

      lt = leftVer < rightVer;
    } break;
    case ModList::COL_INSTALLTIME: {
      QDateTime leftTime = left.data().toDateTime();
      QDateTime rightTime = right.data().toDateTime();
      if (leftTime != rightTime)
        return leftTime < rightTime;
    } break;
    case ModList::COL_PRIORITY: {
      // nop, already compared by priority
    } break;
  }
  return lt;
}

void ModListSortProxy::updateFilter(const QString &filter)
{
  m_CurrentFilter = filter;
  updateFilterActive();
  // using invalidateFilter here should be enough but that crashes the application? WTF?
  // invalidateFilter();
  invalidate();
}

bool ModListSortProxy::hasConflictFlag(const std::set<EModFlag> &flags) const
{
  for (EModFlag flag : flags) {
    if ((flag == EModFlag::CONFLICT_MIXED) ||
        (flag == EModFlag::CONFLICT_OVERWRITE) ||
        (flag == EModFlag::CONFLICT_OVERWRITTEN) ||
        (flag == EModFlag::CONFLICT_REDUNDANT)) {
      return true;
    }
  }

  return false;
}

bool ModListSortProxy::filterMatchesModAnd(ModInfo::Ptr info, bool enabled) const
{
  for (auto iter = m_CategoryFilter.begin(); iter != m_CategoryFilter.end(); ++iter) {
    switch (*iter) {
      case CategoryFactory::CATEGORY_SPECIAL_CHECKED: {
        if (!enabled && !info->alwaysEnabled()) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UNCHECKED: {
        if (enabled || info->alwaysEnabled()) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE: {
        Repository *repo = info->feature<Repository>();
        if (repo == nullptr) {
          return false;
        } else if (!repo->updateAvailable() && !repo->downgradeAvailable()) {
          return false;
        }
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY: {
        Categorized *categorized = info->feature<Categorized>();
        if (categorized != nullptr) {
          if (categorized->getCategories().size() > 0) {
            return false;
          }
        }
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_CONFLICT: {
        if (!hasConflictFlag(info->flags())) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_NOTENDORSED: {
        Endorsable *endorsable = info->feature<Endorsable>();
        if (endorsable != nullptr) {
          if (endorsable->endorsedState() != Endorsable::ENDORSED_FALSE) {
            return false;
          }
        }
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_MANAGED: {
        if (info->hasFlag(EModFlag::FOREIGN)) return false;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UNMANAGED: {
        if (!info->hasFlag(EModFlag::FOREIGN)) return false;
      } break;
      default: {
        MOBase::ModFeature::Categorized *category =
            info->feature<MOBase::ModFeature::Categorized>();
        if (!category->isSet(*iter))
          return false;
      } break;
    }
  }

  for (int content : m_ContentFilter) {
    if (!info->hasContent(static_cast<ModInfo::EContent>(content)))
      return false;
  }

  return true;
}

bool ModListSortProxy::filterMatchesModOr(ModInfo::Ptr info, bool enabled) const
{
  for (auto iter = m_CategoryFilter.begin(); iter != m_CategoryFilter.end(); ++iter) {
    switch (*iter) {
      case CategoryFactory::CATEGORY_SPECIAL_CHECKED: {
        if (enabled || info->alwaysEnabled()) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UNCHECKED: {
        if (!enabled && !info->alwaysEnabled()) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UPDATEAVAILABLE: {
        Repository *repo = info->feature<Repository>();
        if (repo != nullptr) {
          if (repo->updateAvailable() || repo->downgradeAvailable()) {
            return true;
          }
        }
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_NOCATEGORY: {
        Categorized *categorized = info->feature<Categorized>();
        if (categorized != nullptr) {
          if (categorized->getCategories().size() == 0) {
            return true;
          }
        } else {
          return true;
        }
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_CONFLICT: {
        if (hasConflictFlag(info->flags())) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_NOTENDORSED: {
        Endorsable *endorsable = info->feature<Endorsable>();
        if (endorsable != nullptr) {
          Endorsable::EEndorsedState state = endorsable->endorsedState();
          if ((state == Endorsable::ENDORSED_FALSE) || (state == Endorsable::ENDORSED_NEVER)) {
            return true;
          }
        } else {
          return true;
        }
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_MANAGED: {
        if (!info->hasFlag(EModFlag::FOREIGN)) return true;
      } break;
      case CategoryFactory::CATEGORY_SPECIAL_UNMANAGED: {
        if (info->hasFlag(EModFlag::FOREIGN)) return true;
      } break;
      default: {
        Categorized *categorized = info->feature<Categorized>();
        if ((categorized != nullptr) && categorized->isSet(*iter)) {
          return true;
        }
      } break;
    }
  }

  foreach (int content, m_ContentFilter) {
    if (info->hasContent(static_cast<ModInfo::EContent>(content))) return true;
  }

  return false;
}

bool ModListSortProxy::filterMatchesMod(ModInfo::Ptr info, bool enabled) const
{
  if (!m_CurrentFilter.isEmpty() &&
      !info->name().contains(m_CurrentFilter, Qt::CaseInsensitive)) {
    return false;
  }

  if (m_FilterMode == FILTER_AND) {
    return filterMatchesModAnd(info, enabled);
  } else {
    return filterMatchesModOr(info, enabled);
  }
}

void ModListSortProxy::setFilterMode(ModListSortProxy::FilterMode mode)
{
  if (m_FilterMode != mode) {
    m_FilterMode = mode;
    this->invalidate();
  }
}

bool ModListSortProxy::filterAcceptsRow(int row, const QModelIndex &parent) const
{
  if (m_Profile == nullptr) {
    return false;
  }

  if (row >= static_cast<int>(m_Profile->numMods())) {
    qWarning("invalid row idx %d", row);
    return false;
  }

  QModelIndex idx = sourceModel()->index(row, 0, parent);
  if (!idx.isValid()) {
    qDebug("invalid index");
    return false;
  }
  if (sourceModel()->hasChildren(idx)) {
    for (int i = 0; i < sourceModel()->rowCount(idx); ++i) {
      if (filterAcceptsRow(i, idx)) {
        return true;
      }
    }

    return false;
  } else {
    bool modEnabled = idx.sibling(row, 0).data(Qt::CheckStateRole).toInt() == Qt::Checked;
    unsigned int index = idx.data(Qt::UserRole + 1).toInt();
    return filterMatchesMod(ModInfo::getByIndex(index), modEnabled);
  }
}

bool ModListSortProxy::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                    int row, int column, const QModelIndex &parent)
{
  if (!data->hasUrls() && (sortColumn() != ModList::COL_PRIORITY)) {
    QWidget *wid = qApp->activeWindow()->findChild<QTreeView*>("modList");
    MessageDialog::showMessage(tr("Drag&Drop is only supported when sorting by priority"), wid);
    return false;
  }
  if ((row == -1) && (column == -1)) {
    return this->sourceModel()->dropMimeData(data, action, -1, -1, mapToSource(parent));
  }
  // in the regular model, when dropping between rows, the row-value passed to
  // the sourceModel is inconsistent between ascending and descending ordering.
  // This should fix that
  if (sortOrder() == Qt::DescendingOrder) {
    --row;
  }

  QModelIndex proxyIndex = index(row, column, parent);
  QModelIndex sourceIndex = mapToSource(proxyIndex);
  return this->sourceModel()->dropMimeData(data, action, sourceIndex.row(), sourceIndex.column(),
                                           sourceIndex.parent());
}
