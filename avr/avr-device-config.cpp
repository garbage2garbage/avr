#include <stdlib.h>
#include <fstream>

#include "avr-device-config.h"
#include "DevXMLParse.h"

#include <assert.h>
#include <cstdio>

ConfigSpace::ConfigSpace() : isReferenced(false) {}

void
ConfigSpace::SetValues(std::string name, uint32_t addr, uint32_t nbytes)
{
  sname = name;
  address = addr;
  width = nbytes;
}

void
ConfigSpace::Print(std::ostream &Stream)
{
  char buffer[255] = "";
  sprintf (buffer, "Space: %s (0x%x, %d)\n", sname.c_str(), address, width);

  Stream << buffer;

  //Stream << "Space: " << sname << " (0x" << std::hex << address << ", " << width << ")\n";
  for (RegIterator R = registers.begin(); R != registers.end(); R++)
    R->Print(Stream);
}

ConfigReg::ConfigReg(std::string name, uint8_t offset_index,
                     uint8_t bits, uint32_t defval)
{
  rname = name;
  offset = offset_index;
  width = bits;
  factorydefault = defval;
}

ConfigReg::ConfigReg(uint8_t offset_index, uint8_t bits, uint32_t defval)
{
  // Create a register with name reserved for edc:AdjustPoint
  rname = std::string("reserved");
  offset = offset_index,
  width = bits;
  factorydefault = defval;
}

uint32_t
ConfigReg::GetValue()
{
  uint32_t RegValue = factorydefault;

  for (ConfigIterator C = configs.begin(); C != configs.end(); C++)
    {
      // clear config bits in a byte and set config bits
      RegValue = RegValue & (~(C->mask << C->bitPos));
      RegValue |= (C->GetValue() << C->bitPos);
    }

  return RegValue & ((1 << width) - 1);
}

void
ConfigReg::Print(std::ostream &Stream)
{
  char buffer[255] = "";
  sprintf (buffer, "  Reg: %s (offset: 0x%x width: 0x%x default: 0x%x)\n",
           rname.c_str(), offset, width, factorydefault);
  Stream << buffer;
  //Stream << "  Reg: " << rname << " (offset: " << std::hex << offset << " width: "
  //     << std::hex << width << " default: 0x" << std::hex << factorydefault << ")\n";
  for (ConfigIterator C = configs.begin(); C != configs.end(); C++)
    {
      C->Print(Stream);
    }
}

ConfigSpec::ConfigSpec(std::string name, uint8_t nbits,
                       uint8_t bpos, bool canEdit=true)
  :isModified(false)
{
  cname = name;
  width = nbits;
  mask = (1 << width) - 1;
  bitPos = bpos;
  isEditable = canEdit;
}

ConfigSpec::ConfigSpec(uint8_t nbits, uint8_t bpos, bool canEdit=true)
  :isModified(false)
{
  cname = std::string("reserved");
  width = nbits;
  mask = (1 << width) - 1;
  bitPos = bpos;
  isEditable = canEdit;
}

void ConfigSpec::SetDefaultValue(uint32_t RegDefaultVal)
{
  //default_value = ((regFactoryValue & (mask << bpos)) >> bpos);
  default_value = (RegDefaultVal & (mask << bitPos)) >> bitPos;
}

void ConfigSpec::AddRefValue(std::string id, uint8_t val)
{
  reference_values.push_back (std::make_pair(id, val));
}

// Set value for config. Returns false if not valid value
bool ConfigSpec::SetValue(std::string value)
{
  bool isNumber = value.find_first_not_of("0123456789") == std::string::npos;
  uint32_t int_val = strtol(value.c_str(), NULL, 0);

  /* If no reference values found, allow any NUMERIC value
     but check the limits. Ref: XC8-1741  */
  if (reference_values.empty())
    {
      if (!isNumber) return false;
      if (int_val > mask) return false;
      user_value = int_val;
      isModified = true;

      return true;
    }

  bool IsRefFound = false;
  for (RefValIterator R = reference_values.begin();
       R != reference_values.end(); R++)
    {
      if (isNumber ? R->second != int_val :
                     R->first.compare(value) != 0)
        continue;
      IsRefFound = true;
      user_value = R->second;
      isModified = true;
    }
  return IsRefFound;
}

uint8_t ConfigSpec::GetValue()
{
  if (isModified == false)
    return default_value;

  return user_value;
}

void
ConfigSpec::Print(std::ostream &Stream)
{
  char buffer[255]="";
  sprintf (buffer, "    config: %s %d bits offset: %d default: 0x%x\n",
           cname.c_str(), width, bitPos, default_value);

  Stream << buffer;

  for (RefValIterator Val = reference_values.begin();
       Val != reference_values.end(); Val++)
    {
      char buf[128]="";
      sprintf(buf, "      (%s, 0x%x)\n", Val->first.c_str(), Val->second);
      Stream << buf;
    }
}

AvrDeviceConfig::AvrDeviceConfig()
{
  AreConfigsLoaded = false;
  AreConfigsChanged = false;
}

bool AvrDeviceConfig::LoadConfigurations(std::string File)
{
  if (File.empty()) return false;

  std::ifstream cfgfile(File.c_str(), std::ifstream::in);
  if (!cfgfile.good()) return false;

  ConfigFile = File;

  DXMLParser xmlParser (ConfigFile);
  bool status = xmlParser.Initialize();

  if (!status)
    {
      //cout << "XML parser initialization failed.\n";
      return false;
    }

  char ErrorMsg[255] = "";
  ConfigSpace FusesSpace;
  status = xmlParser.GetSectorConfig("FusesSpace", "FUSES", FusesSpace, ErrorMsg);

  if (!status)
    {
      //cout << "Error: " << ErrorMsg << endl;
      return false;
    }

  Spaces.push_back(FusesSpace);
  AreConfigsLoaded = true;
  return true;
}

bool AvrDeviceConfig::SetConfig(std::string config_name, std::string value,
                                char* Error)
{
  assert (AreConfigsLoaded != false);
  bool isConfigFound = false;

  for (SpaceIterator itS = Spaces.begin();
       itS != Spaces.end(); itS++)
    {
      for (RegIterator itR = itS->registers.begin();
           itR != itS->registers.end(); itR++)
        {
          for (ConfigIterator itC = itR->configs.begin();
               itC != itR->configs.end(); itC++)
            {
              if (itC->cname.compare(config_name) != 0)
                continue;

              isConfigFound = true;
              if (!itC->IsPermitted())
                {
                  sprintf (Error, "configuration setting '%s' is not writable",
                           config_name.c_str());
                  return false;
                }

              if (itC->isModified)
                {
                  sprintf (Error, "multiple definition for configuration setting '%s'",
                           config_name.c_str());
                  return false;
                }

              if (false == itC->SetValue(value))
                {
                  sprintf (Error, "unknown value for configuration '%s': '%s'",
                         config_name.c_str(), value.c_str());
                  return false;
                }

              /* return as error issued or config set.  */
              itS->isReferenced = true;
              AreConfigsChanged = true;
              return true;
            }
        }
    }

  if (!isConfigFound)
    {
      sprintf (Error, "unknown configuration setting: '%s'", config_name.c_str());
      return false;
    }

  assert (0);
}

