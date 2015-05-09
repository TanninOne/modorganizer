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

#ifndef MODINFO_H
#define MODINFO_H

#include "nexusinterface.h"
#include "modfeatures.h"
#include "modflags.h"
#include <versioninfo.h>
#include <imodinterface.h>
#include <directoryentry.h>

#include <QString>
#include <QMutex>
#include <QIcon>
#include <QDir>
#include <QSharedPointer>
#include <QDateTime>

#include <boost/any.hpp>

#include <map>
#include <set>
#include <vector>
#include <typeindex>

using MOBase::ModRepositoryFileInfo;


/**
 * @brief Represents meta information about a single mod.
 * 
 * Represents meta information about a single mod. The class interface is used
 * to manage the mod collection
 *
 **/
class ModInfo : public QObject, public MOBase::IModInterface
{

  Q_OBJECT

public:

  typedef QSharedPointer<ModInfo> Ptr;

  static QString s_HiddenExt;

  enum EContent {
    CONTENT_PLUGIN,
    CONTENT_TEXTURE,
    CONTENT_MESH,
    CONTENT_BSA,
    CONTENT_INTERFACE,
    CONTENT_MUSIC,
    CONTENT_SOUND,
    CONTENT_SCRIPT,
    CONTENT_SKSE,
    CONTENT_SKYPROC,
    CONTENT_STRING
  };

  static const int NUM_CONTENT_TYPES = CONTENT_STRING + 1;

  enum EHighlight {
    HIGHLIGHT_NONE = 0,
    HIGHLIGHT_INVALID = 1,
    HIGHLIGHT_CENTER = 2,
    HIGHLIGHT_IMPORTANT = 4
  };

public:

  /**
   * @brief read the mod directory and Mod ModInfo objects for all subdirectories
   **/
  static void updateFromDisc(const QString &modDirectory, MOShared::DirectoryEntry **directoryStructure, bool displayForeign);

  static void clear() { s_Collection.clear(); s_ModsByName.clear(); s_ModsByModID.clear(); }

  ~ModInfo();

  /**
   * @brief stores meta information back to disk
   */
  void saveMeta();

  /**
   * @brief restores meta information from disk
   */
  void readMeta();

  /**
   * @brief retrieve the number of mods
   *
   * @return number of mods
   **/
  static unsigned int getNumMods();

  /**
   * @brief retrieve a ModInfo object based on its index
   *
   * @param index the index to look up. the maximum is getNumMods() - 1
   * @return a reference counting pointer to the mod info.
   * @note since the pointer is reference counting, the pointer remains valid even if the collection is refreshed in a different thread
   **/
  static ModInfo::Ptr getByIndex(unsigned int index);

  /**
   * @brief retrieve a ModInfo object based on its nexus mod id
   *
   * @param modID the nexus mod id to look up
   * @return a reference counting pointer to the mod info
   * @todo in its current form, this function is broken! There may be multiple mods with the same nexus id,
   *       this function will return only one of them
   **/
  static std::vector<ModInfo::Ptr> getByModID(int modID);

  /**
   * @brief remove a mod by index
   *
   * this physically deletes the specified mod from the disc and updates the ModInfo collection
   * but not other structures that reference mods
   * @param index index of the mod to delete
   * @return true if removal was successful, fals otherwise
   **/
  static bool removeMod(unsigned int index);

  /**
   * @brief retrieve the mod index by the mod name
   *
   * @param name name of the mod to look up
   * @return the index of the mod. If the mod doesn't exist, UINT_MAX is returned
   **/
  static unsigned int getIndex(const QString &name);

  /**
   * @brief find the first mod that fulfills the filter function (after no particular order)
   * @param filter a function to filter by. should return true for a match
   * @return index of the matching mod or UINT_MAX if there wasn't a match
   */
  static unsigned int findMod(const boost::function<bool (ModInfo::Ptr)> &filter);

  /**
   * @brief check a bunch of mods for updates
   * @param modIDs list of mods (Nexus Mod IDs) to check for updates
   * @return
   */
  static void checkChunkForUpdate(const std::vector<int> &modIDs, QObject *receiver);

  /**
   * @brief query nexus information for every mod and update the "newest version" information
   **/
  static int checkAllForUpdate(QObject *receiver);

  /**
   * @brief create a new mod from the specified directory and add it to the collection
   * @param dir directory to create from
   * @return pointer to the info-structure of the newly created/added mod
   */
  static ModInfo::Ptr createFrom(const QDir &dir, MOShared::DirectoryEntry **directoryStructure);

  /**
   * @brief create a new "foreign-managed" mod from a tuple of plugin and archives
   * @param espName name of the plugin
   * @param bsaNames names of archives
   * @return a new mod
   */
  static ModInfo::Ptr createFromPlugin(const QString &espName, bool displayForeign);

  static ModInfo::Ptr createFromSteam(const QString &modPath, const QString &steamKey);

  /**
   * @brief retieve a name for one of the CONTENT_ enums
   * @param contentType the content value
   * @return a display string
   */
  static QString getContentTypeName(int contentType);

  virtual bool isEmpty() const;

  /**
   * @brief sets the file this mod was installed from
   * @param fileName name of the file
   * TODO exists only to remain compatible with the plugin interface, internal code should use
   *      the feature() interface
   */
  virtual void setInstallationFile(const QString &fileName);

  /**
   * @brief delete the mod from the disc. This does not update the global ModInfo structure or indices
   * @return true if the mod was successfully removed
   **/
  bool remove();

  /**
   * @brief clear all caches held for this mod
   */
  virtual void clearCaches() {}

  /**
   * @brief getter for the mod name
   *
   * @return the mod name
   **/
  QString name() const;

  /**
   * @brief set the name of this mod
   *
   * set the name of this mod. This will also update the name of the
   * directory that contains this mod
   *
   * @param name new name of the mod
   * @return true on success, false if the new name can't be used (i.e. because the new
   *         directory name wouldn't be valid)
   **/
  virtual bool setName(const QString &name) override;

  /**
   * @brief getter for an internal name. This is usually the same as the regular name, but with special mod types it might be
   *        this is used to distinguish between mods that have the same visible name
   * @return internal mod name
   */
  virtual QString internalName() const;

  /**
   * @return true if the mod is always enabled
   */
  virtual bool alwaysEnabled() const { return false; }

  /**
   * @return true if the mod can be updated
   */
  virtual bool canBeUpdated() const { return false; }

  /**
   * @return a list of flags for this mod
   */
  virtual std::set<EModFlag> flags() const;

  /**
   * @brief test if the specified flag is set for this mod
   * @param flag the flag to test
   * @return true if the flag is set, false otherwise
   */
  bool hasFlag(EModFlag flag) const;

  virtual std::vector<EContent> getContents() const;

  /**
   * @brief test if the mods contains the specified content
   * @param content the content to test
   * @return true if the content is there, false otherwise
   */
  bool hasContent(ModInfo::EContent content) const;

  /**
   * @return an indicator if and how this mod should be highlighted by the UI
   */
  virtual int getHighlight() const
  {
    if (hasFeature<MOBase::ModFeature::OverwriteLocation>()) {
      return (isValid() ? HIGHLIGHT_IMPORTANT : HIGHLIGHT_INVALID) | HIGHLIGHT_CENTER;
    } else {
      return isValid() ? HIGHLIGHT_NONE : HIGHLIGHT_INVALID;
    }
  }

  /**
   * @return list of names of ini tweaks
   **/
  std::vector<QString> getIniTweaks() const;

  /**
   * @return a description about the mod, to be displayed in the ui
   */
  virtual QString getDescription() const;

  /**
   * @return time this mod was created (file time of the directory)
   */
  virtual QDateTime creationTime() const;

  /**
   * @return true if this mod is considered "valid", that is: it contains data used by the game
   **/
  bool isValid() const { return m_Valid; }

  /**
   * @brief updates the valid-flag for this mod
   */
  void testValid();

  /**
   * @return retrieve list of mods (as mod index) that are overwritten by this one. Updates may be delayed
   */
  virtual std::set<unsigned int> getModOverwrite() { return std::set<unsigned int>(); }

  /**
   * @return list of mods (as mod index) that overwrite this one. Updates may be delayed
   */
  virtual std::set<unsigned int> getModOverwritten() { return std::set<unsigned int>(); }

  QStringList archives() const;

  virtual void setVersion(const MOBase::VersionInfo &version) override;
  virtual void setNewestVersion(const MOBase::VersionInfo &version) override;
  virtual void setIsEndorsed(bool endorsed) override;
  virtual void setRepoModID(int modId) override;
  virtual void addNexusCategory(int categoryId) override;
  virtual QString absolutePath() const override;

  template <typename T>
  bool hasFeature() const {
    return m_Features.find(typeid(T)) != m_Features.end();
  }

  template <typename T>
  const T *feature() const {
    auto iter = m_Features.find(typeid(T));
    if (iter != m_Features.end()) {
      try {
        return reinterpret_cast<T*>(iter->second.get());
      } catch (const boost::bad_any_cast&) {
        qCritical("failed to retrieve feature type %s (got %s)",
                  typeid(T).name(), typeid(iter->second).name());
        return nullptr;
      }
    } else {
      return nullptr;
    }
  }

  template <typename T>
  T *feature() {
    auto iter = m_Features.find(typeid(T));
    if (iter != m_Features.end()) {
      try {
        return reinterpret_cast<T*>(iter->second.get());
      } catch (const boost::bad_any_cast&) {
        qCritical("failed to retrieve feature type %s (got %s)",
                  typeid(T).name(), typeid(iter->second).name());
        return nullptr;
      }
    } else {
      return nullptr;
    }
  }

  template <typename T>
  void addFeature(T *feature) {
    feature->setMod(this);
    std::shared_ptr<T> featurePtr(feature);
    m_Features[typeid(T)] = featurePtr;
    // this could be implemented more generic with std::tr2::bases in C++14
    if (std::is_base_of<MOBase::ModFeature::DiskLocation, T>()) {
      m_Features[typeid(MOBase::ModFeature::DiskLocation)] = featurePtr;
    }
    if (std::is_base_of<MOBase::ModFeature::Repository, T>()) {
      m_Features[typeid(MOBase::ModFeature::Repository)] = featurePtr;
    }
    connect(feature, &MOBase::ModFeature::Feature::saveRequired, this,
            [this] () { m_MetaInfoChanged = true; });
  }

protected:

  ModInfo(const QString &name, const std::vector<EModFlag> flags = {});

  static void updateIndices();

private:

  static void createFromOverwrite();

private:

  static QMutex s_Mutex;
  static std::map<QString, std::vector<unsigned int>> s_ModsByModID;
  static int s_NextID;

  static std::vector<ModInfo::Ptr> s_Collection;
  static std::map<QString, unsigned int> s_ModsByName;

  QString m_Name;
  QString m_Description;

  bool m_Valid;

  bool m_MetaInfoChanged { false };

  std::map<std::type_index, std::shared_ptr<MOBase::ModFeature::Feature>> m_Features;

  std::set<EModFlag> m_Flags;

  mutable std::vector<ModInfo::EContent> m_CachedContent;

  mutable QTime m_LastContentCheck;

};


#endif // MODINFO_H
