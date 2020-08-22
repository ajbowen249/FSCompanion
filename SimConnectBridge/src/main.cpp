#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <strsafe.h>

#include <functional>
#include <iostream>
#include <mutex>
#include <string>
#include <vector>

#include "SimConnect.h"
#include "yhs.h"

HANDLE G_SimConnectHandle = NULL;

enum DataDefinitionId {
    DEFINITION_THROTTLE = 0x01,
};

enum DataRequestId {
    THROTTLE = 0x01,
};

struct ThrottleControlData {
    double throttlePercent;
};

struct ServeSimDataContext {
    DataRequestId expectedDatum;
    DataDefinitionId definitionId;
    size_t dataSize;
    std::function<std::string(void*)> formatter;
};

static const std::string C_SimObjectRootPath = "/simobject";

static bool G_HaveConnectedToSim = false;
static std::mutex G_SimAccessMutex;
static std::vector<ServeSimDataContext> C_ServeSimDataRouteParams;

struct AwaitOpenContext {
    bool isOpen;
};

void CALLBACK awaitConnectionOpen(SIMCONNECT_RECV* pData, DWORD cbData, void* context) {
    const std::lock_guard<std::mutex> lock(G_SimAccessMutex);
    if (pData->dwID == SIMCONNECT_RECV_ID_OPEN) {
        G_HaveConnectedToSim = true;
    }
}

struct AwaitRequestedDataContext {
    DataRequestId expectedDatum;
    size_t dataSize;
    bool hasData;
    void* dataOut;
};

void CALLBACK receiveRequestedData(SIMCONNECT_RECV* pData, DWORD cbData, void* context) {
    const std::lock_guard<std::mutex> lock(G_SimAccessMutex);
    const auto ctx = (AwaitRequestedDataContext*)context;
    if (pData->dwID == SIMCONNECT_RECV_ID_SIMOBJECT_DATA) {
        SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;
        if (pObjData->dwRequestID == ctx->expectedDatum) {
            memcpy(ctx->dataOut, &pObjData->dwData, ctx->dataSize);
            ctx->hasData = true;
        }
    }
}

void serveSimData(yhsRequest* re) {
    const auto paramIndex = (size_t)yhs_get_handler_context(re);
    const auto params = C_ServeSimDataRouteParams[paramIndex];

    std::vector<unsigned char> dataReceiver(params.dataSize);

    AwaitRequestedDataContext ctx {
        params.expectedDatum,
        params.dataSize,
        false,
        &dataReceiver[0],
    };

    SimConnect_RequestDataOnSimObject(
        G_SimConnectHandle,
        params.expectedDatum,
        params.definitionId,
        SIMCONNECT_OBJECT_ID_USER,
        SIMCONNECT_PERIOD_ONCE
    );

    while (!ctx.hasData) {
        SimConnect_CallDispatch(G_SimConnectHandle, receiveRequestedData, (void*)&ctx);
    }

    const auto formatted = params.formatter(ctx.dataOut);

    yhs_begin_data_response(re,"text/plain");
    yhs_textf(re, formatted.c_str());
}

void registerSimDataGET(yhsServer* server, const std::string& path, const ServeSimDataContext& context) {
    C_ServeSimDataRouteParams.push_back(context);
    const auto index = C_ServeSimDataRouteParams.size() - 1;

    yhs_add_res_path_handler(server, (C_SimObjectRootPath + "/" + path).c_str(), &serveSimData, (void*)index);
}

int __cdecl _tmain(int argc, _TCHAR* argv[]) {
    // This is currently only meant to be single-client and is totally single-threaded.
    // Requests to GET data will dispatch a SimConnect handler and wait for the specific
    // data to be returned. Lots of room for improvement here.

    WSADATA wd;
    std::cout << "Initializing Windows Sockets" << std::endl;
    if (WSAStartup(MAKEWORD(2, 2), &wd) != 0) {
        std::cout << "Failed to initialize Windows Sockets." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Connecting to Fligh Simulator" << std::endl;
    if (!SUCCEEDED(SimConnect_Open(&G_SimConnectHandle, "SimConnect Bridge", NULL, 0, 0, 0))) {
        std::cout << "Failed to connect to Flight Simulator." << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Waiting for flight to start";
    while (G_HaveConnectedToSim) {
        std::cout << ".";
        SimConnect_CallDispatch(G_SimConnectHandle, awaitConnectionOpen, NULL);
        Sleep(500);
    }

    std::cout << std::endl;

    std::cout << "Initializing HTTP Server" << std::endl;

    yhsServer* server;
    server = yhs_new_server(8080);
    yhs_set_server_name(server,"SimConnect Bridge");

    SimConnect_AddToDataDefinition(
        G_SimConnectHandle,
        DEFINITION_THROTTLE,
        "GENERAL ENG THROTTLE LEVER POSITION:1",
        "percent"
    );

    registerSimDataGET(server, "throttle", {
        DataRequestId::THROTTLE,
        DataDefinitionId::DEFINITION_THROTTLE,
        sizeof(ThrottleControlData),
        [](void* raw) {
            return std::to_string(((ThrottleControlData*)raw)->throttlePercent);
        },
    });

    std::cout << "HTTP Server initialized" << std::endl;

    while (true) {
        yhs_update(server);
        Sleep(1);
    }

    yhs_delete_server(server);
    server = NULL;

    SimConnect_Close(G_SimConnectHandle);

    return 0;
}
