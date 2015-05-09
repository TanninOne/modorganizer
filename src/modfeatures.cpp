#include "modfeatures.h"
#include "categories.h"
#include "modinfo.h"
#include "messagedialog.h"
#include "json.h"
#include <gameinfo.h>
#include <directoryentry.h>
#include <boost/filesystem.hpp>
#include <appconfig.h>
#include <scopeguard.h>
#include <QApplication>
#include <QUrlQuery>


using namespace MOShared;

namespace MOBase {

namespace ModFeature {

void Note::set(const QString &note)
{
  m_Note = note;
  emit saveRequired();
}

QString Note::get() const
{
  return m_Note;
}

void Note::saveMeta(QSettings &settings)
{
  settings.setValue("notes", m_Note);
}

void Note::readMeta(QSettings &settings)
{
  m_Note = settings.value("notes", "").toString();
}

std::set<EModFlag> Note::flags() const {
  std::set<EModFlag> result;
  if (m_Note.length() != 0) {
    result.insert(EModFlag::NOTES);
  }
  return result;
}

void Categorized::saveMeta(QSettings &settings) {
  std::set<int> temp = m_Categories;
  temp.erase(m_PrimaryCategory);
  settings.setValue("category", QString::number(m_PrimaryCategory) + "," + SetJoin(temp, ","));
}

void Categorized::readMeta(QSettings &settings) {
  QString categoriesString = settings.value("category", "").toString();

  QStringList categories = categoriesString.split(',', QString::SkipEmptyParts);
  for (QStringList::iterator iter = categories.begin(); iter != categories.end(); ++iter) {
    bool ok = false;
    int categoryID = iter->toInt(&ok);
    if (categoryID < 0) {
      // ignore invalid id
      continue;
    }
    if (ok && (categoryID != 0) && (CategoryFactory::instance().categoryExists(categoryID))) {
      m_Categories.insert(categoryID);
      if (iter == categories.begin()) {
        m_PrimaryCategory = categoryID;
      }
    }
  }
}

bool Categorized::isSet(int categoryID) const
{
  for (std::set<int>::const_iterator iter = m_Categories.begin(); iter != m_Categories.end(); ++iter) {
    if ((*iter == categoryID) ||
        (CategoryFactory::instance().isDecendantOf(*iter, categoryID))) {
      return true;
    }
  }

  return false;
}

void Categorized::set(int categoryID, bool active)
{
  if (active) {
    m_Categories.insert(categoryID);
    if (m_PrimaryCategory == -1) {
      m_PrimaryCategory = categoryID;
    }
  } else {
    std::set<int>::iterator iter = m_Categories.find(categoryID);
    if (iter != m_Categories.end()) {
      m_Categories.erase(iter);
    }
    if (categoryID == m_PrimaryCategory) {
      if (m_Categories.size() == 0) {
        m_PrimaryCategory = -1;
      } else {
        m_PrimaryCategory = *(m_Categories.begin());
      }
    }
  }
  emit saveRequired();
}

const std::set<int> &Categorized::getCategories() const
{
  return m_Categories;
}

int Categorized::primary() const
{
  return m_PrimaryCategory;
}

void Categorized::setPrimary(int categoryID)
{
  m_PrimaryCategory = categoryID;
  emit saveRequired();
}



void Conflicting::clearCaches()
{
  m_LastConflictCheck = QTime();
}

Conflicting::Conflicting(DirectoryEntry **directoryStructure)
  : Feature()
  , m_DirectoryStructure(directoryStructure)
{}

std::set<EModFlag> Conflicting::flags() const
{
  std::set<EModFlag> result;
  switch (isConflicted()) {
    case CONFLICT_MIXED: {
      result.insert(EModFlag::CONFLICT_MIXED);
    } break;
    case CONFLICT_OVERWRITE: {
      result.insert(EModFlag::CONFLICT_OVERWRITE);
    } break;
    case CONFLICT_OVERWRITTEN: {
      result.insert(EModFlag::CONFLICT_OVERWRITTEN);
    } break;
    case CONFLICT_REDUNDANT: {
      result.insert(EModFlag::CONFLICT_REDUNDANT);
    } break;
    default: { /* NOP */ }
  }
  return result;
}


void Conflicting::doConflictCheck() const
{
  m_OverwriteList.clear();
  m_OverwrittenList.clear();
  bool regular = false;

  int dataID = 0;
  if ((*m_DirectoryStructure)->originExists(L"data")) {
    dataID = (*m_DirectoryStructure)->getOriginByName(L"data").getID();
  }

  std::wstring name = ToWString(mod()->name());
  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntry::Ptr> files = origin.getFiles();
    // for all files in this origin
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      const std::vector<int> &alternatives = (*iter)->getAlternatives();
      if ((alternatives.size() == 0)
          || (alternatives[0] == dataID)) {
        // no alternatives -> no conflict
        regular = true;
      } else {
        if ((*iter)->getOrigin() != origin.getID()) {
          FilesOrigin &altOrigin = (*m_DirectoryStructure)->getOriginByID((*iter)->getOrigin());
          unsigned int altIndex = ModInfo::getIndex(ToQString(altOrigin.getName()));
          m_OverwrittenList.insert(altIndex);
        }
        // for all non-providing alternative origins
        for (auto altIter = alternatives.begin(); altIter != alternatives.end(); ++altIter) {
          if ((*altIter != dataID) && (*altIter != origin.getID())) {
            FilesOrigin &altOrigin = (*m_DirectoryStructure)->getOriginByID(*altIter);
            unsigned int altIndex = ModInfo::getIndex(ToQString(altOrigin.getName()));
            if (origin.getPriority() > altOrigin.getPriority()) {
              m_OverwriteList.insert(altIndex);
            } else {
              m_OverwrittenList.insert(altIndex);
            }
          }
        }
      }
    }
  }

  m_LastConflictCheck = QTime::currentTime();

  if (!m_OverwriteList.empty() && !m_OverwrittenList.empty())
    m_CurrentConflictState = CONFLICT_MIXED;
  else if (!m_OverwriteList.empty())
    m_CurrentConflictState = CONFLICT_OVERWRITE;
  else if (!m_OverwrittenList.empty()) {
    if (!regular) {
      m_CurrentConflictState = CONFLICT_REDUNDANT;
    } else {
      m_CurrentConflictState = CONFLICT_OVERWRITTEN;
    }
  }
  else m_CurrentConflictState = CONFLICT_NONE;
}

Conflicting::EConflictType Conflicting::isConflicted() const
{
  // this is costy so cache the result
  QTime now = QTime::currentTime();
  if (m_LastConflictCheck.isNull() || (m_LastConflictCheck.secsTo(now) > 10)) {
    doConflictCheck();
  }

  return m_CurrentConflictState;
}

bool Conflicting::isRedundant() const
{
  std::wstring name = ToWString(mod()->name());
  if ((*m_DirectoryStructure)->originExists(name)) {
    FilesOrigin &origin = (*m_DirectoryStructure)->getOriginByName(name);
    std::vector<FileEntry::Ptr> files = origin.getFiles();
    bool ignore = false;
    for (auto iter = files.begin(); iter != files.end(); ++iter) {
      if ((*iter)->getOrigin(ignore) == origin.getID()) {
        return false;
      }
    }
    return true;
  } else {
    return false;
  }
}

void Versioned::set(const MOBase::VersionInfo &version)
{
  m_Version = version;
  emit saveRequired();
}

MOBase::VersionInfo Versioned::get() const
{
  return m_Version;
}

void MOBase::ModFeature::Versioned::saveMeta(QSettings &settings)
{
  settings.setValue("version", m_Version.canonicalString());
}

void MOBase::ModFeature::Versioned::readMeta(QSettings &settings)
{
  m_Version.parse(settings.value("version", "").toString());
}

void Repository::setModId(const QString &modId)
{
  m_ModId = modId;
  emit saveRequired();
}

QString Repository::modId() const
{
  return m_ModId;
}

QString Repository::modName() const
{
  ModInfo *modInfo = dynamic_cast<ModInfo*>(mod());
  return modInfo->internalName();
}

void Repository::setVersion(const MOBase::VersionInfo &version)
{
  if (version != m_Version) {
    m_Version = version;
    emit saveRequired();
  }
}

void Repository::setDescription(const QString &description)
{
  if (qHash(description) != qHash(m_Description)) {
    m_Description = description;
    emit saveRequired();
  }
}

QString Repository::description() const
{
  return m_Description;
}

MOBase::VersionInfo Repository::version() const
{
  return m_Version;
}

bool Repository::updateAvailable() const
{
  if (m_IgnoredVersion.isValid() && (m_IgnoredVersion == m_Version)) {
    return false;
  }
  ModInfo *modInfo = dynamic_cast<ModInfo*>(mod());
  if (!modInfo->hasFeature<Versioned>()) {
    return false;
  }
  return m_Version.isValid() && (m_Version < modInfo->feature<Versioned>()->get());
}

bool Repository::downgradeAvailable() const
{
  if (m_IgnoredVersion.isValid() && (m_IgnoredVersion == m_Version)) {
    return false;
  }
  ModInfo *modInfo = dynamic_cast<ModInfo*>(mod());
  if (!modInfo->hasFeature<Versioned>()) {
    return false;
  }
  return m_Version.isValid() && (m_Version < modInfo->feature<Versioned>()->get());
}

bool Repository::updateIgnored() const
{
  return m_IgnoredVersion == m_Version;
}

bool Repository::canBeUpdated() const
{
  return m_ModId.toInt() >= 0;
}

QDateTime Repository::lastQueryTime() const
{
  return m_LastQuery;
}

void Endorsable::saveMeta(QSettings &settings) {
  if (m_EndorsedState != ENDORSED_UNKNOWN) {
    settings.setValue("endorsed", m_EndorsedState);
  }
}

void Endorsable::readMeta(QSettings &settings) {
  if (settings.contains("endorsed")) {
    if (settings.value("endorsed").canConvert<int>()) {
      switch (settings.value("endorsed").toInt()) {
        case ENDORSED_FALSE: m_EndorsedState = ENDORSED_FALSE;   break;
        case ENDORSED_TRUE:  m_EndorsedState = ENDORSED_TRUE;    break;
        case ENDORSED_NEVER: m_EndorsedState = ENDORSED_NEVER;   break;
        default:             m_EndorsedState = ENDORSED_UNKNOWN; break;
      }
    } else {
      m_EndorsedState = settings.value("endorsed", false).toBool() ? ENDORSED_TRUE
                                                                   : ENDORSED_FALSE;
    }
  }
}

std::set<EModFlag> Endorsable::flags() const
{
  std::set<EModFlag> result;
  if (endorsedState() == ENDORSED_FALSE) {
    result.insert(EModFlag::NOTENDORSED);
  }
  return result;
}

void Endorsable::setIsEndorsed(bool endorsed)
{
  if (m_EndorsedState != ENDORSED_NEVER) {
    m_EndorsedState = endorsed ? ENDORSED_TRUE : ENDORSED_FALSE;
    emit saveRequired();
  }
}

void Endorsable::setNeverEndorse()
{
  m_EndorsedState = ENDORSED_NEVER;
  emit saveRequired();
}

void Endorsable::endorse(bool doEndorse)
{
  if (doEndorse != (m_EndorsedState == ENDORSED_TRUE)) {
    ModInfo *modInfo = dynamic_cast<ModInfo*>(mod());
    modInfo->feature<NexusRepository>()->setEndorsed(doEndorse);
  }
}

Endorsable::EEndorsedState Endorsable::endorsedState() const
{
  return ENDORSED_NEVER;
}

void Endorsable::setEndorsedState(Endorsable::EEndorsedState state) {
  if (state != m_EndorsedState) {
    m_EndorsedState = state;
    emit saveRequired();
  }
}



void Installed::saveMeta(QSettings &settings)
{
  settings.setValue("installationFile", m_InstallationFile);

  settings.beginWriteArray("installedFiles");
  int idx = 0;
  for (auto iter = m_InstalledFileIDs.begin(); iter != m_InstalledFileIDs.end(); ++iter) {
    settings.setArrayIndex(idx++);
    settings.setValue("modid", iter->first);
    settings.setValue("fileid", iter->second);
  }
  settings.endArray();
}

void Installed::readMeta(QSettings &settings)
{
  m_InstallationFile = settings.value("installationFile", "").toString();

  int numFiles = settings.beginReadArray("installedFiles");
  for (int i = 0; i < numFiles; ++i) {
    settings.setArrayIndex(i);
    m_InstalledFileIDs.insert(std::make_pair(settings.value("modid").toInt(),
                                             settings.value("fileid").toInt()));
  }
}

void Installed::addInstalledFile(int modId, int fileId)
{
  m_InstalledFileIDs.insert(std::make_pair(modId, fileId));
  emit saveRequired();
}

QString Installed::getInstallationFile() const
{
  return m_InstallationFile;
}

void Installed::setInstallationFile(const QString &fileName)
{
  m_InstallationFile = fileName;
  emit saveRequired();
}

QStringList Installed::archives() const
{
  QStringList result;
  QDir dir(this->absolutePath());
  for (const QString &archive : dir.entryList(QStringList("*.bsa"))) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

QString Installed::absolutePath() const
{
  return m_Path;
}

void Installed::setPath(const QString &path)
{
  m_Path = path;
}


ForeignInstalled::ForeignInstalled(const QString &referenceFile
                                   , bool displayForeign)
  : DiskLocation()
  , m_ReferenceFile(referenceFile)
{
  QFileInfo file(referenceFile);
  QDir dataDir(QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())) + "/data");
  m_Archives.clear();
  for (const QString archiveName : dataDir.entryList(QStringList() << file.baseName() + "*.bsa")) {
    m_Archives.append(dataDir.absoluteFilePath(archiveName));
  }
}

QString ForeignInstalled::absolutePath() const
{
  return QDir::fromNativeSeparators(ToQString(GameInfo::instance().getGameDirectory())) + "/data";
}

QStringList ForeignInstalled::archives() const
{
  return m_Archives;
}

QStringList ForeignInstalled::stealFiles() const
{
  QStringList result = m_Archives;
  result.append(m_ReferenceFile);
  return result;
}

SteamInstalled::SteamInstalled(const QString &modPath)
  : m_Path(modPath)
{
}

QString SteamInstalled::absolutePath() const
{
  return m_Path;
}

QStringList SteamInstalled::files() const
{
  QStringList result;
  QDir dir(absolutePath());
  for (const QString &archive : dir.entryList()) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

QStringList SteamInstalled::archives() const
{
  QStringList result;
  QDir dir(absolutePath());
  for (const QString &archive : dir.entryList(QStringList("*.bsa"))) {
    result.append(this->absolutePath() + "/" + archive);
  }
  return result;
}

QString OverwriteLocation::absolutePath() const
{
  return QDir::fromNativeSeparators(qApp->property("dataPath").toString() + "/" + QString::fromStdWString(AppConfig::overwritePath()));
}

std::set<EModFlag> OverwriteLocation::flags() const
{
  return { EModFlag::OVERWRITE };
}

QStringList OverwriteLocation::archives() const
{
  QStringList result;
  QString path = absolutePath();
  QDir dir(path);
  for (const QString &archive : dir.entryList(QStringList("*.bsa"))) {
    result.append(path + "/" + archive);
  }
  return result;
}

void Repository::saveMeta(QSettings &settings) {
  settings.setValue("ignoredVersion", m_IgnoredVersion.canonicalString());
  settings.setValue("newestVersion", m_Version.canonicalString());
  settings.setValue("repository", name());
  settings.setValue("modid", m_ModId);
  settings.setValue("nexusDescription", m_Description);
  settings.setValue("lastNexusQuery", m_LastQuery.toString(Qt::ISODate));
}

void Repository::readMeta(QSettings &settings)
{
  m_ModId          = settings.value("modid", -1).toInt();
  m_Version        = settings.value("newestVersion", "").toString();
  m_IgnoredVersion = settings.value("ignoredVersion", "").toString();
  m_Description    = settings.value("nexusDescription", "").toString();
  m_LastQuery      = QDateTime::fromString(settings.value("lastNexusQuery", "").toString(),
                                           Qt::ISODate);
}

void Repository::ignoreUpdate(bool ignore)
{
  if (ignore) {
    m_IgnoredVersion = m_Version;
  } else {
    m_IgnoredVersion.clear();
  }
  emit saveRequired();
}

void Repository::markQueried() {
  m_LastQuery = QDateTime::currentDateTime();
}


NexusRepository::NexusRepository()
  : m_NexusBridge()
{
  connect(&m_NexusBridge, SIGNAL(descriptionAvailable(int,QVariant,QVariant))
          , this, SLOT(nxmDescriptionAvailable(int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(endorsementToggled(int,QVariant,QVariant))
          , this, SLOT(nxmEndorsementToggled(int,QVariant,QVariant)));
  connect(&m_NexusBridge, SIGNAL(requestFailed(int,int,QVariant,QString))
          , this, SLOT(nxmRequestFailed(int,int,QVariant,QString)));
}

bool NexusRepository::updateInfo()
{
  if (modId() > 0) {
    m_NexusBridge.requestDescription(modId().toInt(), QVariant());
    return true;
  }
  return false;
}

int NexusRepository::translateCategory(const QString &categoryId)
{
  return CategoryFactory::instance().resolveNexusID(categoryId.toInt());
}

QString NexusRepository::name() const
{
  return "Nexus";
}

void NexusRepository::nxmDescriptionAvailable(int, QVariant, QVariant resultData)
{
  QVariantMap result = resultData.toMap();

qDebug("%s", qPrintable(QStringList(result.keys()).join(", ")));
  setVersion(VersionInfo(result["version"].toString()));
  setDescription(result["description"].toString());

  ModInfo *modInfo = dynamic_cast<ModInfo*>(mod());
  Endorsable *endorsable = modInfo->feature<Endorsable>();

  if ((endorsable->endorsedState() != Endorsable::ENDORSED_NEVER) && (result.contains("voted_by_user"))) {
    endorsable->setEndorsedState(result["voted_by_user"].toBool() ? Endorsable::ENDORSED_TRUE
                                                                  : Endorsable::ENDORSED_FALSE);
  }
  markQueried();
  emit saveRequired();
  emit modDetailsUpdated(true);
}

void NexusRepository::nxmEndorsementToggled(int, QVariant, QVariant resultData)
{
  ModInfo *modInfo = dynamic_cast<ModInfo*>(mod());
  modInfo->feature<Endorsable>()->setEndorsedState(
        resultData.toBool() ? Endorsable::ENDORSED_TRUE
                            : Endorsable::ENDORSED_FALSE);
  emit saveRequired();
  emit modDetailsUpdated(true);
}

void NexusRepository::nxmRequestFailed(int, int, QVariant userData, const QString &errorMessage)
{
  QString fullMessage = errorMessage;
  if (userData.canConvert<int>() && (userData.toInt() == 1)) {
    fullMessage += "\nNexus will reject endorsements within 15 Minutes of a failed attempt, the error message may be misleading.";
  }
  if (QApplication::activeWindow() != nullptr) {
    MessageDialog::showMessage(fullMessage, QApplication::activeWindow());
  }
  emit modDetailsUpdated(false);
}

void NexusRepository::setEndorsed(bool endorsed)
{
  m_NexusBridge.requestToggleEndorsement(modId().toInt(), endorsed, QVariant(1));
}

SteamRepository::SteamRepository(const QString &steamKey)
  : m_Title()
  , m_UpdateReply(nullptr)
{
  setModId(steamKey);
}

QString SteamRepository::name() const
{
  return "Steam";
}

bool SteamRepository::updateInfo()
{
  QNetworkRequest request(QUrl("http://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/"));
  QNetworkAccessManager *manager = new QNetworkAccessManager();
  QSslConfiguration config = QSslConfiguration::defaultConfiguration();
  request.setSslConfiguration(config);
  request.setHeader(QNetworkRequest::ServerHeader, "application/json");
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  QUrlQuery postData;
  postData.addQueryItem("format", "json");
  postData.addQueryItem("itemcount", "1");
  postData.addQueryItem(QString("publishedfileids[0]"), modId());
  m_UpdateReply = manager->post(request, postData.query(QUrl::FullyEncoded).toUtf8());
  connect(m_UpdateReply, &QNetworkReply::finished, [this] () {
    QNetworkReply *reply = this->m_UpdateReply;

    ON_BLOCK_EXIT([this] () {
      this->postUpdate();
    });

    QString data = reply->readAll();

    bool ok;
    QVariant result = QtJson::parse(data, ok);

    if (result.isValid() && ok) {
      QVariantMap response = result.toMap().value("response", QVariant()).toMap();
      if (response.value("resultcount").toInt() == 1) {
        QVariantMap details = response.value("publishedfiledetails").toList()[0].toMap();
        if (this->mod() != nullptr) {
          QString title = details.value("title").toString();
        }
        this->setTitle(details.value("title").toString());
        this->setDescription(details.value("description").toString());
        int timestamp = details.value("time_updated").toInt();
        this->setVersion(VersionInfo(timestamp, 0, 0));
      } else {
        qWarning() << "No results in workshop response for " << this->modId();
      }
    } else {
      qWarning() << "Failed to parse workshop response for " << this->modId();
    }
  });

  connect(m_UpdateReply, static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error),
          [this] (QNetworkReply::NetworkError) {
    QNetworkReply *reply = this->m_ErrorReply;

    ON_BLOCK_EXIT([this] () {
      this->postUpdate();
    });

    qWarning() << "Failed to query workshop info for "
               << this->m_SteamKey << ": "
               << reply->errorString();
    delete reply;
  });
  return true;
}

int SteamRepository::translateCategory(const QString &categoryId)
{
  return 0;
}

QString SteamRepository::modName() const
{
  return m_Title;
}

void SteamRepository::setTitle(const QString &title)
{
  m_Title = title;
}

void SteamRepository::postUpdate()
{
  m_UpdateReply->deleteLater();
  m_UpdateReply = nullptr;
}

Positioning::Positioning(Positioning::ECheckable checkable,
                         Positioning::EPosition position)
  : Feature()
  , m_Checkable(checkable)
  , m_Position(position)
{
}

bool Positioning::isPositionFixed() const
{
  return m_Position != EPosition::USER_POSITIONABLE;
}

Positioning::ECheckable Positioning::checkable() const
{
  return m_Checkable;
}

Positioning::EPosition Positioning::position() const
{
  return m_Position;
}

}

}

