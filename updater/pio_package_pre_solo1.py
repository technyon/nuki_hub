import shutil
import os

if not os.path.exists('src/networkDevices/'):
    os.mkdir('src/networkDevices')

shutil.copy("../src/main.cpp", "src/main.cpp")
shutil.copy("../src/Config.h", "src/Config.h")
shutil.copy("../src/Logger.h", "src/Logger.h")
shutil.copy("../src/NukiNetwork.h", "src/NukiNetwork.h")
shutil.copy("../src/Ota.h", "src/Ota.h")
shutil.copy("../src/PreferencesKeys.h", "src/PreferencesKeys.h")
shutil.copy("../src/RestartReason.h", "src/RestartReason.h")
shutil.copy("../src/WebCfgServer.h", "src/WebCfgServer.h")
shutil.copy("../src/WebCfgServerConstants.h", "src/WebCfgServerConstants.h")

shutil.copy("../src/Logger.cpp", "src/Logger.cpp")
shutil.copy("../src/NukiNetwork.cpp", "src/NukiNetwork.cpp")
shutil.copy("../src/Ota.cpp", "src/Ota.cpp")
shutil.copy("../src/WebCfgServer.cpp", "src/WebCfgServer.cpp")

shutil.copy("../src/networkDevices/EthLan8720Device.h", "src/networkDevices/EthLan8720Device.h")
shutil.copy("../src/networkDevices/IPConfiguration.h", "src/networkDevices/IPConfiguration.h")
shutil.copy("../src/networkDevices/NetworkDevice.h", "src/networkDevices/NetworkDevice.h")
shutil.copy("../src/networkDevices/W5500Device.h", "src/networkDevices/W5500Device.h")
shutil.copy("../src/networkDevices/WifiDevice.h", "src/networkDevices/WifiDevice.h")

shutil.copy("../src/networkDevices/EthLan8720Device.cpp", "src/networkDevices/EthLan8720Device.cpp")
shutil.copy("../src/networkDevices/IPConfiguration.cpp", "src/networkDevices/IPConfiguration.cpp")
shutil.copy("../src/networkDevices/NetworkDevice.cpp", "src/networkDevices/NetworkDevice.cpp")
shutil.copy("../src/networkDevices/W5500Device.cpp", "src/networkDevices/W5500Device.cpp")
shutil.copy("../src/networkDevices/WifiDevice.cpp", "src/networkDevices/WifiDevice.cpp")