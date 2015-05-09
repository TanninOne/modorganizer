#ifndef MODFEATURES_H
#define MODFEATURES_H


#include "nexusinterface.h"
#include <modflags.h>
#include <modfeature.h>
#include <versioninfo.h>
#include <directoryentry.h>
#include <QDateTime>


class ModInfo;

namespace MOBase {

namespace ModFeature {

class Note : public Feature {

public:

  void set(const QString &note);
  QString get() const;

  virtual void saveMeta(QSettings &settings) override;

  virtual void readMeta(QSettings &settings) override;

  virtual std::set<EModFlag> flags() const override;

private:

  QString m_Note;

};

class Categorized : public Feature {

public:

  void saveMeta(QSettings &settings);

  void readMeta(QSettings &settings);

  /**
   * @brief test if the mod belongs to the specified category
   *
   * @param categoryID the category to test for.
   * @return true if the mod belongs to the specified category
   * @note this does not verify the id actually identifies a category
   **/
  bool isSet(int categoryID) const;

  /**
   * @brief assign or unassign the specified category
   *
   * Every mod can have an arbitrary number of categories assigned to it
   *
   * @param categoryID id of the category to set
   * @param active determines wheter the category is assigned or unassigned
   * @note this function does not test whether categoryID actually identifies a valid category
   **/
  void set(int categoryID, bool active);

  /**
   * @brief retrive the whole list of categories (as ids) this mod belongs to
   *
   * @return list of categories
   **/
  const std::set<int> &getCategories() const;

  /**
   * @return id of the primary category of this mod
   */
  int primary() const;

  /**
   * @brief sets the new primary category of the mod
   * @param categoryID the category to set
   */
  virtual void setPrimary(int categoryID);

private:

  int m_PrimaryCategory;
  std::set<int> m_Categories;

};

class Versioned : public Feature {

public:

  void saveMeta(QSettings &settings);

  void readMeta(QSettings &settings);

  /**
   * @brief set the version of this mod
   *
   * this can be used to overwrite the version of a mod without actually
   * updating the mod
   *
   * @param version the new version to use
   **/
  void set(const MOBase::VersionInfo &get);

  /**
   * @return version object for machine based comparisons
   **/
  virtual MOBase::VersionInfo get() const;

private:

  MOBase::VersionInfo m_Version;

};


class Repository : public Feature {

  Q_OBJECT

public:

  void saveMeta(QSettings &settings);

  void readMeta(QSettings &settings);

  /**
   * @brief set/change the repository mod id of this mod
   *
   * @param modID the repository mod id
   **/
  void setModId(const QString &modId);

  /**
   * @brief getter for the nexus mod id
   *
   * @return the nexus mod id. may be 0 if the mod id isn't known or doesn't exist
   **/
  QString modId() const;

  /**
   * @brief mod name as known in the repository
   * @return
   */
  virtual QString modName() const;

  /**
   * @return the repository from which the file was downloaded. Only relevant regular mods
   */
  virtual QString name() const = 0;

  /**
   * @brief set the newest version of this mod on the nexus
   *
   * this can be used to overwrite the version of a mod without actually
   * updating the mod
   *
   * @param version the new version to use
   * @todo this function should be made obsolete. All queries for mod information should go through
   *       this class so no public function for this change is required
   **/
  void setVersion(const MOBase::VersionInfo &version);

  /**
   * @brief changes/updates the nexus description text
   * @param description the current description text
   */
  void setDescription(const QString &description);

  QString description() const;

  /**
   * @brief getter for the newest version number of this mod
   *
   * @return newest version of the mod
   **/
  MOBase::VersionInfo version() const;

  /**
   * @brief test if there is a newer version of the mod
   *
   * test if there is a newer version of the mod. This does NOT cause
   * information to be retrieved from the nexus, it will only test version information already
   * available locally. Use checkAllForUpdate() to update this version information
   *
   * @return true if there is a newer version
   **/
  bool updateAvailable() const;

  /**
   * @brief test if there is a newer version of the mod
   *
   * test if there is a newer version of the mod. This does NOT cause
   * information to be retrieved from the nexus, it will only test version information already
   * available locally. Use checkAllForUpdate() to update this version information
   *
   * @return true if there is a newer version
   **/
  bool downgradeAvailable() const;

  /**
   * @return true if the current update is being ignored
   */
  bool updateIgnored() const;

  /**
   * @return true if the mod can be updated
   */
  bool canBeUpdated() const;

  /**
   * @return last time the repository was queried for infos on this mod
   */
  QDateTime lastQueryTime() const;

  /**
   * @brief ignore the newest version for updates
   */
  void ignoreUpdate(bool ignore);

  /**
   * @brief update the remote information about this mod
   */
  virtual bool updateInfo() = 0;

  /**
   * @param repository specific id of a category
   * @return the internal id of this category
   */
  virtual int translateCategory(const QString &categoryId) = 0;

signals:

  /**
   * @brief emitted whenever the information of a mod changes
   *
   * @param success true if the mod details were updated successfully, false if not
   **/
  void modDetailsUpdated(bool success);

protected:

  void markQueried();

private:

  QString m_ModId;
  QDateTime m_LastQuery;
  MOBase::VersionInfo m_Version;
  MOBase::VersionInfo m_IgnoredVersion;
  QString m_Description;

};


class NexusRepository : public Repository {

  Q_OBJECT

public:

  NexusRepository();

  virtual QString name() const override;

  /**
   * @brief request an update of nexus description for this mod.
   *
   * This requests mod information from the nexus. This is an asynchronous request,
   * so there is no immediate effect of this call.
   *
   * @return returns true if information for this mod will be updated, false if there is no nexus mod id to use
   **/
  virtual bool updateInfo() override;

  /**
   * @brief sets the category id from a nexus category id. Conversion to MO id happens internally
   * @param categoryID the nexus category id
   * @note if a mapping is not possible, the category is set to the default value
   */
  virtual int translateCategory(const QString &categoryId) override;

  /**
   * @brief set the endorsement state for the managed mod
   * @param endorsed new endorsement state
   */
  void setEndorsed(bool endorsed);

private slots:

  void nxmDescriptionAvailable(int modID, QVariant userData, QVariant resultData);
  void nxmEndorsementToggled(int, QVariant userData, QVariant resultData);
  void nxmRequestFailed(int modID, int fileID, QVariant userData, const QString &errorMessage);

private:

  NexusBridge m_NexusBridge;

};

class SteamRepository : public Repository {

public:

  SteamRepository(const QString &steamKey);

  ~SteamRepository() { postUpdate(); }

  virtual QString name() const override;

  virtual bool updateInfo() override;
  virtual int translateCategory(const QString &categoryId) override;

  virtual QString modName() const override;
  void setTitle(const QString &title);

private:

  void postUpdate();

private:

  QString m_Title;
  QString m_SteamKey;

  QNetworkReply *m_UpdateReply;
  QNetworkReply *m_ErrorReply;

};

class Endorsable : public Feature {

public:

  enum EEndorsedState {
    ENDORSED_FALSE,
    ENDORSED_TRUE,
    ENDORSED_UNKNOWN,
    ENDORSED_NEVER
  };

public:

  void saveMeta(QSettings &settings);

  void readMeta(QSettings &settings);

  /**
   * @return a list of flags for this mod
   */
  virtual std::set<EModFlag> flags() const override;

  /**
   * update the endorsement state for the mod. This only changes the
   * buffered state, it does not sync with Nexus
   * @param endorsed the new endorsement state
   */
  void setIsEndorsed(bool endorsed);

  /**
   * set the mod to "i don't intend to endorse". The mod will not show as unendorsed but can still
   * be endorsed
   */
  void setNeverEndorse();

  /**
   * @brief endorse or un-endorse the mod
   * @param doEndorse if true, the mod is endorsed, if false, it's un-endorsed.
   * @note if doEndorse doesn't differ from the current value, nothing happens.
   */
  void endorse(bool doEndorse);

  /**
   * @return true if the file has been endorsed on nexus
   */
  virtual EEndorsedState endorsedState() const;

  void setEndorsedState(EEndorsedState state);

private:

  EEndorsedState m_EndorsedState { ENDORSED_UNKNOWN };

};


class DiskLocation : public Feature {

public:

  virtual QString absolutePath() const = 0;

  virtual QStringList archives() const = 0;

};

class Installed : public DiskLocation {

public:

  Installed(const QString &path) : DiskLocation(), m_Path(path) {}

  void saveMeta(QSettings &settings);

  void readMeta(QSettings &settings);

  void addInstalledFile(int modId, int fileId);

  /**
   * @brief getter for the installation file
   *
   * @return file used to install this mod from
   */
  QString getInstallationFile() const;

  void setInstallationFile(const QString &fileName);

  virtual QStringList archives() const override;

  /**
   * @brief getter for the mod path
   *
   * @return the (absolute) path to the mod
   **/
  virtual QString absolutePath() const override;

  void setPath(const QString &path);

private:

  QString m_Path;
  QString m_InstallationFile;
  std::set<std::pair<int, int>> m_InstalledFileIDs;

};


class ForeignInstalled : public DiskLocation {

public:

  ForeignInstalled(const QString &referenceFile, bool displayForeign);

  virtual QString absolutePath() const override;
  virtual QStringList archives() const override;
  QStringList stealFiles() const;

private:

  QString m_ReferenceFile;
  QStringList m_Archives;

};


class OverwriteLocation : public DiskLocation {

public:

  virtual QString absolutePath() const override;
  virtual std::set<EModFlag> flags() const override;
  virtual QStringList archives() const override;

};


class SteamInstalled : public DiskLocation {

public:

  SteamInstalled(const QString &modPath);
  virtual QString absolutePath() const override;
  virtual QStringList archives() const override;

  QStringList files() const;

private:

  QString m_Path;

};


class Conflicting : public Feature {

public:

  Conflicting(MOShared::DirectoryEntry **directoryStructure);

  virtual std::set<EModFlag> flags() const override;

  /**
     * @brief clear all caches held for this mod
     */
  virtual void clearCaches();

  virtual std::set<unsigned int> getModOverwrite() { return m_OverwriteList; }

  virtual std::set<unsigned int> getModOverwritten() { return m_OverwrittenList; }

  virtual void doConflictCheck() const;

private:

  enum EConflictType {
    CONFLICT_NONE,
    CONFLICT_OVERWRITE,
    CONFLICT_OVERWRITTEN,
    CONFLICT_MIXED,
    CONFLICT_REDUNDANT
  };

private:

  /**
     * @return true if there is a conflict for files in this mod
     */
  EConflictType isConflicted() const;

  /**
     * @return true if this mod is completely replaced by others
     */
  bool isRedundant() const;

private:

  MOShared::DirectoryEntry **m_DirectoryStructure;

  mutable EConflictType m_CurrentConflictState;
  mutable QTime m_LastConflictCheck;

  mutable std::set<unsigned int> m_OverwriteList;   // indices of mods overritten by this mod
  mutable std::set<unsigned int> m_OverwrittenList; // indices of mods overwriting this mod

};


class Positioning : public Feature
{

public:

  enum class ECheckable {
    USER_CHECKABLE,
    FIXED_ACTIVE,
    FIXED_INACTIVE
  };

  enum class EPosition {
    USER_POSITIONABLE,
    FIXED_LOWEST,
    FIXED_HIGHEST
  };

public:

  Positioning(ECheckable checkable = ECheckable::USER_CHECKABLE,
              EPosition position = EPosition::USER_POSITIONABLE);

  bool isPositionFixed() const;
  ECheckable checkable() const;
  EPosition position() const;

private:

  ECheckable m_Checkable;
  EPosition m_Position;

};

}

}

#endif // MODFEATURES_H
