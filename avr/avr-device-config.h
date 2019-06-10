#ifndef AVR_DEVICE_CONFIG_H
#define AVR_DEVICE_CONFIG_H 1
#include <stdint.h>

#include <vector>
#include <string>
#include <iostream>
class ConfigSpec
{
  public:
    std::string    cname;  // edc:DCRFieldDef[edc:cname] e.g. SUT_CKSEL
    uint8_t        width; // ...[edc:nzwidth] e.g. 0x6
    uint8_t        mask;  // (1 << width) - 1 ==> (1<<6)-1 ==> 0x3F
		uint8_t        bitPos;
    uint8_t        default_value;  // edc:DCRDef[edc:factorydefault] & mask
    std::vector< std::pair<std::string,uint8_t> >
                   reference_values; // edc:DCRFieldSemantic[edc:cname],[edc:when]
    bool           isEditable;  // true if editable
    bool           isModified;  // true if user modified
    uint8_t        user_value;  // constant/ ID evaluated to one of reference_values

    struct reg_t*  config_reg;    // pointer register where this config exists
    unsigned int   reg_position;  // bit position in register

    // create config with name and width
    ConfigSpec(std::string name, uint8_t nbits, uint8_t bitpos,
							 bool canEdit);
    ConfigSpec(uint8_t nbits, uint8_t bpos, bool canEdit);
    void SetDefaultValue(uint32_t RegDefaultVal);
    void AddRefValue(std::string id, uint8_t val);

    bool IsPermitted() { return isEditable; }
    bool SetValue(std::string value);
		uint8_t GetValue();
		void Print (std::ostream &stream);
};

class ConfigReg
{
 public:
  std::string  rname;   // register name
  uint8_t      offset; // address in space
  uint8_t      width;   // width in bits (multiple of 8)
  uint32_t     factorydefault; // default value

  ConfigReg(std::string name, uint8_t addr, uint8_t bits,
						uint32_t defval);
  ConfigReg(uint8_t addr, uint8_t bits, uint32_t defval);
  std::vector<class ConfigSpec> configs;
  void Print (std::ostream &stream);
  uint32_t GetValue();
};

class ConfigSpace
{
 public:
	std::string  sname;
	unsigned int address;
	unsigned int width; // size in bytes; endaddress - 1
	std::vector<class ConfigReg> registers;
	bool isReferenced;
	ConfigSpace();
	void SetValues(std::string name, uint32_t addr, uint32_t nbytes);
	void Print (std::ostream &stream);
};

class AvrDeviceConfig
{
 public:
	std::string ConfigFile;
	bool AreConfigsLoaded;
	bool AreConfigsChanged;
	std::vector<class ConfigSpace> Spaces;
	AvrDeviceConfig();
	//void SetConfigFile(std::string ConfigFile);
	bool LoadConfigurations(std ::string config_file);
	bool SetConfig(std::string cname, std::string value, char* err);
};

extern int avr_load_configuration_values (std::string filename);
extern void avr_handle_configuration_setting (std::string config_name,
                                              std::string value);
extern void avr_output_configurations (void);
extern AvrDeviceConfig DeviceConfigurations;

typedef std::vector<class ConfigSpace>::iterator SpaceIterator;
typedef std::vector<class ConfigReg>::iterator RegIterator;
typedef std::vector<class ConfigSpec>::iterator ConfigIterator;
typedef std::vector<std::pair<std::string,uint8_t> >::iterator RefValIterator;

#endif /* AVR_DEVICE_CONFIG_H */

