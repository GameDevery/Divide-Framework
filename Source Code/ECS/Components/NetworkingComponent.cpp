#include "stdafx.h"

#include "Headers/NetworkingComponent.h"

#include "Core/Networking/Headers/LocalClient.h"
#include "Graphs/Headers/SceneGraphNode.h"

namespace Divide {

hashMapImpl<I64, NetworkingComponent*> NetworkingComponent::s_NetComponents;

NetworkingComponent::NetworkingComponent(SceneGraphNode& parentSGN, LocalClient& parentClient)
    : SGNComponent(parentSGN),
     _parentClient(parentClient),
     _resendRequired(true)
{
    // Register a receive callback with parent:
    // e.g.: _receive->bind(NetworkingComponent::onNetworkReceive);

    s_NetComponents[parentSGN.getGUID()] = this;
}

NetworkingComponent::~NetworkingComponent()
{
    s_NetComponents.erase(_parentSGN.getGUID());
}

void NetworkingComponent::flagDirty() {
    _resendRequired = true;
}

WorldPacket NetworkingComponent::deltaCompress(const WorldPacket& crt, const WorldPacket& previous) const {
    return crt;
}

WorldPacket NetworkingComponent::deltaDecompress(const WorldPacket& crt, const WorldPacket& previous) const {
    return crt;
}

void NetworkingComponent::onNetworkSend(U32 frameCountIn)  {
    if (!_resendRequired) {
        return;
    }

    WorldPacket dataOut(OPCodes::CMSG_ENTITY_UPDATE);
    dataOut << _parentSGN.getGUID();
    dataOut << frameCountIn;

    Attorney::SceneNodeNetworkComponent::onNetworkSend(_parentSGN, *_parentSGN.getNode(), dataOut);

    WorldPacket p = deltaCompress(dataOut, _previousSent);
    _previousSent = p;

    _resendRequired = _parentClient.sendPacket(dataOut);
}

void NetworkingComponent::onNetworkReceive(WorldPacket& dataIn) {
    WorldPacket p = deltaDecompress(dataIn, _previousReceived);
    _previousReceived = p;

    Attorney::SceneNodeNetworkComponent::onNetworkReceive(_parentSGN, *_parentSGN.getNode(), dataIn);
}

NetworkingComponent* NetworkingComponent::getReceiver(I64 guid) {
    hashMapImpl<I64, NetworkingComponent*>::const_iterator it = s_NetComponents.find(guid);

    if (it != std::cend(s_NetComponents)) {
        return it->second;
    }

    return nullptr;
}

void UpdateEntities(WorldPacket& p) {
    I64 tempGUID = -1;
    p >> tempGUID;

    NetworkingComponent* net = NetworkingComponent::getReceiver(tempGUID);

    if (net) {
        U32 frameCountOut = 0;
        // May be used to handle delta decompression in a specific manner;
        p >> frameCountOut;
        net->onNetworkReceive(p);
    }
}

};