#ifndef ACFPARSER_H
#define ACFPARSER_H

#include <QVariant>
#include <istream>
#include <map>
#include <string>
#include <boost/variant.hpp>

class ACFPropertyTree; // forward declaration

// any ACF Object is either a string or another object
typedef boost::variant<boost::recursive_wrapper<ACFPropertyTree>, std::string> ACFObject;

typedef std::map<std::string, ACFObject> ACFPropertyMap;

class ACFPropertyTree
{
public:
  static ACFPropertyTree parse(std::istream &input);

  bool contains(const std::string &key) const { return m_Values.find(key) != m_Values.end(); }

  std::vector<std::string> getKeys() const {
    std::vector<std::string> result;
    for (const auto &kv : m_Values) {
      result.push_back(kv.first);
    }
    return result;
  }

  std::string getString(const std::string &key) const {
    auto iter = m_Values.find(key);
    if (iter == m_Values.end()) {
      qDebug("invalid key: %s", key.c_str());
      return std::string();
    } else {
      const std::string *ptr = boost::get<std::string>(&iter->second);
      if (ptr != nullptr) {
        return *ptr;
      } else {
        return std::string();
      }
    }
  }

  ACFPropertyTree getMap(const std::string &key) const {
    auto iter = m_Values.find(key);
    if (iter == m_Values.end()) {
      qDebug("invalid key: %s", key.c_str());
      return ACFPropertyTree();
    } else {
      const ACFPropertyTree *ptr = boost::get<ACFPropertyTree>(&iter->second);
      if (ptr != nullptr) {
        return *ptr;
      } else {
        return ACFPropertyTree();
      }
    }
  }

  ACFPropertyMap m_Values;
};


#endif // ACFPARSER_H
