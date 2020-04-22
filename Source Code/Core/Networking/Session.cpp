#include "stdafx.h"

#include "Headers/Session.h"
#include "Headers/Server.h"
#include "Headers/OPCodesImpl.h"
#include "Headers/Patch.h"

#include "Networking/Headers/ASIO.h"

#include "Core/Resources/Headers/Resource.h"
#include <iostream>

namespace Divide {

Session::Session(boost::asio::io_service& io_service, channel& ch)
    : tcp_session_tpl(io_service, ch)
{
}

void Session::handlePacket(WorldPacket& p) {
    switch (p.opcode()) {
        case OPCodesEx::CMSG_GEOMETRY_LIST:
            HandleGeometryListOpCode(p);
            break;
        case OPCodesEx::CMSG_REQUEST_GEOMETRY:
            HandleRequestGeometry(p);
            break;
        default:
            tcp_session_tpl::handlePacket(p);
            break;
    };
}

void Session::HandleGeometryListOpCode(WorldPacket& p) {
    PatchData dataIn;
    p >> dataIn.sceneName;
    p >> dataIn.size;
    ASIO::LOG_PRINT(("Received [ CMSG_GEOMERTY_LIST ] with : " + to_stringImpl(dataIn.size) + " models").c_str());
    for (U32 i = 0; i < dataIn.size; i++) {
        stringImpl name, modelname;
        p >> name;
        p >> modelname;
        dataIn.name.push_back(name);
        dataIn.modelName.push_back(modelname);
    }

    if (!Patch::compareData(dataIn)) {
        WorldPacket r(OPCodesEx::SMSG_GEOMETRY_APPEND);

        const auto& patchData = Patch::modelData();
        r << patchData.size();
        for (const FileData& dataOut : patchData) {
            r << dataOut.ItemName;
            r << dataOut.ModelName;
            r << dataOut.Orientation.x;
            r << dataOut.Orientation.y;
            r << dataOut.Orientation.z;
            r << dataOut.Position.x;
            r << dataOut.Position.y;
            r << dataOut.Position.z;
            r << dataOut.Scale.x;
            r << dataOut.Scale.y;
            r << dataOut.Scale.z;
        }
        ASIO::LOG_PRINT(("Sending [SMSG_GEOMETRY_APPEND] with : " + to_stringImpl(patchData.size()) + " models to update").c_str());
        sendPacket(r);
        Patch::clearModelData();
    }
}

void Session::HandleRequestGeometry(WorldPacket& p) {
    stringImpl file;
    p >> file;

    ASIO::LOG_PRINT(("Sending SMSG_SEND_FILE with item: " + file).c_str());
    WorldPacket r(OPCodesEx::SMSG_SEND_FILE);
    r << (U8)0;
    sendPacket(r);
    sendFile(file);
}

};  // namespace Divide
