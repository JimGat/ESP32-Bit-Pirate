#include "DependencyProvider.h"

DependencyProvider::DependencyProvider(ITerminalView &terminalView, IDeviceView &deviceView,
                                       IInput &terminalInput, IInput &deviceInput,
                                       LittleFsService &littleFsService)
    : terminalView(terminalView),
      deviceView(deviceView),
      terminalInput(terminalInput),
      deviceInput(deviceInput),
      littleFsService(littleFsService),

      // Services
      sdService(),
      nvsService(),
      ledService(),
      uartService(),
      i2cService(),
      oneWireService(),
      twoWireService(),
      threeWireService(),
      infraredService(),
      spiService(),
      pinService(),
      bluetoothService(),
      wifiService(),
      wifiScannerService(),
      i2sService(),
      sshService(),
      jtagService(),
      canService(),
      systemService(),
      ethernetService(),
      httpService(),
      telnetService(),
      modbusService(),
      subGhzService(),
      rfidService(),
      rf24Service(),
      usbService(),
      cellService(),
      fmService(),

      // Transformers
      commandTransformer(),
      instructionTransformer(),
      argTransformer(),
      webRequestTransformer(),
      jsonTransformer(),
      infraredTransformer(),
      subGhzTransformer(),
      profileTransformer(),
      atTransformer(),

      // Managers
      commandHistoryManager(),
      binaryAnalyzer(terminalView, terminalInput),
      userInputManager(terminalView, terminalInput, argTransformer),
      subGhzAnalyzer(),
      pinAnalyzer(pinService),
      aliasManager(),

      // Shells
      sdCardShell(sdService, terminalView, terminalInput, argTransformer, userInputManager),
      spiFlashShell(spiService, terminalView, terminalInput, argTransformer, userInputManager, binaryAnalyzer),
      spiEepromShell(spiService, terminalView, terminalInput, argTransformer, userInputManager, binaryAnalyzer),
      smartCardShell(twoWireService, terminalView, terminalInput, argTransformer, userInputManager),
      universalRemoteShell(terminalView, terminalInput, infraredService, argTransformer, userInputManager),
      ibuttonShell(terminalView, terminalInput, userInputManager, argTransformer, oneWireService),
      i2cEepromShell(terminalView, terminalInput, i2cService, argTransformer, userInputManager, binaryAnalyzer),
      uartAtShell(terminalView, terminalInput, userInputManager, argTransformer, uartService),
      threeWireEepromShell(terminalView, terminalInput, userInputManager, threeWireService, argTransformer),
      sysInfoShell(terminalView, terminalInput, deviceView, userInputManager, argTransformer, systemService, littleFsService, wifiService),
      modbusShell(terminalView, terminalInput, argTransformer, userInputManager, modbusService),
      oneWireEepromShell(terminalView, terminalInput, oneWireService, argTransformer, userInputManager, binaryAnalyzer),
      guideShell(terminalView, terminalInput, userInputManager),
      helpShell(terminalView, terminalInput, userInputManager),
      uartEmulationShell(terminalView, terminalInput, uartService, argTransformer, userInputManager),
      profileShell(terminalView, terminalInput, userInputManager, littleFsService, profileTransformer),
      cellCallShell(terminalView, terminalInput, userInputManager, argTransformer, atTransformer, cellService),
      cellSmsShell(terminalView, terminalInput, userInputManager, argTransformer, atTransformer, cellService),
      fmBroadcastShell(terminalView, terminalInput, userInputManager, argTransformer, fmService),
      usbAdapterShell(terminalView, terminalInput, userInputManager, nvsService),

      // Selectors
      horizontalSelector(deviceView, deviceInput),

      // Configurators
      terminalTypeConfigurator(horizontalSelector),

      // Controllers
      uartController(terminalView, terminalInput, deviceView, deviceInput, uartService, sdService, hdUartService, argTransformer, userInputManager, uartAtShell, helpShell, uartEmulationShell),
      i2cController(terminalView, terminalInput, i2cService, argTransformer, userInputManager, i2cEepromShell, helpShell),
      oneWireController(terminalView, terminalInput, oneWireService, argTransformer, userInputManager, ibuttonShell, oneWireEepromShell, helpShell),
      infraredController(terminalView, terminalInput, deviceView, infraredService, littleFsService, i2cService, argTransformer, infraredTransformer, userInputManager, universalRemoteShell, helpShell),
      utilityController(terminalView, deviceView, terminalInput, pinService, i2sService, userInputManager, pinAnalyzer, aliasManager, argTransformer, commandTransformer, sysInfoShell, guideShell, helpShell, profileShell),
      hdUartController(terminalView, terminalInput, deviceInput, hdUartService, uartService, argTransformer, userInputManager, helpShell),
      spiController(terminalView, terminalInput, spiService, sdService, argTransformer, userInputManager, binaryAnalyzer, sdCardShell, spiFlashShell, spiEepromShell, helpShell),
      jtagController(terminalView, terminalInput, jtagService, userInputManager, helpShell, usbAdapterShell),
      twoWireController(terminalView, terminalInput, userInputManager, twoWireService, smartCardShell, helpShell),
      threeWireController(terminalView, terminalInput, userInputManager, threeWireService, argTransformer, threeWireEepromShell, helpShell),
      dioController(terminalView, terminalInput, deviceView, pinService, argTransformer, helpShell, userInputManager),
      ledController(terminalView, terminalInput, ledService, argTransformer, userInputManager, helpShell),
      bluetoothController(terminalView, terminalInput, deviceInput, bluetoothService, argTransformer, userInputManager, helpShell),
      i2sController(terminalView, terminalInput, i2sService, argTransformer, userInputManager, helpShell),
      wifiController(terminalView, deviceView, terminalInput, deviceInput, wifiService, wifiScannerService, ethernetService, sshService, netcatService, nmapService, icmpService, nvsService, httpService, telnetService, argTransformer, jsonTransformer, userInputManager, modbusShell, helpShell),
      canController(terminalView, terminalInput, userInputManager, canService, argTransformer, helpShell),
      subGhzController(terminalView, terminalInput, deviceView, subGhzService, pinService, i2sService, littleFsService, argTransformer, subGhzTransformer, userInputManager, subGhzAnalyzer, helpShell),
      rfidController(terminalView, terminalInput, rfidService, userInputManager, argTransformer, helpShell),
      rf24Controller(terminalView, terminalInput, deviceView, rf24Service, pinService, argTransformer, userInputManager, helpShell),
      ethernetController(terminalView, deviceView, terminalInput, deviceInput, wifiService, wifiScannerService, ethernetService, sshService, netcatService, nmapService, icmpService, nvsService, httpService, telnetService, argTransformer, jsonTransformer, userInputManager, modbusShell, helpShell),
      usbController(terminalView, terminalInput, deviceInput, usbService, argTransformer, userInputManager, helpShell, usbAdapterShell),
      cellController(terminalView, terminalInput, cellService, argTransformer, atTransformer, userInputManager, helpShell, cellCallShell, cellSmsShell),
      fmController(terminalView, terminalInput, deviceView, fmService, argTransformer, userInputManager, helpShell, fmBroadcastShell),
      expanderController(terminalView, terminalInput, uartService, argTransformer, userInputManager, helpShell)
{
}

// Accessors for core components
ITerminalView &DependencyProvider::getTerminalView() { return terminalView; }
void DependencyProvider::setTerminalView(ITerminalView &view) { terminalView = view; };
IDeviceView &DependencyProvider::getDeviceView() { return deviceView; }
IInput &DependencyProvider::getTerminalInput() { return terminalInput; }
IInput &DependencyProvider::getDeviceInput() { return deviceInput; }

// Services
SdService &DependencyProvider::getSdService() { return sdService; }
NvsService &DependencyProvider::getNvsService() { return nvsService; }
LedService &DependencyProvider::getLedService() { return ledService; }
I2cService &DependencyProvider::getI2cService() { return i2cService; }
UartService &DependencyProvider::getUartService() { return uartService; }
OneWireService &DependencyProvider::getOneWireService() { return oneWireService; }
TwoWireService &DependencyProvider::getTwoWireService() { return twoWireService; }
InfraredService &DependencyProvider::getInfraredService() { return infraredService; }
UsbS3Service &DependencyProvider::getUsbService() { return usbService; }
SpiService &DependencyProvider::getSpiService() { return spiService; }
HdUartService &DependencyProvider::getHdUartService() { return hdUartService; }
PinService &DependencyProvider::getPinService() { return pinService; }
WifiService &DependencyProvider::getWifiService() { return wifiService; }
BluetoothService &DependencyProvider::getBluetoothService() { return bluetoothService; }
I2sService &DependencyProvider::getI2sService() { return i2sService; }
SshService &DependencyProvider::getSshService() { return sshService; }
NetcatService &DependencyProvider::getNetcatService() { return netcatService; }
NmapService &DependencyProvider::getNmapService() { return nmapService; }
ICMPService &DependencyProvider::getICMPService() { return icmpService; }
JtagService &DependencyProvider::getJtagService() { return jtagService; }
CanService &DependencyProvider::getCanService() { return canService; }
ModbusService &DependencyProvider::getModbusService() { return modbusService; }
SystemService &DependencyProvider::getSystemService() { return systemService; }
EthernetService &DependencyProvider::getEthernetService() { return ethernetService; }
SubGhzService &DependencyProvider::getSubGhzService() { return subGhzService; }
RfidService &DependencyProvider::getRfidService() { return rfidService; }
Rf24Service &DependencyProvider::getRf24Service() { return rf24Service; }
LittleFsService &DependencyProvider::getLittleFsService() { return littleFsService; }
CellService &DependencyProvider::getCellService() { return cellService; }
FmService &DependencyProvider::getFmService() { return fmService; }

// Controllers
UartController &DependencyProvider::getUartController() { return uartController; }
I2cController &DependencyProvider::getI2cController() { return i2cController; }
OneWireController &DependencyProvider::getOneWireController() { return oneWireController; }
UtilityController &DependencyProvider::getUtilityController() { return utilityController; }
InfraredController &DependencyProvider::getInfraredController() { return infraredController; }
UsbS3Controller &DependencyProvider::getUsbController() { return usbController; }
HdUartController &DependencyProvider::getHdUartController() { return hdUartController; }
SpiController &DependencyProvider::getSpiController() { return spiController; }
JtagController &DependencyProvider::getJtagController() { return jtagController; }
TwoWireController &DependencyProvider::getTwoWireController() { return twoWireController; }
ThreeWireController &DependencyProvider::getThreeWireController() { return threeWireController; }
DioController &DependencyProvider::getDioController() { return dioController; }
LedController &DependencyProvider::getLedController() { return ledController; }
WifiController &DependencyProvider::getWifiController() { return wifiController; }
BluetoothController &DependencyProvider::getBluetoothController() { return bluetoothController; }
I2sController &DependencyProvider::getI2sController() { return i2sController; }
CanController &DependencyProvider::getCanController() { return canController; }
EthernetController &DependencyProvider::getEthernetController() { return ethernetController; }
SubGhzController &DependencyProvider::getSubGhzController() { return subGhzController; }
RfidController &DependencyProvider::getRfidController() { return rfidController; }
Rf24Controller &DependencyProvider::getRf24Controller() { return rf24Controller; }
CellController &DependencyProvider::getCellController() { return cellController; }
FmController &DependencyProvider::getFmController() { return fmController; }
ExpanderController &DependencyProvider::getExpanderController() { return expanderController; }

// Transformers
TerminalCommandTransformer &DependencyProvider::getCommandTransformer() { return commandTransformer; }
InstructionTransformer &DependencyProvider::getInstructionTransformer() { return instructionTransformer; }
ArgTransformer &DependencyProvider::getArgTransformer() { return argTransformer; }
WebRequestTransformer &DependencyProvider::getWebRequestTransformer() { return webRequestTransformer; }
JsonTransformer &DependencyProvider::getJsonTransformer() { return jsonTransformer; }
AtTransformer &DependencyProvider::getAtTransformer() { return atTransformer; }
PinoutTransformer &DependencyProvider::getPinoutTransformer() { return pinoutTransformer; }

// Managers
CommandHistoryManager &DependencyProvider::getCommandHistoryManager() { return commandHistoryManager; }
UserInputManager &DependencyProvider::getUserInputManager() { return userInputManager; }
BinaryAnalyzer &DependencyProvider::getBinaryAnalyzer() { return binaryAnalyzer; }
SubGhzAnalyzer &DependencyProvider::getSubGhzAnalyzer() { return subGhzAnalyzer; }
PinAnalyzer &DependencyProvider::getPinAnalyzer() { return pinAnalyzer; }
AliasManager &DependencyProvider::getAliasManager() { return aliasManager; }

// Shells
SdCardShell &DependencyProvider::getSdCardShell() { return sdCardShell; }
UniversalRemoteShell &DependencyProvider::getUniversalRemoteShell() { return universalRemoteShell; }
I2cEepromShell &DependencyProvider::getI2cEepromShell() { return i2cEepromShell; }
SpiFlashShell &DependencyProvider::getSpiFlashShell() { return spiFlashShell; }
SpiEepromShell &DependencyProvider::getSpiEepromShell() { return spiEepromShell; }
SmartCardShell &DependencyProvider::getSmartCardShell() { return smartCardShell; }
ThreeWireEepromShell &DependencyProvider::getThreeWireEepromShell() { return threeWireEepromShell; }
IbuttonShell &DependencyProvider::getIbuttonShell() { return ibuttonShell; }
UartAtShell &DependencyProvider::getUartAtShell() { return uartAtShell; }
SysInfoShell &DependencyProvider::getSysInfoShell() { return sysInfoShell; }
ModbusShell &DependencyProvider::getModbusShell() { return modbusShell; }
OneWireEepromShell &DependencyProvider::getOneWireEepromShell() { return oneWireEepromShell; }
GuideShell &DependencyProvider::getGuideShell() { return guideShell; }
HelpShell &DependencyProvider::getHelpShell() { return helpShell; }
UartEmulationShell &DependencyProvider::getUartEmulationShell() { return uartEmulationShell; }
ProfileShell &DependencyProvider::getProfileShell() { return profileShell; }
CellCallShell &DependencyProvider::getCellCallShell() { return cellCallShell; }
CellSmsShell &DependencyProvider::getCellSmsShell() { return cellSmsShell; }
FmBroadcastShell &DependencyProvider::getFmBroadcastShell() { return fmBroadcastShell; }
UsbAdapterShell &DependencyProvider::getUsbAdapterShell() { return usbAdapterShell; }

// Selectors
HorizontalSelector &DependencyProvider::getHorizontalSelector() { return horizontalSelector; }

// Config
TerminalTypeConfigurator &DependencyProvider::getTerminalTypeConfigurator() { return terminalTypeConfigurator; }

// Disable interfaces
void DependencyProvider::disableAllProtocols()
{
  // getUartService().end();
  // getHdUartService().end();
  // getI2cService().end();
  // getSpiService().end();
  // getI2sService().end();
  // getTwoWireService().end();
}
