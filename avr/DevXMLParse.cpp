#include <stdint.h>

#include "DevXMLParse.h"
#include <cstdio>
#include <sstream>

xmlXPathObjectPtr
DXMLParser::Eval(xmlNodePtr root, const std::string &str) {

  /* Evaluate xpath expression */
  xmlXPathObjectPtr xpathObj;
  if (root == NULL)
    xpathObj = xmlXPathEvalExpression((xmlChar*)str.c_str(), xpathCtx);
  else
    xpathObj= xmlXPathNodeEval(root, (xmlChar *)str.c_str(), xpathCtx);
  if(xpathObj == NULL) {
    fprintf(stderr,"Error: unable to evaluate xpath expression \"%s\"\n",str.c_str());
    return NULL;
  }
  return xpathObj;
}

DXMLParser::DXMLParser(std::string XMLFile)
{
  ParserInitialized = false;
  xmlInitParser();
  xConfigFile = XMLFile;
}

DXMLParser::~DXMLParser()
{
  if (xpathCtx != NULL)
    xmlXPathFreeContext(xpathCtx);
  if (xDoc != NULL)
    xmlFreeDoc(xDoc);

  xmlCleanupParser();
}

bool DXMLParser::Initialize(void)
{
  xDoc = xmlParseFile(xConfigFile.c_str());
  if (xDoc == NULL)
    return false;

  xpathCtx = xmlXPathNewContext(xDoc);
  if(xpathCtx == NULL) {
    fprintf(stderr,"Error: unable to create new XPath context\n");
    return NULL;
  }

  /* Register namespaces */
  if (xmlXPathRegisterNs(xpathCtx, (const unsigned char *)"edc", (const unsigned char *)"http://crownking/edc") < 0) {
    fprintf(stderr,"Error: failed to register namespaces list \n");
    return NULL;
  }

  ParserInitialized = true;
  return ParserInitialized;

  /*
  // create the DOM parser
  xParser = new XercesDOMParser;
  xParser->setValidationScheme(XercesDOMParser::Val_Never);
  xParser->setDoNamespaces(true);
  xParser->setDoSchema(false);
  xParser->setLoadExternalDTD(false);

  try
    {
      xParser->parse(xConfigFile.c_str());
      // get the DOM representation
      xDoc = xParser->getDocument();
      if (xDoc == NULL)
        return false;
      // get the root element
      xRoot = xDoc->getDocumentElement();
      xNSResolver = xDoc->createNSResolver(xRoot);
      ParserInitialized = true;
    }
  catch (...)
    {
      ParserInitialized = false;
    }

  return ParserInitialized;
  */
}

const char *
DXMLParser::GetAttribute(xmlNodePtr node, const char *AttrName) {
  return (const char *)xmlGetProp(node, (const xmlChar*)AttrName);
}

bool
DXMLParser::GetConfigReferenceValues(xmlNodePtr FieldNode, std::string FieldName,
                                     ConfigSpec &Config)
{
  xmlXPathObjectPtr xpathObj = Eval(FieldNode, "edc:DCRFieldSemantic");
  uint32_t nRefValues = xpathObj->nodesetval->nodeNr;
  for (uint8_t i = 0; i < nRefValues; i++)
    {
      xmlNodePtr RefValNode = xpathObj->nodesetval->nodeTab[i];
      std::string RefValName;

      /* Get reference value.
         If it is SET or CLEAR then use as it is, Other wise prefix field name.
         i.e. <FieldName>_<ConfigName>
         Ref: XC8-1723
       */
      const char* ConfigVal = GetAttribute(RefValNode, "cname");
      if ((std::string(ConfigVal).compare("SET") == 0) ||
          (std::string(ConfigVal).compare("CLEAR") == 0))
        {
          RefValName = std::string(ConfigVal);
        }
      else
        {
          RefValName = FieldName;
          RefValName.append(1, '_');
          RefValName.append(ConfigVal);
        }

      std::string Str = GetAttribute(RefValNode, "when");
      uint8_t const_pos = Str.find_last_of('=');
      uint32_t RefValConst = strtol (Str.substr(const_pos+1).c_str(), NULL, 0);
      Config.AddRefValue(RefValName, RefValConst);
    }

  xmlXPathFreeObject(xpathObj);
  return true;
}

bool
DXMLParser::GetFieldsConfig (xmlNodePtr RegNode, ConfigReg &Register)
{
  /* If no bitfields, then create a configuration with register name itself.
     Any value of register size is allowed.
     E.g. atmega4809 APPEND, BOOTEND fuse registers  */
  if (!RegNode->children)
    {
      ConfigSpec config(Register.rname, Register.width, 0, true);
      config.SetDefaultValue(Register.factorydefault);
      Register.configs.push_back(config);
      return true;
    }

  xmlXPathObjectPtr xpathObj = Eval(RegNode, "edc:DCRModeList/edc:DCRMode");

  // Return false if no or more than one DCRMode node found
  if (xpathObj->nodesetval->nodeNr != 1)
    return false;

  // get children of DCRMode (DCRFieldDef or AdjustPoint)
  xmlNodePtr Node = xpathObj->nodesetval->nodeTab[0]->xmlChildrenNode;
  uint8_t BitPos = 0;
  for (xmlNodePtr FieldNode = Node; FieldNode; FieldNode = FieldNode->next)
    {
      if (FieldNode->type != XML_ELEMENT_NODE) continue;

      uint8_t FieldSizeInBits = 0;
      if (xmlStrcmp(FieldNode->name, (const xmlChar*)"AdjustPoint") == 0)
        {
          std::string Str = GetAttribute(FieldNode, "offset");
          FieldSizeInBits = strtol (Str.c_str(), NULL, 0);
          ConfigSpec config(FieldSizeInBits, BitPos, false);
          BitPos += FieldSizeInBits;
          config.SetDefaultValue(Register.factorydefault);
          Register.configs.push_back(config);
        }
      else
        {
          std::string FieldName = GetAttribute(FieldNode, "cname");
          std::string Str = GetAttribute(FieldNode, "nzwidth");
          uint32_t FieldSizeInBits = strtol(Str.c_str(), NULL, 0);
          ConfigSpec config(FieldName, FieldSizeInBits, BitPos, true);
          BitPos += FieldSizeInBits;
          config.SetDefaultValue(Register.factorydefault);
          if (!GetConfigReferenceValues(FieldNode, FieldName, config))
            return false;
          Register.configs.push_back(config);
        }
    }

  xmlXPathFreeObject(xpathObj);
  return true;
}

bool
DXMLParser::GetRegisterConfig (xmlNodePtr SectorNode, ConfigSpace &Space)
{
  xmlNodePtr Node = SectorNode->xmlChildrenNode;
  uint8_t nRegParsed = 0;
  uint32_t LastRegAddr = 0;
  for (xmlNodePtr RegNode = Node; RegNode; RegNode = RegNode->next)
    {
      if (RegNode->type != XML_ELEMENT_NODE) continue;

      const char *Str = GetAttribute(RegNode, "_addr");
      uint32_t RegAddr = strtol (Str, NULL, 0);
      uint32_t RegSizeInBits = 0;
      // Check if it is reserved byte
      if (xmlStrcmp(RegNode->name, (const xmlChar*)"AdjustPoint") == 0)
        {
          Str = GetAttribute(RegNode, "offset");
          RegSizeInBits = (strtol (Str, NULL, 0) * 8);
          ConfigReg Reg(RegAddr, RegSizeInBits, (1 << RegSizeInBits) - 1);
          nRegParsed += RegSizeInBits / 8;
          Space.registers.push_back(Reg);
        }
      else
        {
          const char *RegName = GetAttribute(RegNode, "cname");
          Str = GetAttribute(RegNode, "factorydefault");
          uint32_t RegDefaultVal = strtol(Str, NULL, 0);
          Str = GetAttribute(RegNode, "nzwidth");
          RegSizeInBits = strtol(Str, NULL, 0);
          if (RegSizeInBits % 8)
            {
              fprintf (stderr, "Unsupported config register size (%d), "
                       "expected to be multiple of 8 bits.\n", RegSizeInBits);
              return false;
            }
          nRegParsed += RegSizeInBits / 8;
          ConfigReg Reg(RegName, RegAddr, RegSizeInBits, RegDefaultVal);
          if (!GetFieldsConfig(RegNode, Reg))
            return false;

          Space.registers.push_back(Reg);
        }
      LastRegAddr = RegAddr;
    }

  /* If the last bytes are reserved, there may not be any <AdjustPoint> nodes
     in PIC file. Create reserved byte registers for remaining config
     registers. */
  uint8_t lRemRegs = Space.width - nRegParsed;

  while (lRemRegs > 0)
  {
	  LastRegAddr++;
	  ConfigReg Reg(LastRegAddr, 8, 0xff);
	  Space.registers.push_back(Reg);
	  lRemRegs--;
  }

  return true;
}

bool
DXMLParser::GetSectorConfig (std::string SpaceName, std::string SectorName,
                             ConfigSpace &Space, char *Error)
{
  std::ostringstream ostr;
  ostr << "//edc:ConfigFuseSector[@edc:regionid='" << SectorName << "']";

  xmlXPathObjectPtr xpathObj = Eval(NULL, ostr.str().c_str());

  if(xpathObj == NULL) {
    return false;
  }

  unsigned int nConfigFuseSector = xpathObj->nodesetval->nodeNr;
  if (nConfigFuseSector != 1) // FIXME currently assumes only one valid space
  {
    sprintf (Error, "No unique FUSES region in ConfigFuseSector, '%d' nodes found.",
             nConfigFuseSector);
    return false;
  }

  uint32_t SectorAddress = 0; // FIXME assumed 0 as address, not used anywhere

  xmlNodePtr FusesSectorNode = xpathObj->nodesetval->nodeTab[0];
  const char *Str = GetAttribute(FusesSectorNode, "beginaddr");
  uint32_t saddress = strtol(Str, NULL, 0);
  Str = GetAttribute(FusesSectorNode, "endaddr");
  uint32_t eaddress = strtol(Str, NULL, 0);
  uint32_t FuseSectorWidth = eaddress - saddress;

  Space.SetValues(SectorName, SectorAddress, FuseSectorWidth);
  if (!GetRegisterConfig (FusesSectorNode, Space))
    {
      sprintf (Error, "Error in reading config registers.");
      return false;
    }

  xmlXPathFreeObject(xpathObj);
  return true;
}

#ifdef MAIN_TEST

std::vector<class ConfigSpace> AvrConfigSpaces;

int main(int argc, char*argv[])
{
  if (argc != 2)
    {
      std::cout << "Usage: argv[0] <device xml file>\n";
      return 0;
    }
  DXMLParser xmlParser (argv[1]);
  bool status = xmlParser.Initialize();

  if (!status) {
    std::cout << "XML parser initialization failed.\n";
    return 1;
  }

  char ErrorMsg[255] = "";
  ConfigSpace FusesSpace;
  status = xmlParser.GetSectorConfig("FusesSpace", "FUSES", FusesSpace, ErrorMsg);

  if (!status)
    {
      std::cout << "Error: " << ErrorMsg << std::endl;
      return 1;
    }

  FusesSpace.Print(std::cout);

  for (ConfigIterator C = FusesSpace.registers[0].configs.begin();
       C != FusesSpace.registers[0].configs.end(); C++)
    {
      if (C->cname == std::string("SUT_CKSEL"))
        C->SetValue ("3");
      else if (C->cname == std::string("CKOUT"))
        C->SetValue("CLEAR");
      else if (C->cname == std::string("CKDIV8"))
        C->SetValue("SET");
    }

  std::cout << "LOW: def 0x" << std::hex << FusesSpace.registers[0].factorydefault
            << " user 0x" << std::hex << FusesSpace.registers[0].GetValue() << std::endl;
  for (ConfigIterator C = FusesSpace.registers[0].configs.begin();
       C != FusesSpace.registers[0].configs.end(); C++)
    {
      std:: cout << "  " << C->cname << " 0x" << std::hex << (C->user_value << C->bitPos)
                 << "(" << (uint32_t)C->user_value << ")" << std::endl;
    }

  return 0;
}

#endif
