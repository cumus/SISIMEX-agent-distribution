#include "UCP.h"
#include "MCP.h"
#include "Application.h"
#include "ModuleAgentContainer.h"
#include "ModuleNodeCluster.h"


// TODO: Make an enum with the states
enum State
{
	ST_UCP_INIT,

	ST_UCP_REQUESTING_ITEM,
	ST_UCP_RESOLVING_CONSTRAINT,
	ST_UCP_SENDING_CONSTRAIN,

	ST_UCP_NEGOTIATION_FINISHED
};

UCP::UCP(Node *node, uint16_t requestedItemId, uint16_t contributedItemId, const AgentLocation &uccLocation, unsigned int searchDepth) :
	Agent(node),
	_requestedItemId(requestedItemId),
	_contributedItemId(contributedItemId),
	_uccLocation(uccLocation),
	searchDepth(searchDepth),
	_negotiationAgreement(false)
{
	// TODO: Save input parameters
	setState(ST_UCP_INIT);
}

UCP::~UCP()
{
}

void UCP::update()
{
	switch (state())
	{
		// TODO: Handle states
	case ST_UCP_INIT:
	{
		// Send PacketType::RequestItem to agent
		PacketHeader oPacketHead;
		oPacketHead.packetType = PacketType::RequestItem;
		oPacketHead.srcAgentId = id();
		oPacketHead.dstAgentId = _uccLocation.agentId;

		PacketRequestItem oPacketData;
		oPacketData.requestedItemId = _requestedItemId;

		OutputMemoryStream ostream;
		oPacketHead.Write(ostream);
		oPacketData.Write(ostream);
		
		sendPacketToAgent(_uccLocation.hostIP, _uccLocation.hostPort, ostream);
		setState(ST_UCP_REQUESTING_ITEM);

		break;
	}
	case ST_UCP_RESOLVING_CONSTRAINT:
	{
		if (_mcp->negotiationFinished())
		{
			_negotiationAgreement = _mcp->negotiationAgreement();

			PacketHeader oPacketHead;
			oPacketHead.packetType = PacketType::SendConstraint;
			oPacketHead.srcAgentId = id();
			oPacketHead.dstAgentId = _uccLocation.agentId;

			PacketSendConstraint oPacketData;
			oPacketData.agreement = _negotiationAgreement;
			oPacketData.constraintItemId = _contributedItemId;

			OutputMemoryStream ostream;
			oPacketHead.Write(ostream);
			oPacketData.Write(ostream);

			sendPacketToAgent(_uccLocation.hostIP, _uccLocation.hostPort, ostream);

			setState(ST_UCP_SENDING_CONSTRAIN);
			destroyChildMCP();
		}

		break;
	}
	default:;
	}
}

void UCP::stop()
{
	// TODO: Destroy search hierarchy below this agent
	destroyChildMCP();
	destroy();
}

void UCP::OnPacketReceived(TCPSocketPtr socket, const PacketHeader &packetHeader, InputMemoryStream &stream)
{
	PacketType packetType = packetHeader.packetType;

	switch (packetType)
	{
		// TODO: Handle packets
	case PacketType::RequestItemResponse:
	{
		if (state() == ST_UCP_REQUESTING_ITEM)
		{
			PacketRequestItemResponse packetData;
			packetData.Read(stream);
			_constraintUCCItemId = packetData.constraintItemId;

			if (_constraintUCCItemId != _contributedItemId)
			{
				if (searchDepth < App->modNodeCluster->MaxDepth())
				{
					createChildMCP(_constraintUCCItemId);
					socket->Disconnect();
					setState(ST_UCP_RESOLVING_CONSTRAINT);
				}
				else
				{
					_negotiationAgreement = false;
					PacketHeader oPacketHead;
					oPacketHead.packetType = PacketType::SendConstraint;
					oPacketHead.srcAgentId = id();
					oPacketHead.dstAgentId = _uccLocation.agentId;

					PacketSendConstraint oPacketData;
					oPacketData.agreement = _negotiationAgreement;
					oPacketData.constraintItemId = NULL_ITEM_ID;

					OutputMemoryStream ostream;
					oPacketHead.Write(ostream);
					oPacketData.Write(ostream);

					socket->SendPacket(ostream.GetBufferPtr(), ostream.GetSize());
					setState(ST_UCP_SENDING_CONSTRAIN);
				}
			}
			else
			{
				_negotiationAgreement = true;

				PacketHeader oPacketHead;
				oPacketHead.packetType = PacketType::SendConstraint;
				oPacketHead.srcAgentId = id();
				oPacketHead.dstAgentId = _uccLocation.agentId;

				PacketSendConstraint oPacketData;
				oPacketData.agreement = _negotiationAgreement;
				oPacketData.constraintItemId = _contributedItemId;

				OutputMemoryStream ostream;
				oPacketHead.Write(ostream);
				oPacketData.Write(ostream);

				socket->SendPacket(ostream.GetBufferPtr(), ostream.GetSize());
				setState(ST_UCP_SENDING_CONSTRAIN);
			}
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::RequestItemResponse was unexpected.";
		}
		break;
	}
	case PacketType::SendConstraintResponse:
	{
		if (state() == ST_UCP_SENDING_CONSTRAIN)
		{
			socket->Disconnect();
			setState(ST_UCP_NEGOTIATION_FINISHED);
		}
		else
		{
			wLog << "OnPacketReceived() - PacketType::SendConstraintResponse was unexpected.";
		}
		break;
	}
	default:
		wLog << "OnPacketReceived() - Unexpected PacketType.";
	}
}

bool UCP::negotiationFinished() const {
	return state() == ST_UCP_NEGOTIATION_FINISHED;
}

bool UCP::negotiationAgreement() const {
	return _negotiationAgreement;
}

void UCP::createChildMCP(uint16_t constraintItemId)
{
	_mcp.reset();
	_mcp = App->agentContainer->createMCP(node(), constraintItemId, _contributedItemId, searchDepth + 1);
}

void UCP::destroyChildMCP()
{
	if (_mcp.get()) {
		_mcp->stop();
		_mcp.reset();
	}
}