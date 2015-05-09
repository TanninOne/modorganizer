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

#include "modinfo.h"
#include "utility.h"
#include "installationtester.h"
#include "categories.h"
#include "report.h"
#include "modinfodialog.h"
#include "overwriteinfodialog.h"
#include "json.h"
#include "messagedialog.h"
#include "acfparser.h"

#include <gameinfo.h>
#include <iplugingame.h>
#include <versioninfo.h>
#include <appconfig.h>
#include <scriptextender.h>

#include <QApplication>
#include <QDebug>
#include <QDirIterator>
#include <QMutexLocker>
#include <QSettings>
#include <QUrlQuery>
#include <sstream>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>


using namespace MOBase;
using namespace MOShared;


std::vector<ModInfo::Ptr> ModInfo::s_Collection;
std::map<QString, unsigned int> ModInfo::s_ModsByName;
std::map<QString, std::vector<unsigned int> > ModInfo::s_ModsByModID;
int ModInfo::s_NextID;
QMutex ModInfo::s_Mutex(QMutex::Recursive);

QString ModInfo::s_HiddenExt(".mohidden");


static bool ByName(const ModInfo::Ptr &LHS, const ModInfo::Ptr &RHS)
{
  return QString::compare(LHS->name(), RHS->name(), Qt::CaseInsensitive) < 0;
}


ModInfo::Ptr ModInfo::createFrom(const QDir &dir, DirectoryEntry **directoryStructure)
{
  QMutexLocker locker(&s_Mutex);
  static QRegExp backupExp(".*backup[0-9]*");
  ModInfo::Ptr result(new ModInfo(dir.dirName()));
  if (!backupExp.exactMatch(dir.dirName())) {
    result->addFeature(new ModFeature::Categorized());
    result->addFeature(new ModFeature::Conflicting(directoryStructure));
    result->addFeature(new ModFeature::Endorsable());
    result->addFeature(new ModFeature::Installed(dir.absolutePath()));
    result->addFeature(new ModFeature::Note());
    result->addFeature(new ModFeature::Positioning());
    result->addFeature(new ModFeature::Versioned());
    result->addFeature(new ModFeature::NexusRepository());
  }
  s_Collection.push_back(result);
  return result;
}

ModInfo::Ptr ModInfo::createFromPlugin(const QString &espName
                                       , bool displayForeign)
{
  QMutexLocker locker(&s_Mutex);
  ModInfo::Ptr result = ModInfo::Ptr(
        new ModInfo(QString("Unmanaged: %1").arg(QFileInfo(espName).baseName()),
                    { EModFlag::FOREIGN }));
  result->addFeature(new ModFeature::Positioning(ModFeature::Positioning::ECheckable::FIXED_ACTIVE));
  result->addFeature(new ModFeature::ForeignInstalled(espName, displayForeign));

  s_Collection.push_back(result);
  return result;
}

ModInfo::Ptr ModInfo::createFromSteam(const QString &modPath, const QString &steamKey)
{
  QMutexLocker locker(&s_Mutex);
  ModInfo::Ptr result = ModInfo::Ptr(new ModInfo(QString("Steam: %1").arg(steamKey),
                                                 { EModFlag::FOREIGN }));

  result->addFeature(new ModFeature::Positioning());
  result->addFeature(new ModFeature::SteamRepository(steamKey));
  result->addFeature(new ModFeature::Versioned());
  result->addFeature(new ModFeature::SteamInstalled(modPath));

  s_Collection.push_back(result);

  result->feature<ModFeature::SteamRepository>()->updateInfo();

  return result;
}

QString ModInfo::getContentTypeName(int contentType)
{
  switch (contentType) {
    case CONTENT_PLUGIN:    return tr("Plugins");
    case CONTENT_TEXTURE:   return tr("Textures");
    case CONTENT_MESH:      return tr("Meshes");
    case CONTENT_BSA:       return tr("BSA");
    case CONTENT_INTERFACE: return tr("UI Changes");
    case CONTENT_MUSIC:     return tr("Music");
    case CONTENT_SOUND:     return tr("Sound Effects");
    case CONTENT_SCRIPT:    return tr("Scripts");
    case CONTENT_SKSE:      return tr("SKSE Plugins");
    case CONTENT_SKYPROC:   return tr("SkyProc Tools");
    case CONTENT_STRING:    return tr("Strings");
    default: throw MyException(tr("invalid content type %1").arg(contentType));
  }
}

bool ModInfo::isEmpty() const
{
  const ModFeature::DiskLocation *location = feature<ModFeature::DiskLocation>();
  if (location != nullptr) {
    QDirIterator iter(location->absolutePath(), QDir::NoDotAndDotDot | QDir::Files | QDir::Dirs);
    if (!iter.hasNext()) {
      return true;
    } else {
      iter.next();
      return (iter.fileName() == "meta.ini") && !iter.hasNext();
    }
  } else {
    return true;
  }
}

void ModInfo::setInstallationFile(const QString &fileName) {
  ModFeature::Installed *installed = feature<ModFeature::Installed>();
  if (installed != nullptr) {
    installed->setInstallationFile(fileName);
  }
}

bool ModInfo::remove()
{
  m_MetaInfoChanged = false;
  return shellDelete(QStringList(absolutePath()), true);
}

QString ModInfo::name() const
{
  const ModFeature::SteamRepository *repo = feature<ModFeature::SteamRepository>();
  if (repo != nullptr) {
    QString name = repo->modName();
    if (!name.isEmpty()) {
      return QString("Steam: %1").arg(repo->modName());
    }
  }
  return internalName();
}

QString ModInfo::internalName() const
{
  return m_Name;
}

std::set<EModFlag> ModInfo::flags() const {
  std::set<EModFlag> result = m_Flags;
  if (!isValid()) {
    result.insert(EModFlag::INVALID);
  }
  for (auto feature : m_Features) {
    std::set<EModFlag> featureFlags = feature.second->flags();
    result.insert(featureFlags.begin(), featureFlags.end());
  }
  return result;
}

void ModInfo::createFromOverwrite()
{
  QMutexLocker locker(&s_Mutex);

  ModInfo::Ptr modInfo(new ModInfo("Overwrite"));
  modInfo->addFeature(new ModFeature::OverwriteLocation());
  modInfo->addFeature(
        new ModFeature::Positioning(ModFeature::Positioning::ECheckable::FIXED_ACTIVE,
                                    ModFeature::Positioning::EPosition::FIXED_HIGHEST));

  s_Collection.push_back(modInfo);
}

unsigned int ModInfo::getNumMods()
{
  QMutexLocker locker(&s_Mutex);
  return s_Collection.size();
}


ModInfo::Ptr ModInfo::getByIndex(unsigned int index)
{
  QMutexLocker locker(&s_Mutex);

  if (index >= s_Collection.size()) {
    throw MyException(tr("invalid index %1 (maximum: %2)").arg(index).arg(s_Collection.size()));
  }
  return s_Collection[index];
}


std::vector<ModInfo::Ptr> ModInfo::getByModID(int modID)
{
  QMutexLocker locker(&s_Mutex);

  auto iter = s_ModsByModID.find(QString::number(modID));
  if (iter == s_ModsByModID.end()) {
    return std::vector<ModInfo::Ptr>();
  }

  std::vector<ModInfo::Ptr> result;
  for (unsigned int modIndex : iter->second) {
    result.push_back(getByIndex(modIndex));
  }
  /*
  for (auto idxIter = iter->second.begin(); idxIter != iter->second.end(); ++idxIter) {
    result.push_back(getByIndex(*idxIter));
  }
  */

  return result;
}


bool ModInfo::removeMod(unsigned int index)
{
  QMutexLocker locker(&s_Mutex);

  if (index >= s_Collection.size()) {
    throw MyException(tr("invalid index %1").arg(index));
  }
  // update the indices first
  ModInfo::Ptr modInfo = s_Collection[index];
  s_ModsByName.erase(s_ModsByName.find(modInfo->name()));

  ModFeature::Repository *repo = modInfo->feature<ModFeature::Repository>();

  if (repo != nullptr) {
    auto iter = s_ModsByModID.find(repo->modId());
    if (iter != s_ModsByModID.end()) {
      std::vector<unsigned int> indices = iter->second;
      indices.erase(std::remove(indices.begin(), indices.end(), index), indices.end());
      s_ModsByModID[repo->modId()] = indices;
    }
  }

  // physically remove the mod directory
  //TODO the return value is ignored because the indices were already removed here, so stopping
  // would cause data inconsistencies. Instead we go through with the removal but the mod will show up
  // again if the user refreshes
  modInfo->remove();

  // finally, remove the mod from the collection
  s_Collection.erase(s_Collection.begin() + index);

  // and update the indices
  updateIndices();
  return true;
}


unsigned int ModInfo::getIndex(const QString &name)
{
  QMutexLocker locker(&s_Mutex);

  std::map<QString, unsigned int>::iterator iter = s_ModsByName.find(name);
  if (iter == s_ModsByName.end()) {
    return UINT_MAX;
  }

  return iter->second;
}

unsigned int ModInfo::findMod(const boost::function<bool (ModInfo::Ptr)> &filter)
{
  for (unsigned int i = 0U; i < s_Collection.size(); ++i) {
    if (filter(s_Collection[i])) {
      return i;
    }
  }
  return UINT_MAX;
}


void ModInfo::updateFromDisc(const QString &modDirectory
                             , DirectoryEntry **directoryStructure
                             , bool displayForeign)
{
  QMutexLocker lock(&s_Mutex);
  s_Collection.clear();
  s_NextID = 0;

  { // list all directories in the mod directory and make a mod out of each
    QDir mods(QDir::fromNativeSeparators(modDirectory));
    mods.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QDirIterator modIter(mods);
    while (modIter.hasNext()) {
      createFrom(QDir(modIter.next()), directoryStructure);
    }
  }

  { // list all steam workshop installed mods
    IPluginGame *game = qApp->property("managed_game").value<IPluginGame*>();

    boost::filesystem::path workshopPath =
        (boost::filesystem::path(GameInfo::instance().getGameDirectory())
         / ".." / ".." / "workshop").normalize();

    boost::filesystem::path workshopFilePath =
        workshopPath
        / (boost::format("appworkshop_%1%.acf") % game->steamAPPId().toStdString()).str();

    std::ifstream workshopFile(workshopFilePath.string(),  std::fstream::in);
    if (workshopFile.is_open()) {
      ACFPropertyTree workshopInfo = ACFPropertyTree::parse(workshopFile);
      ACFPropertyTree items = workshopInfo.getMap("AppWorkshop").getMap("WorkshopItemDetails");

      boost::filesystem::path workshopContentPath =
          workshopPath / "content" / game->steamAPPId().toStdString();

      std::vector<std::string> fileIds;

      for (const std::string &key : items.getKeys()) {
        if (items.getMap(key).getString("manifest") == "-1") {
          qDebug("%s seems to be a legacy mod", key.c_str());
          continue;
        }

        boost::filesystem::path modPath = workshopContentPath / key;
        if (!boost::filesystem::exists(modPath)) {
          qWarning("no content directory for steam item %s", key.c_str());
          continue;
        }
        fileIds.push_back(key);

        createFromSteam(QString::fromStdString(modPath.string())
                        , QString::fromStdString(key));
      }
    } else {
      qDebug("no workshop file");
    }
  }

  { // list plugins in the data directory and make a foreign-managed mod out of each
    std::vector<std::wstring> dlcPlugins = GameInfo::instance().getDLCPlugins();
    QDir dataDir(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())) + "/data");
    for (const QFileInfo &file : dataDir.entryInfoList(QStringList() << "*.esp" << "*.esm")) {
      if ((file.baseName() != "Update") // hide update
           && (file.baseName() != ToQString(GameInfo::instance().getGameName())) // hide the game esp
           && (displayForeign // show non-dlc bundles only if the user wants them
               || std::find(dlcPlugins.begin(), dlcPlugins.end(), ToWString(file.fileName())) != dlcPlugins.end())) {
        createFromPlugin(file.fileName(), displayForeign);
      }
    }
  }

  createFromOverwrite();

  std::sort(s_Collection.begin(), s_Collection.end(), ByName);

  updateIndices();
}

ModInfo::~ModInfo()
{
  try {
    saveMeta();
  } catch (const std::exception &e) {
    qCritical("failed to save meta information for \"%s\": %s",
              m_Name.toUtf8().constData(), e.what());
  }
}

void ModInfo::readMeta()
{
  if (QFile::exists(absolutePath())) {
    QSettings metaFile(absolutePath() + "/meta.ini", QSettings::IniFormat);

    for (auto featurePair : m_Features) {
      featurePair.second->readMeta(metaFile);
    }

    metaFile.endArray();
  }

  m_MetaInfoChanged = false;
}

void ModInfo::saveMeta()
{
  // only write meta data if the mod directory exists
  if (m_MetaInfoChanged && QFile::exists(absolutePath())) {
    QSettings metaFile(absolutePath() + "/meta.ini", QSettings::IniFormat);
    if (metaFile.status() == QSettings::NoError) {
      for (auto featurePair : m_Features) {
        featurePair.second->saveMeta(metaFile);
      }

      metaFile.sync(); // sync needs to be called to ensure the file is created

      if (metaFile.status() == QSettings::NoError) {
        m_MetaInfoChanged = false;
      } else {
        reportError(tr("failed to write %1/meta.ini: error %2").arg(absolutePath()).arg(metaFile.status()));
      }
    } else {
      reportError(tr("failed to write %1/meta.ini: error %2").arg(absolutePath()).arg(metaFile.status()));
    }
  }
}

ModInfo::ModInfo(const QString &name, const std::vector<EModFlag> flags)
  : m_Name(name)
  , m_Flags(flags.begin(), flags.end())
{
  testValid();
  // read out the meta-file for information
  readMeta();
}

void ModInfo::updateIndices()
{
  s_ModsByName.clear();
  s_ModsByModID.clear();

  for (unsigned int i = 0; i < s_Collection.size(); ++i) {
    QString modName = s_Collection[i]->internalName();
    s_ModsByName[modName] = i;

    ModFeature::Repository *repo = s_Collection[i]->feature<ModFeature::Repository>();
    if (repo != nullptr) {
      s_ModsByModID[repo->modId()].push_back(i);
    }
  }
}

void ModInfo::checkChunkForUpdate(const std::vector<int> &modIDs, QObject *receiver)
{
  if (modIDs.size() != 0) {
    NexusInterface::instance()->requestUpdates(modIDs, receiver, QVariant(), QString());
  }
}

int ModInfo::checkAllForUpdate(QObject *receiver)
{
  int result = 0;
  std::vector<int> modIDs;

  // check update ofor MO itself
  modIDs.push_back(GameInfo::instance().getNexusModID());

  // check mods
  for (const ModInfo::Ptr &mod : s_Collection) {
    ModFeature::Repository *repo = mod->feature<ModFeature::Repository>();
    if (repo != nullptr) {
      if (repo->canBeUpdated()) {
        modIDs.push_back(repo->modId().toInt());
        if (modIDs.size() > 255) {
          checkChunkForUpdate(modIDs, receiver);
          modIDs.clear();
        }
      }
    }
  }

  checkChunkForUpdate(modIDs, receiver);

  return result;
}

bool ModInfo::hasFlag(EModFlag flag) const
{
  std::set<EModFlag> flagList = flags();
  return std::find(flagList.begin(), flagList.end(), flag) != flagList.end();
}

std::vector<ModInfo::EContent> ModInfo::getContents() const
{
  QTime now = QTime::currentTime();
  if (m_LastContentCheck.isNull() || (m_LastContentCheck.secsTo(now) > 60)) {
    m_CachedContent.clear();
    QDir dir(absolutePath());
    if (dir.entryList(QStringList() << "*.esp" << "*.esm").size() > 0) {
      m_CachedContent.push_back(CONTENT_PLUGIN);
    }
    if (dir.entryList(QStringList() << "*.bsa").size() > 0) {
      m_CachedContent.push_back(CONTENT_BSA);
    }

    ScriptExtender *extender = qApp->property("managed_game").value<IPluginGame*>()->feature<ScriptExtender>();

    if (extender != nullptr) {
      QString sePluginPath = extender->name() + "/plugins";
      if (dir.exists(sePluginPath)) m_CachedContent.push_back(CONTENT_SKSE);
    }
    if (dir.exists("textures"))   m_CachedContent.push_back(CONTENT_TEXTURE);
    if (dir.exists("meshes"))     m_CachedContent.push_back(CONTENT_MESH);
    if (dir.exists("interface")
        || dir.exists("menus"))   m_CachedContent.push_back(CONTENT_INTERFACE);
    if (dir.exists("music"))      m_CachedContent.push_back(CONTENT_MUSIC);
    if (dir.exists("sound"))      m_CachedContent.push_back(CONTENT_SOUND);
    if (dir.exists("scripts"))    m_CachedContent.push_back(CONTENT_SCRIPT);
    if (dir.exists("strings"))    m_CachedContent.push_back(CONTENT_STRING);
    if (dir.exists("SkyProc Patchers"))  m_CachedContent.push_back(CONTENT_SKYPROC);

    m_LastContentCheck = QTime::currentTime();
  }

  return m_CachedContent;
}

bool ModInfo::hasContent(ModInfo::EContent content) const
{
  std::vector<EContent> contents = getContents();
  return std::find(contents.begin(), contents.end(), content) != contents.end();
}

std::vector<QString> ModInfo::getIniTweaks() const
{
  QString metaFileName = absolutePath().append("/meta.ini");
  QSettings metaFile(metaFileName, QSettings::IniFormat);

  std::vector<QString> result;

  int numTweaks = metaFile.beginReadArray("INI Tweaks");

  if (numTweaks != 0) {
    qDebug("%d active ini tweaks in %s",
           numTweaks, QDir::toNativeSeparators(metaFileName).toUtf8().constData());
  }

  for (int i = 0; i < numTweaks; ++i) {
    metaFile.setArrayIndex(i);
    QString filename = absolutePath() + "/INI Tweaks/" + metaFile.value("name").toString();
    result.push_back(filename);
  }
  metaFile.endArray();
  return result;
}

QString ModInfo::getDescription() const
{
  std::set<EModFlag> modFlags = flags();
  if (modFlags.find(EModFlag::BACKUP) != modFlags.end()) {
    return tr("This is the backup of a mod");
  } else if (!isValid()) {
    return tr("%1 contains no esp/esm and no asset (textures, meshes, interface, ...) directory").arg(name());
  } else {
    const ModFeature::Categorized *categorized = feature<ModFeature::Categorized>();

    std::wostringstream categoryString;
    if (categorized != nullptr) {
      const std::set<int> &categories = categorized->getCategories();
      categoryString << ToWString(tr("Categories: <br>"));
      CategoryFactory &categoryFactory = CategoryFactory::instance();
      for (std::set<int>::const_iterator catIter = categories.begin();
           catIter != categories.end(); ++catIter) {
        if (catIter != categories.begin()) {
          categoryString << " , ";
        }
        int categoryIndex = categoryFactory.getCategoryIndex(*catIter);
        categoryString << "<span style=\"white-space: nowrap;\"><i>"
                       << ToWString(categoryFactory.getCategoryName(categoryIndex))
                       << "</font></span>";
      }
    }

    return ToQString(categoryString.str());
  }
}

QDateTime ModInfo::creationTime() const
{
  return QFileInfo(absolutePath()).created();
}

void ModInfo::testValid()
{
  m_Valid = false;
  QDirIterator dirIter(absolutePath());
  while (dirIter.hasNext()) {
    dirIter.next();
    if (dirIter.fileInfo().isDir()) {
      if (InstallationTester::isTopLevelDirectory(dirIter.fileName())) {
        m_Valid = true;
        break;
      }
    } else {
      if (InstallationTester::isTopLevelSuffix(dirIter.fileName())) {
        m_Valid = true;
        break;
      }
    }
  }

  // NOTE: in Qt 4.7 it seems that QDirIterator leaves a file handle open if it is not iterated to the
  // end
  while (dirIter.hasNext()) {
    dirIter.next();
  }
}

void ModInfo::setVersion(const VersionInfo &version)
{
  ModFeature::Versioned *versioned = feature<ModFeature::Versioned>();
  if (versioned != nullptr) {
    versioned->set(version);
  }
}

void ModInfo::setNewestVersion(const VersionInfo &version)
{
  ModFeature::Repository *repository = feature<ModFeature::Repository>();
  if (repository != nullptr) {
     repository->setVersion(version);
  }
}

void ModInfo::setIsEndorsed(bool endorsed)
{
  ModFeature::Endorsable *endorsable = feature<ModFeature::Endorsable>();
  if (endorsable != nullptr) {
    endorsable->setIsEndorsed(endorsed);
  }
}

void ModInfo::setRepoModID(int modId)
{
  ModFeature::Repository *repository = feature<ModFeature::Repository>();
  if (repository != nullptr) {
     repository->setModId(QString::number(modId));
  }
}

void ModInfo::addNexusCategory(int categoryId)
{
  ModFeature::NexusRepository *repository = feature<ModFeature::NexusRepository>();
  ModFeature::Categorized *categorized = feature<ModFeature::Categorized>();
  if ((repository != nullptr) && (categorized != nullptr)) {
    int cat = repository->translateCategory(QString::number(categoryId));
    categorized->set(cat, true);
  }
}

QString ModInfo::absolutePath() const
{
  const ModFeature::DiskLocation *location = feature<ModFeature::DiskLocation>();
  if (location != nullptr) {
    return location->absolutePath();
  } else {
    return QString();
  }
}

QStringList ModInfo::archives() const
{
  const ModFeature::Installed *installed = feature<ModFeature::Installed>();
  if (installed  != nullptr) {
    return installed ->archives();
  } else {
    return QStringList();
  }
}

bool ModInfo::setName(const QString &name)
{
  if (name.contains('/') || name.contains('\\')) {
    return false;
  }
  ModFeature::Installed *installed = feature<ModFeature::Installed>();

  if (installed == nullptr) {
    m_Name = name;
  } else {
    // for installed mods we have to rename the whole thing
    QString path = installed->absolutePath();

    QString newPath = path.mid(0).replace(path.length() - m_Name.length(), m_Name.length(), name);
    QDir modDir(path.mid(0, path.length() - m_Name.length()));

    if (m_Name.compare(name, Qt::CaseInsensitive) == 0) {
      QString tempName = name;
      tempName.append("_temp");
      while (modDir.exists(tempName)) {
        tempName.append("_");
      }
      if (!modDir.rename(m_Name, tempName)) {
        return false;
      }
      if (!modDir.rename(tempName, name)) {
        qCritical("rename to final name failed after successful rename to intermediate name");
        modDir.rename(tempName, m_Name);
        return false;
      }
    } else {
      if (!shellRename(modDir.absoluteFilePath(m_Name), modDir.absoluteFilePath(name))) {
        qCritical("failed to rename mod %s (errorcode %d)",
                  qPrintable(name), ::GetLastError());
        return false;
      }
    }

    std::map<QString, unsigned int>::iterator nameIter = s_ModsByName.find(m_Name);
    if (nameIter != s_ModsByName.end()) {
      unsigned int index = nameIter->second;
      s_ModsByName.erase(nameIter);

      m_Name = name;
      installed->setPath(newPath);

      s_ModsByName[m_Name] = index;

      std::sort(s_Collection.begin(), s_Collection.end(), ByName);
      updateIndices();
    } else { // otherwise mod isn't registered yet?
      m_Name = name;
      installed->setPath(newPath);
    }
  }

  return true;
}
