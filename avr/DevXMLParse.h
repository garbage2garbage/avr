#include <iostream>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "avr-device-config.h"

class DXMLParser
{
 public:
	xmlDocPtr xDoc;
  xmlXPathContextPtr xpathCtx;

	bool ParserInitialized;
  std::string        xConfigFile;

  DXMLParser(std::string XMLFile);
  ~DXMLParser();
  bool Initialize(void);
  bool GetSectorConfig (std::string SpaceName, std::string SectorName,
                        ConfigSpace &Space, char *Error);
 private:
  bool GetRegisterConfig (xmlNodePtr SectorNode, ConfigSpace &Space);
  bool GetFieldsConfig (xmlNodePtr RegNode, ConfigReg &Register);
	bool GetConfigReferenceValues(xmlNodePtr FieldNode, std::string FName,
																ConfigSpec &Config);
  xmlXPathObjectPtr Eval(xmlNodePtr root, const std::string &str);
  const char * GetAttribute(xmlNodePtr node, const char *AttrName);
};

