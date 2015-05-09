#include "acfparser.h"
#include <fstream>
#include <boost/spirit/include/qi.hpp>
#include <boost/spirit/include/phoenix_core.hpp>
#include <boost/spirit/include/phoenix_operator.hpp>
#include <boost/spirit/include/phoenix_fusion.hpp>
#include <boost/spirit/include/phoenix_stl.hpp>
#include <boost/fusion/include/adapt_struct.hpp>
#include <boost/fusion/include/std_pair.hpp>
#include <boost/phoenix/object/construct.hpp>
#include <boost/phoenix/function.hpp>

namespace qi = boost::spirit::qi;
namespace ascii = boost::spirit::ascii;


// create fusion adapter so we can use this structure
BOOST_FUSION_ADAPT_STRUCT(
    ACFPropertyTree,
    (ACFPropertyMap, m_Values)
)

// create a make_pair function that works with spirit
struct MakePair
{
     template <typename K, typename V>
     struct result
     {
         typedef std::pair<K, V> type;
     };

     template <typename K, typename V>
     typename result<K, V>::type operator()(K& k, V& v) const
     {
         return result<K, V>::type(k, v);
     }
};

boost::phoenix::function<MakePair> const make_pair;

template <typename Iterator>
struct ACFGrammer : qi::grammar<Iterator, ACFPropertyTree(), ascii::space_type>
{
  ACFGrammer() : ACFGrammer::base_type(kvList)
  {
    using namespace qi::labels;
    using boost::phoenix::at_c;
    using boost::phoenix::insert;


    // actual grammar

    quotedString = '"'
                >> qi::lexeme[+(ascii::char_ - '"') [ _val += _1 ]]
                >> '"'
                   ;

    object = '{'
          >> kvList [ at_c<0>(_val) = at_c<0>(_1) ]
          >> '}'
             ;

    value = (quotedString | object) [ _val = _1 ];

    kvList = *(quotedString
               >> value)   [ insert(at_c<0>(_val), make_pair(_1, _2)) ];

    // end actual grammar


#ifdef DEBUG_SPIRIT
    quotedString.name("quotedString");
    value.name("value");
    object.name("object");
    keyValue.name("keyValue");
    kvList.name("kvList");

    using boost::phoenix::construct;
    using boost::phoenix::val;
    boost::spirit::qi::on_error<boost::spirit::qi::fail>
    (
        kvList
      , std::cout
            << val("Error! Expecting ")
            << _4                               // what failed?
            << val(" here: \"")
            << construct<std::string>(_3, _2)   // iterators to error-pos, end
            << val("\"")
            << std::endl
    );

    debug(quotedString);
    debug(value);
    debug(object);
    debug(keyValue);
    debug(kvList);
#endif // DEBUG SPIRIT
  }

  qi::rule<Iterator, std::string(), ascii::space_type> quotedString;
  qi::rule<Iterator, ACFObject(), ascii::space_type> value;
  qi::rule<Iterator, ACFPropertyTree(), ascii::space_type> object;
  qi::rule<Iterator, ACFPropertyTree(), ascii::space_type> kvList;
};



ACFPropertyTree ACFPropertyTree::parse(std::istream &input)
{
  ACFPropertyTree output;

  ACFGrammer<std::string::const_iterator> parser;

  // can't use streambuf-iterators directly in phrase_parse because it requires forward iterators
  std::istreambuf_iterator<char> eos;
  std::string buffer(std::istreambuf_iterator<char>(input), eos);

  std::string::const_iterator iter = buffer.begin();
  std::string::const_iterator end = buffer.end();

  bool success =  qi::phrase_parse(
        iter
        , end
        , parser
        , ascii::space
        , output);
  if (!success || (iter != end)) {
    throw std::runtime_error("failed to parse ACF object");
  }
  return output;
}
