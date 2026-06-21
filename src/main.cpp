#ifndef UNIT_TEST

#include <Views/SerialTerminalView.h>
#include <Views/M5DeviceView.h>
#include <Views/WebTerminalView.h>
#include <Views/NoScreenDeviceView.h>
#include <Views/TembedDeviceView.h>
#include <Views/TdisplayDeviceView.h>
#include <Views/WaveshareS3GeekDeviceView.h>
#include <Views/CardputerTerminalView.h>
#include <Views/CardputerDeviceView.h>
#include <Inputs/SerialTerminalInput.h>
#include <Inputs/CardputerInput.h>
#include <Inputs/StickInput.h>
#include <Inputs/StampS3Input.h>
#include <Inputs/TembedInput.h>
#include <Inputs/TdisplayInput.h>
#include <Inputs/WaveshareS3GeekInput.h>
#include <Inputs/S3DevKitInput.h>
#include <Providers/DependencyProvider.h>
#include <Dispatchers/ActionDispatcher.h>
#include <Servers/HttpServer.h>
#include <Servers/WebSocketServer.h>
#include <Servers/DnsServer.h>
#include <Services/NvsService.h>
#include <Services/WifiService.h>
#include <Inputs/WebTerminalInput.h>
#include <Selectors/HorizontalSelector.h>
#include <Config/TerminalTypeConfigurator.h>
#include <Config/WifiTypeConfigurator.h>
#include <Config/BootModeConfigurator.h>
#include <Enums/TerminalTypeEnum.h>
#include <Serial/DefaultHostSerial.h>
#include <Serial/UartHostSerial.h>
#include <States/GlobalState.h>

/*
This file initializes the device (M5Stick / Cardputer / StampS3 / T-Embed / S3 DevKit),
selects the terminal mode (Serial, Wi-Fi Client/AP, Standalone Cardputer),
and then launches the main loop through the ActionDispatcher.

- Terminal View: the interface where the user SEES and INTERACTS with the CLI.
    * SerialTerminalView    -> text terminal via USB serial (COM/tty).
    * WebTerminalView       -> text terminal in a browser (via WebSocket).
    * CardputerTerminalView -> Cardputer LCD acts as the terminal screen.

- Device View: the interface for device’s screen (if any).
    * M5DeviceView, TembedDeviceView, CardputerDeviceView, NoScreenDeviceView, etc.
    * Used for UI elements like mode, pinout mapping, or logic traces.

- Terminal Input: how the user TYPES commands into the system.
    * SerialTerminalInput  -> keyboard input over USB serial.
    * WebTerminalInput     -> keystrokes/events from a browser WebSocket.

- Device Input: physical buttons on the device.
    * StickInput, TembedInput, StampS3Input, S3DevKitInput -> button/encoders.

- ActionDispatcher: the central loop that reads user actions,
  dispatches them to controllers/services, and keeps the system running.

- DependencyProvider: constructs and wires together the correct Views,
  Inputs, Services, and Controllers based on the current configuration.

  A typical command follows this path:

    User types a command
            |
            v
    TerminalInput
    (read characters)
            |
            v
    ActionDispatcher
    (route command/instruction)
            |
            v
    Controller
    (Parse and validate actions)
            |
            v
    Service
    (access hardware / protocol)
            |
            v
    Controller
    (Process response)
            |
            v
    TerminalView
    (display output)

*/

void setup() {
    #if DEVICE_STICKS3
        // Setup the Stick
        #include <M5Unified.h>
        auto cfg = M5.config();
        M5.begin(cfg);
        M5DeviceView deviceView;
        deviceView.setRotation(3);
        StickInput deviceInput;
        M5.Power.setExtOutput(false);
        deviceView.logo();
        deviceInput.waitPress(3000);
    #elif DEVICE_CARDPUTER
        // Setup the Cardputer
        #include <M5Unified.h>
        auto cfg = M5.config();
        M5Cardputer.begin(cfg, true);
        M5DeviceView deviceView;
        deviceView.setRotation(1);
        CardputerInput deviceInput;
        deviceView.logo();
        deviceInput.waitPress(3000);
    #elif DEVICE_M5STAMPS3
        // Setup the StampS3/AtomS3
        #include <M5Unified.h>
        auto cfg = M5.config();
        M5.begin(cfg);
        NoScreenDeviceView deviceView;
        StampS3Input deviceInput;
    #elif defined(DEVICE_TEMBEDS3) || defined(DEVICE_TEMBEDS3CC1101)
        // Setup the T-embed
        TembedDeviceView deviceView;
        TembedInput deviceInput;
        deviceView.initialize();
        deviceView.logo();
        deviceInput.waitPress(3000);
        deviceView.clear();
    #elif defined(DEVICE_TDISPLAYS3)
        TdisplayDeviceView deviceView;
        TdisplayInput deviceInput;
        deviceView.initialize();
        deviceView.logo();
        deviceInput.waitPress(3000);
        deviceView.clear();
    #elif defined(DEVICE_WAVESHARE_S3_GEEK)
        WaveshareS3GeekDeviceView deviceView;
        WaveshareS3GeekInput deviceInput;
        deviceView.initialize();
        deviceView.logo();
        deviceInput.waitPress(3000);
        deviceView.clear();
    #else
        // Fallback to S3 dev kit
        NoScreenDeviceView deviceView;
        S3DevKitInput deviceInput;
    #endif

    #if defined(DEVICE_HOST_SERIAL_UART)
        UartHostSerial hostSerial;
    #else
        DefaultHostSerial hostSerial;
    #endif

    // USB Adapter boot mode if set, otherwise continue to terminal type selection
    NvsService bootNvsService;
    BootModeConfigurator bootModeConfigurator(deviceView, deviceInput, bootNvsService, hostSerial);
    if (bootModeConfigurator.configure()) {
        return;
    }

    LittleFsService littleFsService;
    GlobalState& state = GlobalState::getInstance();

    std::string webIp = "0.0.0.0";
    TerminalTypeEnum terminalType = TerminalTypeEnum::None;

    // JARVIS AI Enabled boot path:
    // If a network was saved by the Wi-Fi connect command, automatically attach
    // to it on boot so WebSocket + REST control are reachable on the local LAN.
    // The Wi-Fi `forget` command clears these NVS credentials and disables this.
    {
        NvsService autoWifiNvsService;
        autoWifiNvsService.open();
        std::string savedSsid = autoWifiNvsService.getString(state.getNvsSsidField());
        std::string savedPassword = autoWifiNvsService.getString(state.getNvsPasswordField());
        autoWifiNvsService.close();

        if (!savedSsid.empty() && !savedPassword.empty()) {
            WifiService autoWifiService;
            if (autoWifiService.connect(savedSsid, savedPassword, 8000)) {
                terminalType = TerminalTypeEnum::WiFiClient;
                webIp = autoWifiService.getLocalIP();
                state.setTerminalIp(webIp);
            }
        }
    }

    // Select the terminal type only if boot auto-connect did not claim Wi-Fi client mode.
    if (terminalType == TerminalTypeEnum::None) {
        HorizontalSelector selector(deviceView, deviceInput);
        TerminalTypeConfigurator configurator(selector);
        terminalType = configurator.configure();
    }

    // Configure Wi-Fi if manually selected.
    if (terminalType == TerminalTypeEnum::WiFiClient || terminalType == TerminalTypeEnum::WiFiAp) {
        if (webIp == "0.0.0.0") {
            WifiTypeConfigurator wifiTypeConfigurator(deviceView, deviceInput);
            webIp = wifiTypeConfigurator.configure(terminalType);
        }

        if (webIp == "0.0.0.0") {
            terminalType = TerminalTypeEnum::SerialPort;
        } else {
            state.setTerminalIp(webIp);
        }
    }
    state.setTerminalMode(terminalType);

    switch (terminalType) {
        case TerminalTypeEnum::SerialPort: {
            // Serial View/Input
            SerialTerminalView serialView(hostSerial);
            SerialTerminalInput serialInput(hostSerial);

            // Baudrate
            auto baud = std::to_string(state.getSerialTerminalBaudRate());
            serialView.setBaudrate(state.getSerialTerminalBaudRate());

            // Build the provider for serial type and run the dispatcher loop
            // too big to fit on the stack anymore, allocated on the heap
            DependencyProvider* provider = new DependencyProvider(serialView, deviceView, serialInput, deviceInput,
                                                                  littleFsService);
            ActionDispatcher dispatcher(*provider);
            dispatcher.setup(terminalType, baud);
            dispatcher.run(); // Forever
            break;
        }
        case TerminalTypeEnum::WiFiAp:
        case TerminalTypeEnum::WiFiClient: {
            // Configure Server
            httpd_handle_t server = nullptr;
            httpd_config_t config = HTTPD_DEFAULT_CONFIG();
            config.max_uri_handlers = 16; // web UI + ws + LittleFS + automation API + captive routes
            config.lru_purge_enable = true;
            config.recv_wait_timeout = 11;
            config.send_wait_timeout = 11;

            // DNS server for captive portal if AP mode
            if (terminalType == TerminalTypeEnum::WiFiAp) {
                DnsServer::configureCaptiveDns(config);
                DnsServer::startCaptiveDns(webIp);
            }

            if (httpd_start(&server, &config) != ESP_OK) {
                return;
            }

            JsonTransformer jsonTransformer;
            WebSocketServer wsServer(server);
            HttpServer httpServer(server, littleFsService, jsonTransformer, &wsServer);

            // Web View/Input
            WebTerminalView webView(wsServer);
            WebTerminalInput webInput(wsServer);
            deviceView.loading();
            delay(7000); // let the server begin

            // Setup routes for index, ws, captive if needed
            wsServer.setupRoutes();
            httpServer.setupRoutes();
            if (terminalType == TerminalTypeEnum::WiFiAp) {
                httpServer.setupCaptivePortalRoutes(webIp);
            }

            // Build the provider for webui type and run the dispatcher loop
            // too big to fit on the stack anymore, allocated on the heap
            DependencyProvider* provider = new DependencyProvider(webView, deviceView, webInput, deviceInput,
                                                                  littleFsService);
            ActionDispatcher dispatcher(*provider);

            dispatcher.setup(terminalType, webIp);
            dispatcher.run(); // Forever
            break;
        }

        #ifdef DEVICE_CARDPUTER
        case TerminalTypeEnum::Standalone:
            // Cardputer all in one
            CardputerTerminalView standaloneView; // cardputer screen as terminal
            CardputerInput standaloneInput; // cardputer keyboard for command input
            standaloneView.initialize();
            CardputerDeviceView deviceView; // used for logic analyzer only
            S3DevKitInput deviceInput; // the G0 button of the cardputer

            // Build the provider for cardputer standalone and run the dispatcher loop
            DependencyProvider* provider = new DependencyProvider(standaloneView, deviceView, standaloneInput, deviceInput,
                                                                  littleFsService);
            ActionDispatcher dispatcher(*provider);
            dispatcher.setup(terminalType, "standalone");
            dispatcher.run(); // Forever
            break;
        #endif
    }
}

void loop() {
    // Empty as all logic is handled in dispatcher
}

#endif
